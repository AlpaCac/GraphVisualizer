/* ============================================================
 * latency.c —— 与 Python evaluation/latency.py 等价的 CPA 实现
 *
 * 算法两层不动点：
 *
 *   外层 sb_cpa_analyze
 *     ┌── 拷贝当前 delays -> snapshot（只读基准）
 *     │   for each flow, for each (node, phase):
 *     │       new_delay = solve_queueing_delay(...; snapshot)
 *     │       if |new - old| > eps: not converged
 *     │       写入 next_delays
 *     └── delays ← next_delays
 *
 *   内层 solve_queueing_delay
 *     竞争流预计算 -> 时间步长逼近：
 *       t ← base
 *       loop: demand = base + interference(t); if t ≥ demand return t-base; else t = demand
 * ============================================================ */
#include "latency.h"
#include "sb_phys_bind.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define CPA_EPSILON 1e-9
#define CPA_INNER_TIMEOUT_DEFAULT 200000.0
#define CPA_MAX_OUTER_DEFAULT     100
#define CPA_OUTER_EPS_DEFAULT     0.001

/* ============================================================
 * 工具：在 comp_flow 的 routing_path 中查找 node_id 的 path index
 *   找不到返回 -1
 * ============================================================ */
static int path_index_of(const Flow *f, int node_id) {
    for (int k = 0; k < f->path_len; k++)
        if (f->routing_path[k] == node_id) return k;
    return -1;
}

/* ============================================================
 * 节点角色判定（一次性预计算）
 *   规则与 Python 严格一致（互斥 elif）：
 *     flow ∈ node.source_flows -> SOURCE
 *     elif flow ∈ node.sink_flows -> SINK
 *     elif flow ∈ node.relay_flows -> RELAY
 *     else -> NONE
 * ============================================================ */
static int role_of(const BusNode *n, int flow_idx) {
    for (int i = 0; i < n->source_count; i++)
        if (n->source_flows[i] == flow_idx) return CPA_ROLE_SOURCE;
    for (int i = 0; i < n->sink_count; i++)
        if (n->sink_flows[i] == flow_idx) return CPA_ROLE_SINK;
    for (int i = 0; i < n->relay_count; i++)
        if (n->relay_flows[i] == flow_idx) return CPA_ROLE_RELAY;
    return CPA_ROLE_NONE;
}

/* ============================================================
 * 基础处理时间 = message_size / cpu_capacity
 * ============================================================ */
static inline double base_processing_time(const Flow *f, const BusNode *n) {
    double cap = sb_phys_node(n->physical_node_idx)->cpu_capacity;
    if (cap <= 0) return CPA_INF;
    return f->message_size / cap;
}

/* ============================================================
 * ArrivalCurve.max_events
 *   ceil((delta_t + pending_time + jitter - EPSILON) / period)
 * ============================================================ */
static double max_events(double period, double jitter,
                         double delta_t, double pending_time) {
    if (delta_t <= 0) return 0;
    if (pending_time >= CPA_INF || jitter >= CPA_INF) return CPA_INF;
    if (period <= 0) return CPA_INF;
    double v = (delta_t + pending_time + jitter - CPA_EPSILON) / period;
    return ceil(v);
}

/* ============================================================
 * 溯源累积 jitter
 *   对 comp_flow 沿 routing_path 走到 target_node 之前的所有节点，
 *   累加历史排队时延作为到达抖动。
 *   target_node 自身按 phase 规则可能再加一次本节点的 RX。
 *
 *   注意：访问 snapshot 的 delays 时，要用 comp_flow 的 path_index 索引。
 * ============================================================ */
static double accumulate_jitter(const CpaAnalyzer *a,
                                const CpaState *snapshot,
                                int comp_flow_idx,
                                int target_node_id,
                                int phase)
{
    const Flow *comp = &a->topo->flows[comp_flow_idx];
    double jitter = 0.0;

    for (int k = 0; k < comp->path_len; k++) {
        int cur_node_id = comp->routing_path[k];

        if (cur_node_id == target_node_id) {
            /* 到达目标节点：若计算的是 TX 阶段且 comp 在此节点为 relay，
             * 把本节点的 RX 排队也加上（RX 已发生，TX 还未发生） */
            int comp_role_here = snapshot->role[comp_flow_idx][k];
            if (phase == CPA_PH_TX && comp_role_here == CPA_ROLE_RELAY) {
                jitter += snapshot->delays[comp_flow_idx][k][CPA_PH_RX];
            }
            break;
        }

        /* 严格处于 target 之前的节点：累加该节点对 comp 流贡献的全部排队 */
        int comp_role_here = snapshot->role[comp_flow_idx][k];
        if (comp_role_here == CPA_ROLE_SOURCE) {
            jitter += snapshot->delays[comp_flow_idx][k][CPA_PH_TX];
        } else if (comp_role_here == CPA_ROLE_RELAY) {
            jitter += snapshot->delays[comp_flow_idx][k][CPA_PH_RX];
            jitter += snapshot->delays[comp_flow_idx][k][CPA_PH_TX];
        }
        /* SINK / NONE 不贡献历史 jitter */
    }

    return jitter;
}

