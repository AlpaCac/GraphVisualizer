#include "initializer.h"
#include "routing.h"
#include "sb_phys_bind.h"
#include "latency.h"
#include <stdio.h>

static unsigned long lcg = 42u;
static int rnd(void) {
    lcg = lcg * 1103515245u + 12345u;
    return (lcg >> 16) & 0x7FFF;
}

static int build_topo(const Sandbox *sb, const int *gene, BusTopology *t) {
    sb_topology_init(t);
    for (int i = 0; i < sb->b_node_count; i++) {
        BusNode bn = sb->b_nodes[i];
        bn.source_count = bn.sink_count = bn.relay_count = 0;
        bn.link_count = 0;
        sb_topology_add_node(t, &bn);
    }
    for (int i = 0; i < sb->c_link_count; i++) {
        if (!gene[i]) continue;
        BusLink bl; sb_bus_link_init(&bl);
        bl.id = sb->c_links[i].id;
        bl.node_a_id = sb->c_links[i].node_a_id;
        bl.node_b_id = sb->c_links[i].node_b_id;
        bl.physical_link_idx = i;
        sb_topology_add_link(t, &bl);
    }
    for (int i = 0; i < sb->flow_graph.count; i++) {
        Flow f = sb->flow_graph.flows[i];
        sb_topology_add_flow(t, &f);
    }
    return 0;
}

int main(void) {
    Sandbox sb; sb_build_sandbox(&sb);
    sb_phys_bind(sb.p_nodes, sb.c_links);

    BusTopology topo;
    DeterministicRouter rt; sb_router_init(&rt);

    for (int trial = 0; trial < 200; trial++) {
        int gene[SB_MAX_LINKS] = {0};
        /* 不同 trial 用不同密度：1/4 / 1/3 / 1/2 / 2/3 */
        int mod = 2 + (trial % 4);  /* 2..5 */
        for (int i = 0; i < sb.c_link_count; i++)
            gene[i] = (rnd() % mod != 0);

        build_topo(&sb, gene, &topo);
        sb_router_route_all(&rt, &topo);

        CpaAnalyzer cpa; sb_cpa_init(&cpa, &topo);
        sb_cpa_analyze(&cpa);
        sb_cpa_write_back(&cpa);

        printf("trial %2d  ", trial);
        for (int i = 0; i < topo.flow_count; i++) {
            const Flow *f = &topo.flows[i];
            if (f->worst_case_delay >= CPA_INF)
                printf("F%d=INF ", f->id);
            else
                printf("F%d=%.6f ", f->id, f->worst_case_delay);
        }
        printf("\n");
    }
    return 0;
}
