#include "initializer.h"
#include "routing.h"
#include "sb_phys_bind.h"
#include "cost.h"
#include "reliability.h"
#include <stdio.h>

int main(void) {
    Sandbox sb;
    sb_build_sandbox(&sb);

    /* 全激活拓扑 */
    BusTopology topo; sb_topology_init(&topo);
    for (int i = 0; i < sb.b_node_count; i++) {
        BusNode bn = sb.b_nodes[i];
        bn.source_count = bn.sink_count = bn.relay_count = 0;
        bn.link_count = 0;
        sb_topology_add_node(&topo, &bn);
    }
    for (int i = 0; i < sb.c_link_count; i++) {
        BusLink bl; sb_bus_link_init(&bl);
        bl.id = sb.c_links[i].id;
        bl.node_a_id = sb.c_links[i].node_a_id;
        bl.node_b_id = sb.c_links[i].node_b_id;
        bl.physical_link_idx = i;
        sb_topology_add_link(&topo, &bl);
    }
    for (int i = 0; i < sb.flow_graph.count; i++) {
        Flow f = sb.flow_graph.flows[i];
        sb_topology_add_flow(&topo, &f);
    }

    sb_phys_bind(sb.p_nodes, sb.c_links);

    /* 路由 */
    DeterministicRouter rt; sb_router_init(&rt);
    sb_router_route_all(&rt, &topo);

    /* 成本 */
    double total_cost = sb_cost_evaluate(&topo);
    printf("=== Cost ===\n");
    printf("total_cost = %.6f\n", total_cost);

    /* 可靠性 */
    sb_reliability_evaluate_all(&topo);
    printf("\n=== Reliability per flow ===\n");
    for (int i = 0; i < topo.flow_count; i++) {
        const Flow *f = &topo.flows[i];
        printf("Flow %2d  %-22s  rel=%.8f  req=%.2f  sched=%d\n",
               f->id, f->flow_name,
               f->actual_reliability,
               f->reliability_requirement,
               f->is_schedulable);
    }
    return 0;
}
