#include "initializer.h"
#include "connectivity.h"
#include <stdio.h>
#include <time.h>

static unsigned long lcg = 12345u;
static int rnd(void) {
    lcg = lcg * 1103515245u + 12345u;
    return (lcg >> 16) & 0x7FFF;
}

int main(void) {
    Sandbox sb; sb_build_sandbox(&sb);
    int gene[SB_MAX_LINKS] = {0};
    BusTopology t;

    const int N = 10000;
    struct timespec t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);

    double acc = 0.0;
    for (int k = 0; k < N; k++) {
        for (int i = 0; i < sb.c_link_count; i++) gene[i] = rnd() & 1;

        sb_topology_init(&t);
        for (int i = 0; i < sb.b_node_count; i++) {
            BusNode bn = sb.b_nodes[i];
            bn.source_count = bn.sink_count = bn.relay_count = 0;
            bn.link_count = 0;
            sb_topology_add_node(&t, &bn);
        }
        for (int i = 0; i < sb.c_link_count; i++) {
            if (!gene[i]) continue;
            BusLink bl; sb_bus_link_init(&bl);
            bl.id = sb.c_links[i].id;
            bl.node_a_id = sb.c_links[i].node_a_id;
            bl.node_b_id = sb.c_links[i].node_b_id;
            bl.physical_link_idx = i;
            sb_topology_add_link(&t, &bl);
        }

        acc += sb_connectivity_evaluate(&t);
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);

    double ms = (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_nsec - t1.tv_nsec) / 1e6;
    printf("%d evaluations: %.1f ms, %.3f us each, acc=%.4f\n",
           N, ms, ms*1000.0/N, acc);
    return 0;
}
