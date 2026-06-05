#include "initializer.h"
#include "routing.h"
#include "sb_phys_bind.h"
#include "latency.h"
#include <stdio.h>
#include <time.h>

static unsigned long lcg = 42u;
static int rnd(void) { lcg = lcg*1103515245u+12345u; return (lcg>>16)&0x7FFF; }

int main(void) {
    Sandbox sb; sb_build_sandbox(&sb);
    sb_phys_bind(sb.p_nodes, sb.c_links);

    const int N = 1000;
    struct timespec t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);

    double acc = 0;
    int overload = 0;
    for (int k = 0; k < N; k++) {
        int gene[SB_MAX_LINKS] = {0};
        for (int i = 0; i < sb.c_link_count; i++) gene[i] = (rnd()%3 != 0);

        BusTopology topo; sb_topology_init(&topo);
        for (int i = 0; i < sb.b_node_count; i++) {
            BusNode bn = sb.b_nodes[i];
            bn.source_count=bn.sink_count=bn.relay_count=0; bn.link_count=0;
            sb_topology_add_node(&topo, &bn);
        }
        for (int i = 0; i < sb.c_link_count; i++) {
            if (!gene[i]) continue;
            BusLink bl; sb_bus_link_init(&bl);
            bl.id=sb.c_links[i].id; bl.node_a_id=sb.c_links[i].node_a_id;
            bl.node_b_id=sb.c_links[i].node_b_id; bl.physical_link_idx=i;
            sb_topology_add_link(&topo, &bl);
        }
        for (int i = 0; i < sb.flow_graph.count; i++)
            sb_topology_add_flow(&topo, &sb.flow_graph.flows[i]);

        DeterministicRouter rt; sb_router_init(&rt);
        sb_router_route_all(&rt, &topo);

        CpaAnalyzer cpa; sb_cpa_init(&cpa, &topo);
        sb_cpa_analyze(&cpa);
        for (int fi = 0; fi < topo.flow_count; fi++) {
            double e = sb_cpa_e2e_of(&cpa, fi);
            if (e >= CPA_INF) overload++;
            else acc += e;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);
    double ms = (t2.tv_sec-t1.tv_sec)*1000.0 + (t2.tv_nsec-t1.tv_nsec)/1e6;
    printf("%d topologies (route+CPA): %.1f ms, %.3f ms each, overload=%d, acc=%.2f\n",
           N, ms, ms/N, overload, acc);
    return 0;
}
