/* 故障场景：剔除 node 15，重建拓扑后再跑路由 */
#include "initializer.h"
#include "routing.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    Sandbox sb;
    sb_build_sandbox(&sb);
    sb_rebuild_with_damage(&sb, 15);   /* node 15 损毁 */

    BusTopology topo; sb_topology_init(&topo);
    for (int i = 0; i < sb.b_node_count; i++) {
        if (sb.b_nodes[i].id == 15) continue;        /* 顺便剔除 b_node */
        BusNode bn = sb.b_nodes[i];
        bn.source_count = bn.sink_count = bn.relay_count = 0;
        bn.link_count = 0;
        sb_topology_add_node(&topo, &bn);
    }
    /* 重新建立 BusNode.physical_node_idx：剔除 node15 后 p_nodes 索引偏移 */
    for (int i = 0; i < topo.node_count; i++) {
        for (int j = 0; j < sb.p_node_count; j++) {
            if (sb.p_nodes[j].id == topo.nodes[i].id) {
                topo.nodes[i].physical_node_idx = j;
                break;
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
    /* 流：剔除源/目的为 15 的流 */
    for (int i = 0; i < sb.flow_graph.count; i++) {
        Flow f = sb.flow_graph.flows[i];
        if (f.source_node_id == 15 || f.target_node_id == 15) continue;
        sb_topology_add_flow(&topo, &f);
    }

    sb_router_bind_physical(sb.p_nodes, sb.c_links);
    DeterministicRouter router; sb_router_init(&router);
    sb_router_route_all(&router, &topo);

    printf("=== Routing after node 15 destroyed ===\n");
    for (int i = 0; i < topo.flow_count; i++) {
        const Flow *f = &topo.flows[i];
        printf("Flow %2d  %-22s  ", f->id, f->flow_name);
        if (f->path_len == 0) { printf("(unreachable)\n"); continue; }
        for (int j = 0; j < f->path_len; j++) {
            printf("%d", f->routing_path[j]);
            if (j+1 < f->path_len) printf(" -> ");
        }
        printf("\n");
    }
    return 0;
}
