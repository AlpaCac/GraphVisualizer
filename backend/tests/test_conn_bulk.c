/* 用固定 LCG 生成 100 组随机 link_gene，把 Fiedler 值挨个打印出来 */
#include "initializer.h"
#include "connectivity.h"
#include <stdio.h>

static unsigned long lcg = 1u;
static int rnd(void) {
    lcg = lcg * 1103515245u + 12345u;
    return (lcg >> 16) & 0x7FFF;
}

static void build_topo(const Sandbox *sb, const int *gene, BusTopology *t) {
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
}

int main(void) {
    Sandbox sb; sb_build_sandbox(&sb);
    BusTopology t;
    for (int trial = 0; trial < 100; trial++) {
        int gene[SB_MAX_LINKS] = {0};
        for (int i = 0; i < sb.c_link_count; i++) gene[i] = rnd() & 1;
        build_topo(&sb, gene, &t);
        double f = sb_connectivity_evaluate(&t);
        printf("trial %3d: fiedler=%.10f\n", trial, f);
    }
    return 0;
}