/* ============================================================
 * 竞争者结构（预计算）
 * ============================================================ */
typedef struct {
    int    flow_idx;
    int    is_self;          /* 是否就是 target flow（== target_flow_idx） */
    double period;
    double curve_jitter;     /* ArrivalCurve.jitter */
    double cost_per_event;   /* base_processing_time at target_node */
    double local_R_bar;      /* queue_delay_snapshot + cost_per_event；self=0 */
} Competitor;

/* ============================================================
 * calculate_interference
 *   按 Python 原始逻辑：
 *     - 自干扰：events-1 个事件，每个耗 cost
 *     - TX：高优先级 += events*cost；低优先级取 max blocking
 *     - RX：FIFO，events 用 EPSILON 窗口（瞬时积压）
 *   Python 用 if/elif (priority > / 否则)，没有同优先级独立分支——
 *   严格相等也归入"低优先级阻塞"。
 * ============================================================ */
static double calculate_interference(int target_flow_idx,
                                     int target_priority,
                                     int phase,
                                     double delta_t,
                                     const Competitor *comp, int comp_count,
                                     const CpaAnalyzer *a)
{
    double total = 0.0;
    double max_lo_blocking = 0.0;

    for (int i = 0; i < comp_count; i++) {
        const Competitor *c = &comp[i];

        if (c->is_self) {
            double e = max_events(c->period, c->curve_jitter, delta_t, c->local_R_bar);
            double self_events = e - 1.0;
            if (self_events < 0) self_events = 0;
            if (self_events >= CPA_INF) return CPA_INF;
            total += self_events * c->cost_per_event;
            continue;
        }

        if (phase == CPA_PH_TX) {
            int comp_prio = a->topo->flows[c->flow_idx].priority;
            if (comp_prio > target_priority) {
                double e = max_events(c->period, c->curve_jitter, delta_t, c->local_R_bar);
                if (e >= CPA_INF) return CPA_INF;
                total += e * c->cost_per_event;
            } else {
                if (c->cost_per_event > max_lo_blocking)
                    max_lo_blocking = c->cost_per_event;
            }
        } else { /* RX */
            double e = max_events(c->period, c->curve_jitter, CPA_EPSILON, c->local_R_bar);
            if (e >= CPA_INF) return CPA_INF;
            total += e * c->cost_per_event;
        }
    }

    if (phase == CPA_PH_TX) total += max_lo_blocking;
    return total;
}

/* ============================================================
 * solve_queueing_delay
 *   target_flow_idx / target_path_idx 确定目标 (流, 节点位置)
 *   phase = RX/TX
 *   snapshot = 只读快照
 * ============================================================ */
