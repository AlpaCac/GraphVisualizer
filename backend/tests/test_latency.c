#include "initializer.h"
#include "routing.h"
#include "sb_phys_bind.h"
#include "latency.h"
#include <stdio.h>

int main(void) {
    Sandbox sb; sb_build_sandbox(&sb);

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

    /* 路由 + CPA */
    DeterministicRouter rt; sb_router_init(&rt);
    sb_router_route_all(&rt, &topo);

    CpaAnalyzer cpa; sb_cpa_init(&cpa, &topo);
    sb_cpa_analyze(&cpa);
    sb_cpa_write_back(&cpa);

    printf("=== CPA End-to-End Latency ===\n");
    for (int i = 0; i < topo.flow_count; i++) {
        const Flow *f = &topo.flows[i];
        printf("Flow %2d  %-22s  deadline=%-6.1f  e2e=%.10f  sched=%d\n",
               f->id, f->flow_name, f->deadline, f->worst_case_delay,
               f->is_schedulable);
    }
    return 0;
}
