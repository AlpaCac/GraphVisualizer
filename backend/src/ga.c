/* ============================================================
 * ga.c —— NSGA-II 主循环
 * ============================================================ */
#include "ga.h"
#include "sb_phys_bind.h"
#include "cost.h"
#include "reliability.h"
#include "connectivity.h"
#include "latency.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 随机数：xoshiro256** —— 周期 2^256-1，64位输出，无外部依赖
 * 同种子可重现实验。比 LCG 质量好得多，足够 GA 使用。
 * ============================================================ */
static uint64_t g_rng_state[4] = {
    0xdeadbeefcafebabeULL, 0x0123456789abcdefULL,
    0xfedcba9876543210ULL, 0x1357924680bdfaceULL
};

static inline uint64_t rotl64(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

static uint64_t rng_next64(void) {
    const uint64_t result = rotl64(g_rng_state[1] * 5, 7) * 9;
    const uint64_t t = g_rng_state[1] << 17;
    g_rng_state[2] ^= g_rng_state[0];
    g_rng_state[3] ^= g_rng_state[1];
    g_rng_state[1] ^= g_rng_state[2];
    g_rng_state[0] ^= g_rng_state[3];
    g_rng_state[2] ^= t;
    g_rng_state[3] = rotl64(g_rng_state[3], 45);
    return result;
}

void ga_seed_rng(uint64_t seed) {
    /* 用 splitmix64 把单一种子展开到 256 位 */
    uint64_t z = seed;
    for (int i = 0; i < 4; i++) {
        z += 0x9e3779b97f4a7c15ULL;
        uint64_t x = z;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        x = x ^ (x >> 31);
        g_rng_state[i] = x;
    }
    /* 至少要有一位非零 */
    if ((g_rng_state[0] | g_rng_state[1] | g_rng_state[2] | g_rng_state[3]) == 0)
        g_rng_state[0] = 1;
}

double ga_rand_uniform(void) {
    /* 53 位精度浮点 */
    return (rng_next64() >> 11) * (1.0 / 9007199254740992.0);
}

int ga_rand_int(int n) {
    if (n <= 1) return 0;
    return (int)(ga_rand_uniform() * n);
}

/* ============================================================
 * 个体初始化
 * ============================================================ */
void ga_individual_init_empty(Individual *ind, int n_roles, int n_links) {
    memset(ind, 0, sizeof(*ind));
    ind->n_roles = n_roles;
    ind->n_links = n_links;
    for (int i = 0; i < GA_OBJ_COUNT; i++) ind->fitness[i] = INFINITY;
    ind->rank = 0;
    ind->crowding_distance = 0.0;
}

/* ============================================================
 * 解码：基因 -> BusTopology
 *   与 Python decode_to_topology 等价
 *
 * 关键点：
 *   - role_gene[i] == -1 的物理节点跳过（损毁）
 *   - 流的源/目的若不在拓扑里，丢弃该流
 *   - link_gene[j] == -1 跳过；==1 才激活；两端节点必须存活
 *   - sb_topology_add_flow 会自动挂 source/sink_flows（与 ga_core 一致）
 * ============================================================ */
int ga_decode_to_topology(const Individual *ind, const Sandbox *sb,
                          BusTopology *out)
{
    sb_topology_init(out);

    /* 1. 节点（按存活与角色） */
    for (int i = 0; i < sb->p_node_count; i++) {
        if (ind->role_gene[i] == -1) continue;
        BusNode bn;
        sb_bus_node_init(&bn);
        bn.id = sb->p_nodes[i].id;
        bn.physical_node_idx = i;
        bn.is_core = (ind->role_gene[i] == 1) ? 1 : 0;
        sb_topology_add_node(out, &bn);
    }

    /* 2. 流（端点必须存活） */
    for (int i = 0; i < sb->flow_graph.count; i++) {
        const Flow *gf = &sb->flow_graph.flows[i];
        if (!sb_topology_get_node(out, gf->source_node_id)) continue;
        if (!sb_topology_get_node(out, gf->target_node_id)) continue;
        Flow lf = *gf;            /* 按值复制，避免污染模板 */
        lf.path_len = 0;
        lf.worst_case_delay = -1.0;
        lf.actual_reliability = -1.0;
        lf.is_schedulable = 1;
        sb_topology_add_flow(out, &lf);
    }

    /* 3. 链路（基因激活 + 两端存活） */
    for (int j = 0; j < sb->c_link_count; j++) {
        if (ind->link_gene[j] != 1) continue;
        const PhysicalLink *pl = &sb->c_links[j];
        if (!sb_topology_get_node(out, pl->node_a_id)) continue;
        if (!sb_topology_get_node(out, pl->node_b_id)) continue;
        BusLink bl;
        sb_bus_link_init(&bl);
        bl.id = pl->id;
        bl.node_a_id = pl->node_a_id;
        bl.node_b_id = pl->node_b_id;
        bl.physical_link_idx = j;
        sb_topology_add_link(out, &bl);
    }
    return 0;
}

/* ============================================================
 * 验证：6 条硬约束
 *
 *   1. 核心节点数 ∈ [4, 7]
 *   2. 每节点链路数 ≤ max_physical_ports
 *   3. 每个 edge 节点连 1~2 个核心
 *   4. edge-edge 链路总数 ≤ 4
 *   5. 全图连通（BFS）
 *   6. 核心子图连通（BFS）
 * ============================================================ */
int ga_validate_topology(const BusTopology *topo, const Sandbox *sb) {
    int N = topo->node_count;
    if (N == 0) return 0;

    /* 1. 核心数 */
    int core_count = 0;
    for (int i = 0; i < N; i++) if (topo->nodes[i].is_core) core_count++;
    if (core_count < 4 || core_count > 7) return 0;

    int edge_edge_links = 0;

    for (int i = 0; i < N; i++) {
        const BusNode *n = &topo->nodes[i];

        /* 2. 端口数 */
        int max_ports = sb->p_nodes[n->physical_node_idx].max_physical_ports;
        if (n->link_count > max_ports) return 0;

        if (n->is_core) continue;

        /* 3. edge 节点的核心邻居数；同时计 edge-edge 链路 */
        int core_neighbors = 0;
        for (int k = 0; k < n->link_count; k++) {
            const BusLink *l = &topo->links[n->link_idx[k]];
            int peer_id = (l->node_a_id == n->id) ? l->node_b_id : l->node_a_id;
            const BusNode *peer = NULL;
            for (int q = 0; q < N; q++)
                if (topo->nodes[q].id == peer_id) { peer = &topo->nodes[q]; break; }
            if (!peer) continue;
            if (peer->is_core) {
                core_neighbors++;
            } else if (l->node_a_id < l->node_b_id) {
                /* 避免无向边重复计数 */
                edge_edge_links++;
            }
        }
        if (core_neighbors == 0 || core_neighbors > 2) return 0;
    }

    /* 4. edge-edge 上限 */
    if (edge_edge_links > 4) return 0;

    /* 5. 全图连通：从第一个节点 BFS */
    {
        int visited[SB_MAX_NODES] = {0};
        int queue[SB_MAX_NODES];
        int qh = 0, qt = 0;
        queue[qt++] = 0;
        visited[0] = 1;
        int cnt = 1;
        while (qh < qt) {
            const BusNode *u = &topo->nodes[queue[qh++]];
            for (int k = 0; k < u->link_count; k++) {
                const BusLink *l = &topo->links[u->link_idx[k]];
                int peer_id = (l->node_a_id == u->id) ? l->node_b_id : l->node_a_id;
                /* 找 peer 在 nodes[] 中的下标 */
                for (int q = 0; q < N; q++) {
                    if (topo->nodes[q].id == peer_id && !visited[q]) {
                        visited[q] = 1;
                        queue[qt++] = q;
                        cnt++;
                        break;
                    }
                }
            }
        }
        if (cnt != N) return 0;
    }

    /* 6. 核心子图连通 */
    if (core_count == 0) return 0;
    {
        int first_core = -1;
        for (int i = 0; i < N; i++) if (topo->nodes[i].is_core) { first_core = i; break; }

        int visited[SB_MAX_NODES] = {0};
        int queue[SB_MAX_NODES];
        int qh = 0, qt = 0;
        queue[qt++] = first_core;
        visited[first_core] = 1;
        int cnt = 1;
        while (qh < qt) {
            const BusNode *u = &topo->nodes[queue[qh++]];
            for (int k = 0; k < u->link_count; k++) {
                const BusLink *l = &topo->links[u->link_idx[k]];
                int peer_id = (l->node_a_id == u->id) ? l->node_b_id : l->node_a_id;
                for (int q = 0; q < N; q++) {
                    if (topo->nodes[q].id == peer_id && topo->nodes[q].is_core && !visited[q]) {
                        visited[q] = 1;
                        queue[qt++] = q;
                        cnt++;
                        break;
                    }
                }
            }
        }
        if (cnt != core_count) return 0;
    }
    return 1;
}

/* ============================================================
 * Baseline seed —— MST 三步法
 *   1. 指定 5 个核心：{4, 7, 11, 14, 15}
 *   2. 用 Prim 把核心连通（选最短传播时延边）
 *   3. 每个边缘节点接入最近核心（无直连则接最近 edge）
 *   4. 加 1 条核心间冗余（端口数允许下）
 * ============================================================ */
static int is_in_set(const int *set, int n, int x) {
    for (int i = 0; i < n; i++) if (set[i] == x) return 1;
    return 0;
}

/* 找节点 u 与 v 之间的候选链路索引，找不到返回 -1 */
static int find_candidate_link(const Sandbox *sb, int a, int b) {
    for (int i = 0; i < sb->c_link_count; i++) {
        const PhysicalLink *l = &sb->c_links[i];
        if ((l->node_a_id == a && l->node_b_id == b) ||
            (l->node_a_id == b && l->node_b_id == a)) return i;
    }
    return -1;
}

void ga_create_baseline_seed(Individual *seed, const Sandbox *sb) {
    ga_individual_init_empty(seed, sb->p_node_count, sb->c_link_count);

    static const int CORE_IDS[] = {4, 7, 11, 14, 15};
    const int N_CORE = sizeof(CORE_IDS) / sizeof(int);

    for (int i = 0; i < N_CORE; i++) seed->role_gene[CORE_IDS[i]] = 1;

    /* Step 1: Prim MST 连接核心 */
    int visited[SB_MAX_NODES] = {0};
    int vis_count = 1;
    visited[CORE_IDS[0]] = 1;

    while (vis_count < N_CORE) {
        int best_link_idx = -1;
        double min_delay = INFINITY;
        int next_core = -1;

        for (int i = 0; i < N_CORE; i++) {
            int u = CORE_IDS[i];
            if (!visited[u]) continue;
            for (int j = 0; j < N_CORE; j++) {
                int v = CORE_IDS[j];
                if (visited[v]) continue;
                int li = find_candidate_link(sb, u, v);
                if (li < 0) continue;
                if (sb->c_links[li].propagation_delay < min_delay) {
                    min_delay = sb->c_links[li].propagation_delay;
                    best_link_idx = li;
                    next_core = v;
                }
            }
        }
        if (best_link_idx < 0) {
            fprintf(stderr, "[ga seed] core ids cannot be connected\n");
            return;
        }
        seed->link_gene[best_link_idx] = 1;
        visited[next_core] = 1;
        vis_count++;
    }

    /* Step 2: 每个 edge 节点接最近核心；无直连核心则接最近 edge */
    for (int u = 0; u < sb->p_node_count; u++) {
        if (is_in_set(CORE_IDS, N_CORE, u)) continue;
        int best_idx = -1;
        double min_delay = INFINITY;

        /* 先找最近核心 */
        for (int i = 0; i < N_CORE; i++) {
            int v = CORE_IDS[i];
            int li = find_candidate_link(sb, u, v);
            if (li < 0) continue;
            if (sb->c_links[li].propagation_delay < min_delay) {
                min_delay = sb->c_links[li].propagation_delay;
                best_idx = li;
            }
        }
        /* 备选：最近任意节点 */
        if (best_idx < 0) {
            for (int v = 0; v < sb->p_node_count; v++) {
                if (v == u) continue;
                int li = find_candidate_link(sb, u, v);
                if (li < 0) continue;
                if (sb->c_links[li].propagation_delay < min_delay) {
                    min_delay = sb->c_links[li].propagation_delay;
                    best_idx = li;
                }
            }
        }
        if (best_idx >= 0) seed->link_gene[best_idx] = 1;
    }

    /* Step 3: 一条核心间冗余链路（按时延升序的第一条且端口足） */
    int added = 0;
    int MAX_RED = 1;

    /* 收集候选冗余边 (idx, u, v, delay)，按 delay 升序 */
    typedef struct { int li; int u; int v; double d; } Cand;
    Cand cands[SB_MAX_LINKS];
    int  nc = 0;
    for (int i = 0; i < N_CORE; i++) {
        int u = CORE_IDS[i];
        for (int j = i+1; j < N_CORE; j++) {
            int v = CORE_IDS[j];
            int li = find_candidate_link(sb, u, v);
            if (li < 0) continue;
            if (seed->link_gene[li] != 0) continue;
            cands[nc].li = li; cands[nc].u = u; cands[nc].v = v;
            cands[nc].d = sb->c_links[li].propagation_delay;
            nc++;
        }
    }
    /* 简单插入排序 */
    for (int i = 1; i < nc; i++) {
        Cand k = cands[i]; int j = i - 1;
        while (j >= 0 && cands[j].d > k.d) { cands[j+1] = cands[j]; j--; }
        cands[j+1] = k;
    }

    for (int i = 0; i < nc && added < MAX_RED; i++) {
        int u = cands[i].u, v = cands[i].v;
        /* 统计两端当前活跃端口数 */
        int u_used = 0, v_used = 0;
        for (int k = 0; k < sb->c_link_count; k++) {
            if (seed->link_gene[k] != 1) continue;
            if (sb->c_links[k].node_a_id == u || sb->c_links[k].node_b_id == u) u_used++;
            if (sb->c_links[k].node_a_id == v || sb->c_links[k].node_b_id == v) v_used++;
        }
        if (u_used < sb->p_nodes[u].max_physical_ports &&
            v_used < sb->p_nodes[v].max_physical_ports) {
            seed->link_gene[cands[i].li] = 1;
            added++;
        }
    }
}

/* ============================================================
 * Evaluate Individual
 *   - decode + route
 *   - 任一流无路径 -> fitness = (inf,...) [5 维]
 *   - 否则计算 5 项指标：comp_lat / C_conn_norm / E_throughput / comp_rel / cost
 * ============================================================ */
void ga_evaluate(Individual *ind, GaContext *ctx) {
    static BusTopology topo;
    ga_decode_to_topology(ind, ctx->sb, &topo);
    sb_router_route_all(&ctx->router, &topo);

    /* 检查不可达 */
    for (int i = 0; i < topo.flow_count; i++) {
        if (topo.flows[i].path_len == 0) {
            for (int d = 0; d < GA_OBJ_COUNT; d++) ind->fitness[d] = INFINITY;
            ind->m_comp_lat = INFINITY;
            ind->m_max_lat  = INFINITY;
            ind->m_fiedler  = 0.0;
            ind->m_conn_norm = 0.0;
            ind->m_throughput = 0.0;
            ind->m_min_satisfaction = 0.0;
            ind->m_comp_rel = 0.0;
            ind->m_cost     = INFINITY;
            ind->is_fully_schedulable = 0;
            ind->evaluated = 1;
            return;
        }
    }

    /* CPA 时延 */
    static CpaAnalyzer cpa;
    sb_cpa_init(&cpa, &topo);
    sb_cpa_analyze(&cpa);
    sb_cpa_write_back(&cpa);

    /* Fiedler 与归一化连通度 */
    double fiedler = sb_connectivity_evaluate(&topo);
    double conn_norm = (ctx->lambda_base > 0)
                     ? fiedler / (ctx->lambda_base + 1e-10)
                     : fiedler;
    if (conn_norm < 0) conn_norm = 0;

    /* 可靠性（写回 flow.actual_reliability + is_schedulable AND累积） */
    sb_reliability_evaluate_all(&topo);

    /* 吞吐量（写回 flow.effective_throughput） */
    static ThroughputResult tput;
    sb_throughput_evaluate(&topo, &ctx->mac, &tput);

    /* 成本 */
    double cost = sb_cost_evaluate(&topo);

    /* 加权 comp_lat / comp_rel */
    double wl_sum = 0.0, wl_w = 0.0;
    double wr_sum = 0.0, wr_w = 0.0;
    double max_lat = 0.0;

    for (int i = 0; i < topo.flow_count; i++) {
        const Flow *f = &topo.flows[i];
        double prio = (double)f->priority;
        double ratio = (f->deadline > 0) ? f->worst_case_delay / f->deadline
                                         : INFINITY;
        wl_sum += prio * ratio;
        wl_w   += prio;
        wr_sum += prio * f->actual_reliability;
        wr_w   += prio;
        if (f->worst_case_delay > max_lat) max_lat = f->worst_case_delay;
    }

    double comp_lat = (wl_w > 0) ? wl_sum / wl_w : INFINITY;
    double comp_rel = (wr_w > 0) ? wr_sum / wr_w : 0.0;

    ind->m_comp_lat = comp_lat;
    ind->m_max_lat  = max_lat;
    ind->m_fiedler  = fiedler;
    ind->m_conn_norm = conn_norm;
    ind->m_throughput = tput.system_weighted_throughput;
    ind->m_min_satisfaction = tput.system_min_satisfaction;
    ind->m_comp_rel = comp_rel;
    ind->m_cost     = cost;

    /* 全可调度 = 所有 flow.is_schedulable */
    int all_ok = 1;
    for (int i = 0; i < topo.flow_count; i++) {
        if (!topo.flows[i].is_schedulable) { all_ok = 0; break; }
    }
    ind->is_fully_schedulable = all_ok;

    /* 5 维 fitness（全部最小化）：
     *   0: comp_lat            加权时延比，越小越好
     *   1: -C_conn_norm        归一化连通度，最大化 → 取反
     *   2: -E_throughput       系统级加权吞吐，最大化 → 取反
     *   3: -comp_rel           加权可靠性，最大化 → 取反
     *   4: cost                组网成本，越小越好
     */
    ind->fitness[0] = comp_lat;
    ind->fitness[1] = -conn_norm;
    ind->fitness[2] = -tput.system_weighted_throughput;
    ind->fitness[3] = -comp_rel;
    ind->fitness[4] = cost;
    ind->evaluated = 1;
}

/* ============================================================
 * Dominance
 *   先 fully_schedulable 优先，再向量比较（所有维度 ≤ 且至少一维 <）
 * ============================================================ */
int ga_dominates(const Individual *p, const Individual *q) {
    if (p->is_fully_schedulable && !q->is_fully_schedulable) return 1;
    if (!p->is_fully_schedulable && q->is_fully_schedulable) return 0;

    int strictly_less = 0;
    for (int i = 0; i < GA_OBJ_COUNT; i++) {
        if (p->fitness[i] > q->fitness[i]) return 0;
        if (p->fitness[i] < q->fitness[i]) strictly_less = 1;
    }
    return strictly_less;
}

/* ============================================================
 * Fast non-dominated sort
 *   写入 ind.rank（1 是最优层）。pop 内部顺序保持不变。
 * ============================================================ */
void ga_fast_non_dominated_sort(Population *pop) {
    int n = pop->count;
    static int dominated_count[GA_POP_SIZE * 4];
    static int dominator_set[GA_POP_SIZE * 4][GA_POP_SIZE * 4];
    static int dominator_sz [GA_POP_SIZE * 4];
    static int current_front[GA_POP_SIZE * 4];
    static int next_front   [GA_POP_SIZE * 4];

    for (int i = 0; i < n; i++) {
        dominated_count[i] = 0;
        dominator_sz[i] = 0;
    }

    /* O(N^2) 比较 */
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            if (ga_dominates(&pop->items[i], &pop->items[j])) {
                dominator_set[i][dominator_sz[i]++] = j;
            } else if (ga_dominates(&pop->items[j], &pop->items[i])) {
                dominated_count[i]++;
            }
        }
    }

    int front_size = 0;
    for (int i = 0; i < n; i++) {
        if (dominated_count[i] == 0) {
            pop->items[i].rank = 1;
            current_front[front_size++] = i;
        }
    }

    int rank = 1;
    while (front_size > 0) {
        int next_size = 0;
        for (int k = 0; k < front_size; k++) {
            int p_idx = current_front[k];
            for (int m = 0; m < dominator_sz[p_idx]; m++) {
                int q_idx = dominator_set[p_idx][m];
                if (--dominated_count[q_idx] == 0) {
                    pop->items[q_idx].rank = rank + 1;
                    next_front[next_size++] = q_idx;
                }
            }
        }
        rank++;
        front_size = next_size;
        memcpy(current_front, next_front, sizeof(int) * next_size);
    }
}