static double solve_queueing_delay(const CpaAnalyzer *a,
                                   const CpaState *snapshot,
                                   int target_flow_idx,
                                   int target_path_idx,
                                   int phase)
{
    BusTopology *topo = a->topo;
    Flow *target = &topo->flows[target_flow_idx];
    int target_node_id = target->routing_path[target_path_idx];
    BusNode *node = sb_topology_get_node(topo, target_node_id);
    if (!node) return CPA_INF;

    double base = base_processing_time(target, node);
    if (base >= CPA_INF) return CPA_INF;

    /* 预计算 competitors */
    Competitor comp[SB_MAX_FLOWS_PER_NODE * 2];   /* sink+relay 或 source+relay */
    int cn = 0;

    /* RX: sink_flows + relay_flows
     * TX: source_flows + relay_flows
     * 注意按 Python 顺序拼接（先 sink/source，后 relay） */
    const int *list1; int list1_len;
    const int *list2; int list2_len;
    if (phase == CPA_PH_RX) {
        list1 = node->sink_flows;   list1_len = node->sink_count;
        list2 = node->relay_flows;  list2_len = node->relay_count;
    } else {
        list1 = node->source_flows; list1_len = node->source_count;
        list2 = node->relay_flows;  list2_len = node->relay_count;
    }

    /* 把两段流连续放进 comp[] */
    int merged[SB_MAX_FLOWS_PER_NODE * 2];
    int merged_len = 0;
    for (int i = 0; i < list1_len; i++) merged[merged_len++] = list1[i];
    for (int i = 0; i < list2_len; i++) merged[merged_len++] = list2[i];

    for (int i = 0; i < merged_len; i++) {
        int cf_idx = merged[i];
        Flow *cf = &topo->flows[cf_idx];

        /* 用 target_node_id 在 cf.routing_path 中找位置。
         * 即便找不到（cf 路由失败），也要按 Python .get(default=0.0) 的语义参与计算：
         *   - cost_per_event 仍按 cf.message_size / target_node.cpu 计算
         *   - local_R_bar 里的 queue snapshot 取 0
         *   - accumulate_jitter 在 path 上找不到 target 时也自然返回累加到的值（空 path 返回 0） */
        int cf_path_idx = path_index_of(cf, target_node_id);

        comp[cn].flow_idx = cf_idx;
        comp[cn].is_self  = (cf_idx == target_flow_idx);
        comp[cn].period   = cf->period;
        comp[cn].curve_jitter = accumulate_jitter(a, snapshot, cf_idx,
                                                  target_node_id, phase);
        comp[cn].cost_per_event = base_processing_time(cf, node);

        if (comp[cn].is_self) {
            comp[cn].local_R_bar = 0.0;
        } else {
            /* snapshot 中只在 (cf, k, phase) 对应有效角色才有值；
             * cf_path_idx<0 时 Python 走默认 0.0 */
            double q = (cf_path_idx >= 0)
                       ? snapshot->delays[cf_idx][cf_path_idx][phase]
                       : 0.0;
            comp[cn].local_R_bar = q + comp[cn].cost_per_event;
        }
        cn++;
    }

    /* 时间步长逼近 */
    double t = base;
    int iter_guard = 0;
    while (t < a->inner_timeout) {
        double interference = calculate_interference(
            target_flow_idx, target->priority, phase, t, comp, cn, a);
        if (interference >= CPA_INF) return CPA_INF;

        double demand = base + interference;
        if (t >= demand) return t - base;
        t = demand;

        if (++iter_guard > 1000000) return CPA_INF;
    }
    return CPA_INF;
}

/* ============================================================
 * 初始化：预计算角色 + delays 清零
 * ============================================================ */
void sb_cpa_init(CpaAnalyzer *a, BusTopology *topo) {
    a->topo = topo;
    a->max_outer     = CPA_MAX_OUTER_DEFAULT;
    a->outer_eps     = CPA_OUTER_EPS_DEFAULT;
    a->inner_timeout = CPA_INNER_TIMEOUT_DEFAULT;

    memset(&a->state, 0, sizeof(a->state));
    for (int fi = 0; fi < topo->flow_count; fi++) {
        Flow *f = &topo->flows[fi];
        for (int k = 0; k < f->path_len; k++) {
            BusNode *n = sb_topology_get_node(topo, f->routing_path[k]);
            a->state.role[fi][k] = n ? role_of(n, fi) : CPA_ROLE_NONE;
        }
        for (int k = f->path_len; k < SB_MAX_PATH_HOPS; k++)
            a->state.role[fi][k] = CPA_ROLE_NONE;
    }
}

/* ============================================================
 * 主入口：跑外层不动点
 * ============================================================ */
