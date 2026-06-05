/* ============================================================
 * test_routing.c
 *   - 构造与 Python 等价的"全激活"拓扑（83 条候选链路全部纳入）
 *   - 跑确定性路由
 *   - 输出 14 条流的路径
 * ============================================================ */
#include "initializer.h"
#include "routing.h"
#include <stdio.h>

int main(void) {
    Sandbox sb;
    sb_build_sandbox(&sb);

    /* 1. 构造 BusTopology：全部节点 + 全部链路 */
    BusTopology topo;
    sb_topology_init(&topo);

    for (int i = 0; i < sb.b_node_count; i++) {
        BusNode bn = sb.b_nodes[i];     /* 拷贝；source/sink 重新挂到 topo */
        bn.source_count = bn.sink_count = bn.relay_count = 0;
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
    /* 流：复制并通过 add_flow 重新挂载 source/sink */
    for (int i = 0; i < sb.flow_graph.count; i++) {
        Flow f = sb.flow_graph.flows[i];
        sb_topology_add_flow(&topo, &f);
    }

    /* 2. 路由 */
    sb_router_bind_physical(sb.p_nodes, sb.c_links);
    DeterministicRouter router; sb_router_init(&router);
    sb_router_route_all(&router, &topo);

    /* 3. 输出（按 flow.id 升序，便于和 Python 对照） */
    printf("=== Routing Result (all-active candidate links) ===\n");
    for (int id = 0; id < topo.flow_count; id++) {
        const Flow *f = &topo.flows[id];
        printf("Flow %2d  %-22s  ", f->id, f->flow_name);
        if (f->path_len == 0) {
            printf("(unreachable)\n");
            continue;
        }
        printf("path: ");
        for (int j = 0; j < f->path_len; j++) {
            printf("%d", f->routing_path[j]);
            if (j + 1 < f->path_len) printf(" -> ");
        }
        printf("\n");
    }

    /* 4. 打印链路 current_load 前 10 条 */
    printf("\n=== Top 10 link loads ===\n");
    for (int i = 0; i < topo.link_count && i < 10; i++) {
        const BusLink *l = &topo.links[i];
        printf(" link %2d-%-2d  load=%.4f  passing=%d\n",
               l->node_a_id, l->node_b_id, l->current_load, l->passing_count);
    }

    /* 5. 打印每个节点的最终 dynamic_weight */
    printf("\n=== Node dynamic weights ===\n");
    for (int i = 0; i < topo.node_count; i++) {
        const BusNode *n = &topo.nodes[i];
        printf(" node %2d: dw=%.6f  src=%d sink=%d relay=%d\n",
               n->id, n->dynamic_weight,
               n->source_count, n->sink_count, n->relay_count);
    }
    return 0;
}
