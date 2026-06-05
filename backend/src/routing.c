/* ============================================================
 * routing.c —— 严格对齐 Python routing/routing.py
 * ============================================================ */
#include "routing.h"
#include "sb_phys_bind.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 兼容旧入口：转发到共享绑定模块 */
void sb_router_bind_physical(const PhysicalNode *pnodes,
                             const PhysicalLink *plinks) {
    sb_phys_bind(pnodes, plinks);
}

static inline const PhysicalNode *bn_phys(const BusNode *n) {
    return sb_phys_node(n->physical_node_idx);
}
static inline const PhysicalLink *bl_phys(const BusLink *l) {
    return sb_phys_link(l->physical_link_idx);
}

/* ============================================================
 * 节点动态权重计算（与 Python 一致）
 *   load = Σ msg/period over (source/sink[/relay])
 *   w    = α * load / cap
 * ============================================================ */
static double load_of_flow(const Flow *f) {
    double period = f->period > 0 ? f->period : 0.001;
    return f->message_size / period;
}

/* 默认：基线（source + sink） */
static double compute_node_weight_baseline(const DeterministicRouter *r,
                                           const BusTopology *topo,
                                           const BusNode *n) {
    const PhysicalNode *pn = bn_phys(n);
    if (pn->cpu_capacity <= 0) return 0.0;

    double load = 0.0;
    for (int i = 0; i < n->source_count; i++)
        load += load_of_flow(&topo->flows[n->source_flows[i]]);
    for (int i = 0; i < n->sink_count; i++)
        load += load_of_flow(&topo->flows[n->sink_flows[i]]);
    return r->congestion_penalty_alpha * (load / pn->cpu_capacity);
}

/* 中继更新后：source + sink + relay */
static double compute_node_weight_full(const DeterministicRouter *r,
                                       const BusTopology *topo,
                                       const BusNode *n) {
    const PhysicalNode *pn = bn_phys(n);
    if (pn->cpu_capacity <= 0) return 0.0;

    double load = 0.0;
    for (int i = 0; i < n->source_count; i++)
        load += load_of_flow(&topo->flows[n->source_flows[i]]);
    for (int i = 0; i < n->sink_count; i++)
        load += load_of_flow(&topo->flows[n->sink_flows[i]]);
    for (int i = 0; i < n->relay_count; i++)
        load += load_of_flow(&topo->flows[n->relay_flows[i]]);
    return r->congestion_penalty_alpha * (load / pn->cpu_capacity);
}

/* ============================================================
 * Reset
 * ============================================================ */
void sb_router_init(DeterministicRouter *r) {
    r->congestion_penalty_alpha = 1.0;
}

void sb_router_reset_states(const DeterministicRouter *r, BusTopology *topo) {
    /* 1. 链路状态 */
    for (int i = 0; i < topo->link_count; i++) {
        BusLink *l = &topo->links[i];
        l->dynamic_weight = bl_phys(l)->propagation_delay;
        l->current_load   = 0.0;
        l->passing_count  = 0;
    }
    /* 2. 节点动态权重（基线） + 清理 relay */
    for (int i = 0; i < topo->node_count; i++) {
        BusNode *n = &topo->nodes[i];
        n->relay_count = 0;
        n->dynamic_weight = compute_node_weight_baseline(r, topo, n);
    }
    /* 3. 流路径与评估结果 */
    for (int i = 0; i < topo->flow_count; i++) {
        Flow *f = &topo->flows[i];
        f->path_len = 0;
        f->worst_case_delay   = -1.0;   /* 用 -1 代替 None */
        f->actual_reliability = -1.0;
        f->is_schedulable = 1;
    }
}

/* ============================================================
 * 最小堆（dist 升序）
 *   - 用数组实现的二叉堆，与 Python heapq 同语义
 *   - 元素：(dist, node_id)。注意我们不用 (dist, id) 当 tie-break，
 *     因为 Python 也没有那么做——Python 直接 tuple compare，
 *     但本算法靠 visited 集合 + 同代价覆盖规则消除影响。
 *   - 为完全对齐 Python，比较函数中：dist 严格小，则上浮；相等不动。
 * ============================================================ */
typedef struct { double d; int id; } HeapNode;

