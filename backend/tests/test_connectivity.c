#include "initializer.h"
#include "connectivity.h"
#include <stdio.h>

/* 把候选物理链路按 link_gene 选择性地装入拓扑 */
static void build_topo(const Sandbox *sb, const int *link_gene,
                       BusTopology *topo) {
    sb_topology_init(topo);
    for (int i = 0; i < sb->b_node_count; i++) {
        BusNode bn = sb->b_nodes[i];
        bn.source_count = bn.sink_count = bn.relay_count = 0;
        bn.link_count = 0;
        sb_topology_add_node(topo, &bn);
    }
    for (int i = 0; i < sb->c_link_count; i++) {
        if (link_gene && !link_gene[i]) continue;
        BusLink bl; sb_bus_link_init(&bl);
        bl.id = sb->c_links[i].id;
        bl.node_a_id = sb->c_links[i].node_a_id;
        bl.node_b_id = sb->c_links[i].node_b_id;
        bl.physical_link_idx = i;
        sb_topology_add_link(topo, &bl);
    }
}

int main(void) {
    Sandbox sb;
    sb_build_sandbox(&sb);

    BusTopology t;

    /* Case 1：全激活 83 条链路 */
    build_topo(&sb, NULL, &t);
    printf("Case1 (all 83 links): Fiedler = %.10f\n",
           sb_connectivity_evaluate(&t));

    /* Case 2：只激活前 30 条，看连通度怎么变 */
    {
        int gene[SB_MAX_LINKS] = {0};
        for (int i = 0; i < 30; i++) gene[i] = 1;
        build_topo(&sb, gene, &t);
        printf("Case2 (first 30 links):   Fiedler = %.10f\n",
               sb_connectivity_evaluate(&t));
    }

    /* Case 3：完全没链路（不连通） */
    {
        int gene[SB_MAX_LINKS] = {0};
        build_topo(&sb, gene, &t);
        printf("Case3 (no links):         Fiedler = %.10f\n",
               sb_connectivity_evaluate(&t));
    }

    /* Case 4：只有 1 条链路 0-1 */
    {
        int gene[SB_MAX_LINKS] = {0};
        gene[0] = 1;
        build_topo(&sb, gene, &t);
        printf("Case4 (only link 0-1):    Fiedler = %.10f\n",
               sb_connectivity_evaluate(&t));
    }

    /* Case 5：只激活奇数索引的链路 */
    {
        int gene[SB_MAX_LINKS] = {0};
        for (int i = 1; i < sb.c_link_count; i += 2) gene[i] = 1;
        build_topo(&sb, gene, &t);
        printf("Case5 (odd-idx links):    Fiedler = %.10f\n",
               sb_connectivity_evaluate(&t));
    }

    return 0;
}
