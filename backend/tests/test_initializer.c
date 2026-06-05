#include "initializer.h"
#include <stdio.h>

static const char *flow_kind_tag(const char *name) {
    if (!name) return "?";
    if (name[0]=='V') return "VID";
    if (name[0]=='C') return "C2 ";
    if (name[0]=='S') return "SYN";
    if (name[0]=='T') return "TEL";
    return "?";
}

int main(void) {
    Sandbox sb;
    sb_build_sandbox(&sb);

    printf("=== Sandbox Initialization Complete ===\n");
    printf("Physical Nodes: %d\n", sb.p_node_count);
    printf("Candidate Links: %d\n", sb.c_link_count);
    printf("Total Flows Defined: %d\n\n", sb.flow_graph.count);

    /* 节点详情 */
    printf("--- Nodes ---\n");
    for (int i = 0; i < sb.p_node_count; i++) {
        const PhysicalNode *n = &sb.p_nodes[i];
        printf(" id=%-2d  (%4.0f,%4.0f) cpu=%-6.0f ports=%d rel=%.3f  %s\n",
               n->id, n->x, n->y, n->cpu_capacity, n->max_physical_ports,
               n->reliability,
               n->meta.typeInfo.staticType == NODE_TYPE_MASTER ? "MASTER" : "COMPUTE");
    }

    /* 链路前/后各 5 条，方便和 Python 比对 */
    printf("\n--- Candidate Links (first 5) ---\n");
    for (int i = 0; i < 5 && i < sb.c_link_count; i++) {
        const PhysicalLink *l = &sb.c_links[i];
        printf(" id=%-3d  %2d-%-2d  BW=%9.3f  prop=%6.3f  cost=%7.3f  rel=%.6f\n",
               l->id, l->node_a_id, l->node_b_id,
               l->bandwidth, l->propagation_delay, l->cost, l->reliability);
    }
    printf("--- Candidate Links (last 5) ---\n");
    for (int i = sb.c_link_count - 5; i < sb.c_link_count; i++) {
        if (i < 0) continue;
        const PhysicalLink *l = &sb.c_links[i];
        printf(" id=%-3d  %2d-%-2d  BW=%9.3f  prop=%6.3f  cost=%7.3f  rel=%.6f\n",
               l->id, l->node_a_id, l->node_b_id,
               l->bandwidth, l->propagation_delay, l->cost, l->reliability);
    }

    /* 流详情 */
    printf("\n--- Flows ---\n");
    for (int i = 0; i < sb.flow_graph.count; i++) {
        const Flow *f = &sb.flow_graph.flows[i];
        printf(" id=%-2d %s  %-22s  src=%-2d dst=%-2d prio=%-2d period=%-5.1f deadline=%-5.1f msg=%-7.0f relReq=%.2f\n",
               f->id, flow_kind_tag(f->flow_name),
               f->flow_name, f->source_node_id, f->target_node_id,
               f->priority, f->period, f->deadline, f->message_size,
               f->reliability_requirement);
    }

    /* 节点上的 source/sink 挂载情况 */
    printf("\n--- BusNode source/sink counts ---\n");
    for (int i = 0; i < sb.b_node_count; i++) {
        const BusNode *n = &sb.b_nodes[i];
        if (n->source_count == 0 && n->sink_count == 0) continue;
        printf(" node %-2d: source=%d sink=%d\n",
               n->id, n->source_count, n->sink_count);
    }

    /* 损毁重建对比：剔除 node 15 后链路数应当减少 */
    int before = sb.c_link_count;
    sb_rebuild_with_damage(&sb, 15);
    printf("\n--- After removing node 15 ---\n");
    printf("Physical Nodes: %d  (was 20)\n", sb.p_node_count);
    printf("Candidate Links: %d  (was %d)\n", sb.c_link_count, before);

    return 0;
}