#define HEAP_CAP (SB_MAX_NODES * 16)

typedef struct {
    HeapNode buf[HEAP_CAP];
    int size;
} MinHeap;

static void heap_init(MinHeap *h) { h->size = 0; }

static int heap_less(const HeapNode *a, const HeapNode *b) {
    /* Python heapq 用 tuple 比较：先比 d，再比 id。
     * 为完全等价，这里也带 id 作为第二关键字。*/
    if (a->d < b->d) return 1;
    if (a->d > b->d) return 0;
    return a->id < b->id;
}

static void heap_push(MinHeap *h, double d, int id) {
    if (h->size >= HEAP_CAP) return;   /* 容量保护 */
    int i = h->size++;
    h->buf[i].d = d; h->buf[i].id = id;
    /* 上浮 */
    while (i > 0) {
        int p = (i - 1) / 2;
        if (heap_less(&h->buf[i], &h->buf[p])) {
            HeapNode t = h->buf[i]; h->buf[i] = h->buf[p]; h->buf[p] = t;
            i = p;
        } else break;
    }
}

static int heap_pop(MinHeap *h, HeapNode *out) {
    if (h->size == 0) return 0;
    *out = h->buf[0];
    h->size--;
    if (h->size > 0) {
        h->buf[0] = h->buf[h->size];
        /* 下沉 */
        int i = 0;
        while (1) {
            int l = 2*i + 1, r = 2*i + 2, s = i;
            if (l < h->size && heap_less(&h->buf[l], &h->buf[s])) s = l;
            if (r < h->size && heap_less(&h->buf[r], &h->buf[s])) s = r;
            if (s == i) break;
            HeapNode t = h->buf[i]; h->buf[i] = h->buf[s]; h->buf[s] = t;
            i = s;
        }
    }
    return 1;
}

/* ============================================================
 * 单源 Dijkstra（带物理带宽剪枝 + 节点ID tie-break）
 *   path_node_ids 输出节点 id 序列（含起点终点），返回长度。
 *   不可达返回 0。
 * ============================================================ */
int sb_router_find_path(const DeterministicRouter *r,
                        BusTopology *topo,
                        int source_id, int target_id,
                        double demand, double message_size,
                        int *path_node_ids, int path_cap)
{
    (void)r;
    int N = topo->node_count;

    /* 用节点 id 做主键索引；为防止 id 不连续，这里用 id2idx 间接寻址。
     * Python 用的是 dict {node_id: ...}，C 这边用平铺数组更快。*/
    static double dist[SB_MAX_NODES * 2];
    static int    prev[SB_MAX_NODES * 2];   /* 前驱节点 id；-1 表示无 */
    static int    visited[SB_MAX_NODES * 2];
    (void)N;

    /* 初始化所有出现过的节点 id */
    for (int i = 0; i < topo->node_count; i++) {
        int nid = topo->nodes[i].id;
        dist[nid]    = DBL_MAX;
        prev[nid]    = -1;
        visited[nid] = 0;
    }
    dist[source_id] = 0.0;

    MinHeap pq; heap_init(&pq);
    heap_push(&pq, 0.0, source_id);

    HeapNode cur;
    while (heap_pop(&pq, &cur)) {
        int u_id = cur.id;
        if (visited[u_id]) continue;
        visited[u_id] = 1;

        if (u_id == target_id) break;

        BusNode *u = sb_topology_get_node(topo, u_id);
        if (!u) continue;
        double u_dist = cur.d;

        /* 遍历 u 上挂的所有链路 */
        for (int k = 0; k < u->link_count; k++) {
            BusLink *link = &topo->links[u->link_idx[k]];
            int v_id = (link->node_b_id == u_id) ? link->node_a_id
                                                 : link->node_b_id;
            if (visited[v_id]) continue;

            const PhysicalLink *pl = bl_phys(link);

            /* 带宽剪枝（0.99 系数） */
            if (link->current_load + demand > pl->bandwidth * 0.99)
                continue;

            /* 边权 = 物理传播时延 + 消息传输时延 */
            double trans = (pl->bandwidth > 0) ? (message_size / pl->bandwidth) : 0.0;
            double weight_edge = pl->propagation_delay + trans;

            /* 节点动态权重 */
            BusNode *v = sb_topology_get_node(topo, v_id);
            double weight_node = v ? v->dynamic_weight : 0.0;

            double alt = u_dist + weight_edge + weight_node;

            if (alt < dist[v_id]) {
                dist[v_id] = alt;
                prev[v_id] = u_id;
                heap_push(&pq, alt, v_id);
            } else if (alt == dist[v_id]) {
                /* tie-break：当前节点 id 比既有前驱小 -> 覆盖（不重新入堆） */
                int old_prev = prev[v_id];
                if (old_prev >= 0 && u_id < old_prev) {
                    prev[v_id] = u_id;
                }
            }
        }
    }

    /* 回溯 */
    if (prev[target_id] < 0 && target_id != source_id) return 0;

    /* 先倒着填，再反转 */
    int tmp[SB_MAX_PATH_HOPS];
    int len = 0;
    int x = target_id;
    while (x >= 0 && len < SB_MAX_PATH_HOPS) {
        tmp[len++] = x;
        if (x == source_id) break;
        x = prev[x];
    }
    if (tmp[len-1] != source_id) return 0;   /* 异常 */

    if (len > path_cap) return 0;
    for (int i = 0; i < len; i++) path_node_ids[i] = tmp[len - 1 - i];
    return len;
}

