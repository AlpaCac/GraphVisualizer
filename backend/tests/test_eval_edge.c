#include "initializer.h"
#include "routing.h"
#include "sb_phys_bind.h"
#include "cost.h"
#include "reliability.h"
#include <stdio.h>

int main(void) {
    Sandbox sb;
    sb_build_sandbox(&sb);
    sb_phys_bind(sb.p_nodes, sb.c_links);

    /* Case 1：空拓扑（无节点无链路） */
    BusTopology empty; sb_topology_init(&empty);
    printf("Case1 (empty): cost = %.6f\n", sb_cost_evaluate(&empty));

    /* Case 2：仅 2 节点、0 链路、1 条流 */
    BusTopology t2; sb_topology_init(&t2);
    BusNode bn; sb_bus_node_init(&bn);
    bn.id = 0; bn.physical_node_idx = 0; sb_topology_add_node(&t2, &bn);
    bn.id = 1; bn.physical_node_idx = 1; sb_topology_add_node(&t2, &bn);
    Flow f; sb_flow_init(&f);
    f.id = 99; snprintf(f.flow_name, SB_STR_LEN, "Test_0_to_1");
    f.source_node_id = 0; f.target_node_id = 1;
    f.priority = 1; f.period = 100; f.deadline = 200;
    f.reliability_requirement = 0.8; f.message_size = 1024;
    sb_topology_add_flow(&t2, &f);

    DeterministicRouter rt; sb_router_init(&rt);
    sb_router_route_all(&rt, &t2);   /* 应该无路径 */
    sb_reliability_evaluate_all(&t2);

    printf("Case2 (2 nodes, 0 link):\n");
    printf("  cost     = %.6f\n", sb_cost_evaluate(&t2));
    printf("  flow.path_len = %d\n", t2.flows[0].path_len);
    printf("  flow.actual_reliability = %.6f\n", t2.flows[0].actual_reliability);
    printf("  flow.is_schedulable = %d\n", t2.flows[0].is_schedulable);
    return 0;
}
