#ifndef SB_COST_H
#define SB_COST_H

#include "sb_models.h"

/* ============================================================
 * 组网成本评估器
 *   Cost(G) = Σ_{link ∈ G} link.physical_link.cost
 *   - 空拓扑返回 0.0
 *   - 与 Python evaluation/cost.py 等价
 *   - 需要先调用 sb_router_bind_physical 才能取到 PhysicalLink.cost
 * ============================================================ */

/* 直接函数式接口，没有内部状态 */
double sb_cost_evaluate(const BusTopology *topo);

#endif
