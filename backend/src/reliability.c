/* ============================================================
 * reliability.c —— 与 Python evaluation/reliability.py 等价
 * ============================================================ */
#include "reliability.h"
#include "sb_phys_bind.h"

double sb_reliability_evaluate_flow(const Flow *f, BusTopology *topo) {
    if (!f || f->path_len <= 0) return 0.0;

    double r = 1.0;

    /* 1) 节点可靠性连乘 */
    for (int i = 0; i < f->path_len; i++) {
        BusNode *n = sb_topology_get_node(topo, f->routing_path[i]);
        if (!n) return 0.0;
        r *= sb_phys_node(n->physical_node_idx)->reliability;
    }

    /* 2) 链路可靠性连乘 */
    for (int i = 0; i < f->path_len - 1; i++) {
        BusLink *l = sb_topology_get_link(topo,
                                          f->routing_path[i],
                                          f->routing_path[i+1]);
        if (!l) return 0.0;   /* 路径上的相邻节点没有链路：异常 -> 0 */
        r *= sb_phys_link(l->physical_link_idx)->reliability;
    }

    return r;
}

void sb_reliability_evaluate_all(BusTopology *topo) {
    if (!topo) return;
    for (int i = 0; i < topo->flow_count; i++) {
        Flow *f = &topo->flows[i];
        f->actual_reliability = sb_reliability_evaluate_flow(f, topo);

        /* AND 累积，与 Python 一致 */
        int meets = (f->actual_reliability >= f->reliability_requirement);
        f->is_schedulable = f->is_schedulable && meets;
    }
}
