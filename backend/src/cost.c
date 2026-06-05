/* ============================================================
 * cost.c —— 与 Python evaluation/cost.py 等价
 *
 * Cost(G) = Σ_{link∈G} link.physical_link.cost
 *   - 拓扑无链路返回 0.0
 *   - 调用前需 sb_phys_bind(p_nodes, c_links)
 * ============================================================ */
#include "cost.h"
#include "sb_phys_bind.h"

double sb_cost_evaluate(const BusTopology *topo) {
    if (!topo || topo->link_count == 0) return 0.0;
    double total = 0.0;
    for (int i = 0; i < topo->link_count; i++) {
        const BusLink *l = &topo->links[i];
        total += sb_phys_link(l->physical_link_idx)->cost;
    }
    return total;
}
