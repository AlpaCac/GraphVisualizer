#ifndef SB_ROUTING_H
#define SB_ROUTING_H

#include "sb_models.h"

/* ============================================================
 * Deterministic Router —— 与 Python routing/routing.py 等价
 *
 * 关键性质：
 *   - 改进版 Dijkstra：物理带宽剪枝 + 节点动态算力权重 + 节点ID tie-break
 *   - 输入相同 -> 输出完全相同（无随机性）
 *   - 寻路顺序：业务流按 (priority 降序, id 升序) 严格排序
 * ============================================================ */

typedef struct {
    double congestion_penalty_alpha;   /* 拥塞惩罚系数，默认 1.0 */
} DeterministicRouter;

/* 默认构造 */
void sb_router_init(DeterministicRouter *r);

/* 绑定物理对象表，使 BusNode/BusLink 能反查 cpu_capacity/bandwidth/... */
void sb_router_bind_physical(const PhysicalNode *pnodes,
                             const PhysicalLink *plinks);

/* 重置：链路负载/动态权重、节点动态权重、流路径与评估结果 */
void sb_router_reset_states(const DeterministicRouter *r,
                            BusTopology *topo);

/* 对拓扑内所有 flow 做全局确定性寻路；写回 flow.routing_path */
void sb_router_route_all(const DeterministicRouter *r,
                         BusTopology *topo);

/* 单条流寻路：成功返回路径长度并写回 path_node_ids[]；不可达返回 0 */
int  sb_router_find_path(const DeterministicRouter *r,
                         BusTopology *topo,
                         int source_node_id,
                         int target_node_id,
                         double demand,
                         double message_size,
                         int *path_node_ids,
                         int path_cap);

#endif