void sb_cpa_analyze(CpaAnalyzer *a) {
    BusTopology *topo = a->topo;

    /* 外层迭代用的两份拷贝 */
    static CpaState snapshot;   /* 静态：avoid 栈上 ~7KB 的多次拷贝 */
    static CpaState next;

    for (int iter = 0; iter < a->max_outer; iter++) {
        snapshot = a->state;
        next     = a->state;
        int converged = 1;

        for (int fi = 0; fi < topo->flow_count; fi++) {
            Flow *f = &topo->flows[fi];
            for (int k = 0; k < f->path_len; k++) {
                int r = snapshot.role[fi][k];
                if (r == CPA_ROLE_NONE) continue;

                /* phases 列表：source->TX, sink->RX, relay->RX+TX */
                int phases[2]; int np = 0;
                if (r == CPA_ROLE_SOURCE) phases[np++] = CPA_PH_TX;
                else if (r == CPA_ROLE_SINK) phases[np++] = CPA_PH_RX;
                else /* RELAY */ { phases[np++] = CPA_PH_RX; phases[np++] = CPA_PH_TX; }

                for (int p = 0; p < np; p++) {
                    int ph = phases[p];
                    double old_d = snapshot.delays[fi][k][ph];
                    double new_d = solve_queueing_delay(a, &snapshot, fi, k, ph);

                    if (new_d >= CPA_INF) {
                        next.delays[fi][k][ph] = CPA_INF;
                        /* Python 对 inf 也算"未收敛"吗？看代码：inf 时 continue 跳过比较，
                         * 所以不主动置 converged=false。我们也照搬。*/
                        continue;
                    }
                    next.delays[fi][k][ph] = new_d;
                    if (fabs(new_d - old_d) > a->outer_eps) converged = 0;
                }
            }
        }

        a->state = next;
        if (converged) break;
    }
}

/* ============================================================
 * E2E 拼装
 *   total = pre_processing_time
 *         + Σ (base_proc_time + RX_delay + TX_delay 按角色)
 *         + Σ link_delay[u, v, flow]   (= prop_delay + msg/bw)
 *         + post_processing_time
 *   任意阶段 INF -> 整流 INF
 * ============================================================ */
double sb_cpa_e2e_of(const CpaAnalyzer *a, int flow_idx) {
    const BusTopology *topo = a->topo;
    const Flow *f = &topo->flows[flow_idx];
    const CpaState *S = &a->state;

    /* 1) 检查溢出 */
    for (int k = 0; k < f->path_len; k++) {
        int r = S->role[flow_idx][k];
        if (r == CPA_ROLE_SOURCE && S->delays[flow_idx][k][CPA_PH_TX] >= CPA_INF) return CPA_INF;
        if (r == CPA_ROLE_SINK   && S->delays[flow_idx][k][CPA_PH_RX] >= CPA_INF) return CPA_INF;
        if (r == CPA_ROLE_RELAY) {
            if (S->delays[flow_idx][k][CPA_PH_RX] >= CPA_INF) return CPA_INF;
            if (S->delays[flow_idx][k][CPA_PH_TX] >= CPA_INF) return CPA_INF;
        }
    }

    double total = f->pre_processing_time;

    for (int k = 0; k < f->path_len; k++) {
        BusNode *n = sb_topology_get_node((BusTopology*)topo, f->routing_path[k]);
        if (!n) return CPA_INF;
        double base = base_processing_time(f, n);
        if (base >= CPA_INF) return CPA_INF;

        int r = S->role[flow_idx][k];
        if (r == CPA_ROLE_SOURCE) {
            total += base + S->delays[flow_idx][k][CPA_PH_TX];
        } else if (r == CPA_ROLE_SINK) {
            total += base + S->delays[flow_idx][k][CPA_PH_RX];
        } else if (r == CPA_ROLE_RELAY) {
            total += base
                   + S->delays[flow_idx][k][CPA_PH_RX]
                   + S->delays[flow_idx][k][CPA_PH_TX];
        }

        /* 到下一跳的链路时延（= prop_delay + msg / bw） */
        if (k < f->path_len - 1) {
            BusLink *l = sb_topology_get_link((BusTopology*)topo,
                                              f->routing_path[k],
                                              f->routing_path[k+1]);
            if (l) {
                const PhysicalLink *pl = sb_phys_link(l->physical_link_idx);
                double trans = (pl->bandwidth > 0) ? f->message_size / pl->bandwidth : 0.0;
                total += pl->propagation_delay + trans;
            }
            /* 若 link 找不到（不应发生），按 Python 行为 fallback 加 0 */
        }
    }

    total += f->post_processing_time;
    return total;
}

void sb_cpa_write_back(CpaAnalyzer *a) {
    BusTopology *topo = a->topo;
    for (int fi = 0; fi < topo->flow_count; fi++) {
        Flow *f = &topo->flows[fi];
        double e2e = sb_cpa_e2e_of(a, fi);
        f->worst_case_delay = e2e;
        /* 覆盖式赋值，对齐 Python ga_core */
        f->is_schedulable = (e2e <= f->deadline);
    }
}
