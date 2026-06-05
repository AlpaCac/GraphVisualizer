#include "initializer.h"
#include "routing.h"
#include "sb_phys_bind.h"
#include "latency.h"
#include <stdio.h>

int main(void) {
    Sandbox sb; sb_build_sandbox(&sb);
    sb_rebuild_with_damage(&sb, 15);

    BusTopology topo; sb_topology_init(&topo);
    for (int i = 0; i < sb.b_node_count; i++) {
        if (sb.b_nodes[i].id == 15) continue;
        BusNode bn = sb.b_nodes[i];
        bn.source_count = bn.sink_count = bn.relay_count = 0;
        bn.link_count = 0;
        sb_topology_add_node(&topo, &bn);
    }
    for (int i = 0; i < topo.node_count; i++) {
        for (int j = 0; j < sb.p_node_count; j++) {
            if (sb.p_nodes[j].id == topo.nodes[i].id) {
                topo.nodes[i].physical_node_idx = j; break;
            }
        }
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
        if (f.source_node_id == 15 || f.target_node_id == 15) continue;
        sb_topology_add_flow(&topo, &f);
    }

    sb_phys_bind(sb.p_nodes, sb.c_links);

    DeterministicRouter rt; sb_router_init(&rt);
    sb_router_route_all(&rt, &topo);

    CpaAnalyzer cpa; sb_cpa_init(&cpa, &topo);
    sb_cpa_analyze(&cpa);
    sb_cpa_write_back(&cpa);

    printf("=== CPA after node 15 destroyed ===\n");
    for (int i = 0; i < topo.flow_count; i++) {
        const Flow *f = &topo.flows[i];
        printf("Flow %2d  %-22s  deadline=%-6.1f  e2e=%.10f  sched=%d\n",
               f->id, f->flow_name, f->deadline, f->worst_case_delay,
               f->is_schedulable);
    }
    return 0;
}