/* ============================================================
 * 在路径上更新链路负载 + 中继节点
 * ============================================================ */
static void update_link_states(const DeterministicRouter *r,
                               BusTopology *topo,
                               const int *path, int len,
                               int flow_idx, double demand)
{
    /* 1) 链路 */
    for (int i = 0; i < len - 1; i++) {
        BusLink *l = sb_topology_get_link(topo, path[i], path[i+1]);
        if (!l) continue;
        l->current_load += demand;
        if (l->passing_count < SB_MAX_PASSING_PER_LINK)
            l->passing_flows[l->passing_count++] = flow_idx;
    }
    /* 2) 中继节点：path[1..len-2] */
    for (int i = 1; i < len - 1; i++) {
        BusNode *n = sb_topology_get_node(topo, path[i]);
        if (!n) continue;
        if (n->relay_count < SB_MAX_FLOWS_PER_NODE)
            n->relay_flows[n->relay_count++] = flow_idx;
        n->dynamic_weight = compute_node_weight_full(r, topo, n);
    }
}

/* ============================================================
 * 全局排序键：priority 降序，id 升序
 * ============================================================ */
typedef struct { int flow_idx; int priority; int flow_id; } FlowOrderKey;
static int cmp_flow_order(const void *a, const void *b) {
    const FlowOrderKey *fa = (const FlowOrderKey*)a;
    const FlowOrderKey *fb = (const FlowOrderKey*)b;
    if (fa->priority != fb->priority) return fb->priority - fa->priority; /* desc */
    return fa->flow_id - fb->flow_id; /* asc */
}

/* ============================================================
 * 主入口
 * ============================================================ */
void sb_router_route_all(const DeterministicRouter *r, BusTopology *topo) {
    sb_router_reset_states(r, topo);

    /* 排序 */
    FlowOrderKey keys[SB_MAX_FLOWS];
    int n = topo->flow_count;
    for (int i = 0; i < n; i++) {
        keys[i].flow_idx = i;
        keys[i].priority = topo->flows[i].priority;
        keys[i].flow_id  = topo->flows[i].id;
    }
    qsort(keys, n, sizeof(FlowOrderKey), cmp_flow_order);

    /* 顺序寻路 */
    for (int k = 0; k < n; k++) {
        int idx = keys[k].flow_idx;
        Flow *f = &topo->flows[idx];

        double period = f->period > 0 ? f->period : 0.001;
        double demand = f->message_size / period;

        int path[SB_MAX_PATH_HOPS];
        int len = sb_router_find_path(r, topo,
                                      f->source_node_id, f->target_node_id,
                                      demand, f->message_size,
                                      path, SB_MAX_PATH_HOPS);
        if (len > 0) {
            sb_flow_set_path(f, path, len);
            if (len > 1)
                update_link_states(r, topo, path, len, idx, demand);
        } else {
            f->path_len = 0;
        }
    }
}
