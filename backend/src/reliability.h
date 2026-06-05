#ifndef SB_RELIABILITY_H
#define SB_RELIABILITY_H

#include "sb_models.h"

/* ============================================================
 * 可靠性评估器
 *   端到端可靠性 = Π 路径上节点可靠性 × Π 路径上链路可靠性
 *   - 路径为空 / 中间断链 -> 返回 0.0
 *   - 与 Python evaluation/reliability.py 等价
 *   - 调用前需 sb_phys_bind(p_nodes, c_links)
 *
 * evaluate_all 会写回 flow.actual_reliability，并把 is_schedulable
 * 用 AND 累积（不会从 false 翻回 true）。
 * ============================================================ */

double sb_reliability_evaluate_flow(const Flow *f, BusTopology *topo);
void   sb_reliability_evaluate_all (BusTopology *topo);

#endif