/* ============================================================
 * Crowding distance
 *   对某一 rank 的所有个体计算拥挤度
 * ============================================================ */
static int sort_indices_by_dim;
static const Population *sort_pop_ref;

static int cmp_fitness_dim(const void *a, const void *b) {
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    double va = sort_pop_ref->items[ia].fitness[sort_indices_by_dim];
    double vb = sort_pop_ref->items[ib].fitness[sort_indices_by_dim];
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

void ga_calc_crowding_for_rank(Population *pop, int target_rank) {
    int idx_buf[GA_POP_SIZE * 4];
    int nf = 0;
    for (int i = 0; i < pop->count; i++) {
        if (pop->items[i].rank == target_rank) idx_buf[nf++] = i;
    }
    if (nf == 0) return;

    if (nf <= 2) {
        for (int i = 0; i < nf; i++)
            pop->items[idx_buf[i]].crowding_distance = INFINITY;
        return;
    }

    for (int i = 0; i < nf; i++)
        pop->items[idx_buf[i]].crowding_distance = 0.0;

    sort_pop_ref = pop;
    for (int m = 0; m < GA_OBJ_COUNT; m++) {
        sort_indices_by_dim = m;
        qsort(idx_buf, nf, sizeof(int), cmp_fitness_dim);

        pop->items[idx_buf[0]].crowding_distance      = INFINITY;
        pop->items[idx_buf[nf-1]].crowding_distance   = INFINITY;

        double fmin = pop->items[idx_buf[0]].fitness[m];
        double fmax = pop->items[idx_buf[nf-1]].fitness[m];
        if (fmax == fmin) continue;

        for (int i = 1; i < nf - 1; i++) {
            if (pop->items[idx_buf[i]].crowding_distance == INFINITY) continue;
            double d = (pop->items[idx_buf[i+1]].fitness[m] -
                        pop->items[idx_buf[i-1]].fitness[m]) / (fmax - fmin);
            pop->items[idx_buf[i]].crowding_distance += d;
        }
    }
}

/* ============================================================
 * Tournament Selection (size=2)
 *   Rank 低者优先；Rank 同则 crowding 大者优先
 * ============================================================ */
Individual *ga_tournament_select(Population *pop, int tournament_size) {
    int best_idx = ga_rand_int(pop->count);
    for (int t = 1; t < tournament_size; t++) {
        int chal_idx = ga_rand_int(pop->count);
        Individual *best = &pop->items[best_idx];
        Individual *chal = &pop->items[chal_idx];
        if (chal->rank < best->rank) best_idx = chal_idx;
        else if (chal->rank == best->rank &&
                 chal->crowding_distance > best->crowding_distance)
            best_idx = chal_idx;
    }
    return &pop->items[best_idx];
}

/* ============================================================
 * Crossover：均匀，对相同基因位保留，不同位 50/50 互换
 * ============================================================ */
void ga_crossover(const Individual *p1, const Individual *p2,
                  Individual *c1, Individual *c2)
{
    ga_individual_init_empty(c1, p1->n_roles, p1->n_links);
    ga_individual_init_empty(c2, p2->n_roles, p2->n_links);

    for (int i = 0; i < p1->n_roles; i++) {
        int g1 = p1->role_gene[i], g2 = p2->role_gene[i];
        if (g1 == g2) { c1->role_gene[i] = g1; c2->role_gene[i] = g2; }
        else if (ga_rand_uniform() < 0.5) {
            c1->role_gene[i] = g1; c2->role_gene[i] = g2;
        } else {
            c1->role_gene[i] = g2; c2->role_gene[i] = g1;
        }
    }
    for (int i = 0; i < p1->n_links; i++) {
        int g1 = p1->link_gene[i], g2 = p2->link_gene[i];
        if (g1 == g2) { c1->link_gene[i] = g1; c2->link_gene[i] = g2; }
        else if (ga_rand_uniform() < 0.5) {
            c1->link_gene[i] = g1; c2->link_gene[i] = g2;
        } else {
            c1->link_gene[i] = g2; c2->link_gene[i] = g1;
        }
    }
}

/* ============================================================
 * Mutation：位翻转（跳过 -1）
 * ============================================================ */
void ga_mutate(Individual *ind, double rate) {
    for (int i = 0; i < ind->n_roles; i++) {
        if (ind->role_gene[i] == -1) continue;
        if (ga_rand_uniform() < rate)
            ind->role_gene[i] = 1 - ind->role_gene[i];
    }
    for (int i = 0; i < ind->n_links; i++) {
        if (ind->link_gene[i] == -1) continue;
        if (ga_rand_uniform() < rate)
            ind->link_gene[i] = 1 - ind->link_gene[i];
    }
}

/* ============================================================
 * Generate single immigrant: 基于 seed 做 10% 变异直到合法
 * ============================================================ */
static int gen_immigrant(const Individual *seed, GaContext *ctx,
                         Individual *out) {
    static BusTopology topo;
    for (int attempt = 0; attempt < 10000; attempt++) {
        *out = *seed;
        ga_mutate(out, 0.1);
        ga_decode_to_topology(out, ctx->sb, &topo);
        if (ga_validate_topology(&topo, ctx->sb)) return 1;
    }
    return 0;
}

/* ============================================================
 * Generate offspring population
 *   每对父代尝试最多 50 次生成 2 个合法子代；失败则注入 immigrant
 * ============================================================ */
void ga_generate_offspring(Population *parents, const Individual *seed,
                           GaContext *ctx, Population *offspring,
                           int pop_size)
{
    offspring->count = 0;
    int immigrant_cnt = 0;
    static BusTopology topo;

    while (offspring->count < pop_size) {
        Individual *p1 = ga_tournament_select(parents, 2);
        Individual *p2 = ga_tournament_select(parents, 2);

        int valid_found = 0;
        int max_retries = 50;
        for (int attempt = 0; attempt < max_retries && valid_found < 2; attempt++) {
            Individual c1, c2;
            ga_crossover(p1, p2, &c1, &c2);
            ga_mutate(&c1, 0.01);
            ga_mutate(&c2, 0.01);

            if (valid_found < 2 && offspring->count < pop_size) {
                ga_decode_to_topology(&c1, ctx->sb, &topo);
                if (ga_validate_topology(&topo, ctx->sb)) {
                    offspring->items[offspring->count++] = c1;
                    valid_found++;
                }
            }
            if (valid_found < 2 && offspring->count < pop_size) {
                ga_decode_to_topology(&c2, ctx->sb, &topo);
                if (ga_validate_topology(&topo, ctx->sb)) {
                    offspring->items[offspring->count++] = c2;
                    valid_found++;
                }
            }
        }

        /* 兜底：注入 immigrant */
        while (valid_found < 2 && offspring->count < pop_size) {
            Individual imm;
            if (gen_immigrant(seed, ctx, &imm)) {
                offspring->items[offspring->count++] = imm;
                immigrant_cnt++;
            }
            valid_found++;
        }
    }
    if (ctx->verbose) {
        printf("  [offspring] count=%d, immigrants=%d\n",
               offspring->count, immigrant_cnt);
    }
}

/* ============================================================
 * Environmental Selection
 *   1. 合并父代 + 子代
 *   2. 按基因序列去重
 *   3. 非支配排序
 *   4. 按 rank 累加，最后一层用 crowding 截断
 * ============================================================ */
static int hash_individual(const Individual *ind, char *buf, int cap) {
    int p = 0;
    for (int i = 0; i < ind->n_roles && p < cap - 2; i++)
        p += snprintf(buf + p, cap - p, "%d", ind->role_gene[i] + 1);
    if (p < cap - 1) buf[p++] = '|';
    for (int i = 0; i < ind->n_links && p < cap - 2; i++)
        p += snprintf(buf + p, cap - p, "%d", ind->link_gene[i] + 1);
    buf[p] = '\0';
    return p;
}

void ga_environmental_select(Population *parents, Population *offspring,
                             Population *next_gen, int pop_size)
{
    static Population merged;
    merged.count = 0;
    /* 1. 合并 */
    for (int i = 0; i < parents->count; i++)
        merged.items[merged.count++] = parents->items[i];
    for (int i = 0; i < offspring->count; i++)
        merged.items[merged.count++] = offspring->items[i];

    /* 2. 去重：用 O(N^2) 比较基因——N=200 完全 OK */
    static Population unique_pop;
    unique_pop.count = 0;
    for (int i = 0; i < merged.count; i++) {
        int dup = 0;
        for (int j = 0; j < unique_pop.count; j++) {
            if (memcmp(unique_pop.items[j].role_gene, merged.items[i].role_gene,
                       sizeof(int) * merged.items[i].n_roles) == 0 &&
                memcmp(unique_pop.items[j].link_gene, merged.items[i].link_gene,
                       sizeof(int) * merged.items[i].n_links) == 0) {
                dup = 1; break;
            }
        }
        if (!dup) unique_pop.items[unique_pop.count++] = merged.items[i];
    }

    /* 3. 非支配排序 */
    ga_fast_non_dominated_sort(&unique_pop);

    /* 4. 按 rank 累加 */
    next_gen->count = 0;
    int cur_rank = 1;
    while (next_gen->count < pop_size) {
        /* 收集 cur_rank 这一层 */
        int rank_indices[GA_POP_SIZE * 4]; int nrank = 0;
        for (int i = 0; i < unique_pop.count; i++)
            if (unique_pop.items[i].rank == cur_rank) rank_indices[nrank++] = i;
        if (nrank == 0) break;

        ga_calc_crowding_for_rank(&unique_pop, cur_rank);

        if (next_gen->count + nrank <= pop_size) {
            for (int i = 0; i < nrank; i++)
                next_gen->items[next_gen->count++] = unique_pop.items[rank_indices[i]];
        } else {
            /* 按 crowding 降序选剩余空位 */
            int slot = pop_size - next_gen->count;
            /* 用 qsort by crowding desc */
            for (int i = 0; i < nrank - 1; i++) {
                for (int j = 0; j < nrank - 1 - i; j++) {
                    double da = unique_pop.items[rank_indices[j]].crowding_distance;
                    double db = unique_pop.items[rank_indices[j+1]].crowding_distance;
                    if (da < db) {
                        int t = rank_indices[j];
                        rank_indices[j] = rank_indices[j+1];
                        rank_indices[j+1] = t;
                    }
                }
            }
            for (int i = 0; i < slot; i++)
                next_gen->items[next_gen->count++] = unique_pop.items[rank_indices[i]];
            break;
        }
        cur_rank++;
    }

    /* 兜底：若不足 pop_size，从 rank 1 克隆 */
    if (next_gen->count < pop_size) {
        int rank1_indices[GA_POP_SIZE * 4]; int n1 = 0;
        for (int i = 0; i < unique_pop.count; i++)
            if (unique_pop.items[i].rank == 1) rank1_indices[n1++] = i;
        if (n1 == 0) return;
        while (next_gen->count < pop_size) {
            int pick = rank1_indices[ga_rand_int(n1)];
            next_gen->items[next_gen->count++] = unique_pop.items[pick];
        }
    }
}

/* ============================================================
 * Main loop —— 通用版
 *   给定一个种子（可能是 baseline，也可能是 damaged），
 *   初始化种群并演化 max_gen 代。
 * ============================================================ */
void ga_run_from_seed(GaContext *ctx, const Individual *seed_in,
                      int max_gen, Population *pop_out)
{
    sb_phys_bind(ctx->sb->p_nodes, ctx->sb->c_links);

    /* 1. seed（外部传入，已可能含 -1 故障基因） */
    Individual seed = *seed_in;
    ga_evaluate(&seed, ctx);
    if (ctx->verbose) {
        printf("[seed] sched=%d lat=%.4f C_conn=%.4f thpt=%.2f rel=%.4f cost=%.2f\n",
               seed.is_fully_schedulable, seed.m_comp_lat, seed.m_conn_norm,
               seed.m_throughput, seed.m_comp_rel, seed.m_cost);
    }

    /* 2. 初始化种群（种子 + 10% 变异）*/
    static Population pop;
    pop.count = 0;
    pop.items[pop.count++] = seed;

    int attempts = 0;
    static BusTopology topo;
    while (pop.count < GA_POP_SIZE) {
        attempts++;
        Individual cand = seed;
        ga_mutate(&cand, 0.1);
        ga_decode_to_topology(&cand, ctx->sb, &topo);
        if (ga_validate_topology(&topo, ctx->sb)) {
            pop.items[pop.count++] = cand;
        }
        if (attempts > 10000000) {
            fprintf(stderr, "init population stuck\n");
            break;
        }
    }
    if (ctx->verbose) printf("[init pop] %d valid in %d attempts\n", pop.count, attempts);

    for (int i = 0; i < pop.count; i++) ga_evaluate(&pop.items[i], ctx);

    ga_fast_non_dominated_sort(&pop);
    for (int r = 1; r <= GA_POP_SIZE; r++) ga_calc_crowding_for_rank(&pop, r);

    /* 3. 演化 */
    static Population offspring, next_gen;
    for (int gen = 0; gen < max_gen; gen++) {
        if (ctx->verbose) printf("\n=== Gen %d/%d ===\n", gen+1, max_gen);

        ga_generate_offspring(&pop, &seed, ctx, &offspring, GA_POP_SIZE);

        for (int i = 0; i < offspring.count; i++) ga_evaluate(&offspring.items[i], ctx);

        ga_environmental_select(&pop, &offspring, &next_gen, GA_POP_SIZE);

        pop = next_gen;
        ga_fast_non_dominated_sort(&pop);
        for (int r = 1; r <= GA_POP_SIZE; r++) ga_calc_crowding_for_rank(&pop, r);

        if (ctx->verbose) {
            int r1 = 0, sched_total = 0;
            double min_lat = INFINITY, max_cn = -INFINITY, max_tp = -INFINITY,
                   max_rel = -INFINITY, min_cost = INFINITY;
            for (int i = 0; i < pop.count; i++) {
                Individual *p = &pop.items[i];
                if (p->is_fully_schedulable) sched_total++;
                if (p->rank == 1 && p->is_fully_schedulable) {
                    r1++;
                    if (p->m_comp_lat   < min_lat) min_lat = p->m_comp_lat;
                    if (p->m_conn_norm  > max_cn)  max_cn  = p->m_conn_norm;
                    if (p->m_throughput > max_tp)  max_tp  = p->m_throughput;
                    if (p->m_comp_rel   > max_rel) max_rel = p->m_comp_rel;
                    if (p->m_cost       < min_cost) min_cost = p->m_cost;
                }
            }
            printf("  sched=%d r1=%d  lat=%.4f C_conn=%.4f thpt=%.2f rel=%.4f cost=%.2f\n",
                   sched_total, r1, min_lat, max_cn, max_tp, max_rel, min_cost);
        }
    }

    *pop_out = pop;
}

/* 默认入口：构造 baseline seed 并演化 GA_MAX_GEN 代 */
void ga_run(GaContext *ctx, Population *pop_out) {
    Individual seed;
    ga_create_baseline_seed(&seed, ctx->sb);
    ga_run_from_seed(ctx, &seed, GA_MAX_GEN, pop_out);
}

/* ============================================================
 * 故障注入
 * ============================================================ */
void ga_inject_node_failure(const Individual *base, const Sandbox *sb,
                            int failed_node_id, Individual *out)
{
    *out = *base;
    if (failed_node_id < 0 || failed_node_id >= out->n_roles) return;

    out->role_gene[failed_node_id] = -1;

    /* 与失效节点相连的所有候选链路标记为 -1 */
    for (int j = 0; j < sb->c_link_count; j++) {
        const PhysicalLink *l = &sb->c_links[j];
        if (l->node_a_id == failed_node_id || l->node_b_id == failed_node_id) {
            out->link_gene[j] = -1;
        }
    }

    /* 失效后这个个体的评估结果失效，重置 */
    out->evaluated = 0;
    out->is_fully_schedulable = 0;
    out->rank = 0;
    out->crowding_distance = 0.0;
    for (int i = 0; i < GA_OBJ_COUNT; i++) out->fitness[i] = INFINITY;
}

int ga_strip_flows_for_failed_node(Sandbox *sb, int failed_node_id) {
    int stripped = 0;
    int new_count = 0;
    for (int i = 0; i < sb->flow_graph.count; i++) {
        Flow *f = &sb->flow_graph.flows[i];
        if (f->source_node_id == failed_node_id ||
            f->target_node_id == failed_node_id) {
            stripped++;
            continue;
        }
        if (new_count != i) sb->flow_graph.flows[new_count] = *f;
        new_count++;
    }
    sb->flow_graph.count = new_count;
    return stripped;
}
