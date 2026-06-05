/* ============================================================
 * main_ga.c —— 跑完整 NSGA-II 主循环
 * ============================================================ */
#include "ga.h"
#include "connectivity.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

int main(int argc, char **argv) {
    uint64_t seed = (argc > 1) ? (uint64_t)atoll(argv[1]) : 42ULL;
    ga_seed_rng(seed);

    static Sandbox sb;
    sb_build_sandbox(&sb);

    GaContext ctx;
    ctx.sb = &sb;
    sb_router_init(&ctx.router);
    sb_mac_params_default(&ctx.mac);
    ctx.verbose = 1;

    /* 计算连通度归一化基线 = 全激活候选链路拓扑的 Fiedler 值 */
    {
        static BusTopology baseline_topo;
        Individual all_on;
        ga_individual_init_empty(&all_on, sb.b_node_count, sb.c_link_count);
        for (int i = 0; i < sb.b_node_count; i++)  all_on.role_gene[i] = 1;
        for (int j = 0; j < sb.c_link_count; j++)  all_on.link_gene[j] = 1;
        ga_decode_to_topology(&all_on, &sb, &baseline_topo);
        ctx.lambda_base = sb_connectivity_evaluate(&baseline_topo);
        if (ctx.verbose)
            printf("[baseline] lambda_2 (all-active) = %.6f\n", ctx.lambda_base);
    }

    struct timespec t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);

    static Population pop;
    ga_run(&ctx, &pop);

    clock_gettime(CLOCK_MONOTONIC, &t2);
    double ms = (t2.tv_sec - t1.tv_sec) * 1000.0 +
                (t2.tv_nsec - t1.tv_nsec) / 1e6;

    printf("\n=== Final Pareto front (rank 1, fully schedulable) ===\n");
    int count = 0;
    int min_lat_i = -1, max_fie_i = -1, max_tp_i = -1,
        max_rel_i = -1, min_cost_i = -1;
    double min_lat = INFINITY, max_fie = -INFINITY, max_tp = -INFINITY,
           max_rel = -INFINITY, min_cost = INFINITY;
    for (int i = 0; i < pop.count; i++) {
        Individual *p = &pop.items[i];
        if (p->rank != 1 || !p->is_fully_schedulable) continue;
        count++;
        if (p->m_comp_lat   < min_lat)  { min_lat  = p->m_comp_lat;   min_lat_i  = i; }
        if (p->m_conn_norm  > max_fie)  { max_fie  = p->m_conn_norm;  max_fie_i  = i; }
        if (p->m_throughput > max_tp)   { max_tp   = p->m_throughput; max_tp_i   = i; }
        if (p->m_comp_rel   > max_rel)  { max_rel  = p->m_comp_rel;   max_rel_i  = i; }
        if (p->m_cost       < min_cost) { min_cost = p->m_cost;       min_cost_i = i; }
    }
    printf("rank-1 feasible size: %d\n", count);

    const char *fmt =
        "[%-12s] lat=%.4f max=%.2f C_conn=%.4f thpt=%.2f rel=%.4f cost=%.2f\n";
    if (min_lat_i >= 0) {
        Individual *p = &pop.items[min_lat_i];
        printf(fmt, "min_lat", p->m_comp_lat, p->m_max_lat, p->m_conn_norm,
               p->m_throughput, p->m_comp_rel, p->m_cost);
    }
    if (max_fie_i >= 0) {
        Individual *p = &pop.items[max_fie_i];
        printf(fmt, "max_conn", p->m_comp_lat, p->m_max_lat, p->m_conn_norm,
               p->m_throughput, p->m_comp_rel, p->m_cost);
    }
    if (max_tp_i >= 0) {
        Individual *p = &pop.items[max_tp_i];
        printf(fmt, "max_thpt", p->m_comp_lat, p->m_max_lat, p->m_conn_norm,
               p->m_throughput, p->m_comp_rel, p->m_cost);
    }
    if (max_rel_i >= 0) {
        Individual *p = &pop.items[max_rel_i];
        printf(fmt, "max_rel", p->m_comp_lat, p->m_max_lat, p->m_conn_norm,
               p->m_throughput, p->m_comp_rel, p->m_cost);
    }
    if (min_cost_i >= 0) {
        Individual *p = &pop.items[min_cost_i];
        printf(fmt, "min_cost", p->m_comp_lat, p->m_max_lat, p->m_conn_norm,
               p->m_throughput, p->m_comp_rel, p->m_cost);
    }

    printf("\nTotal time: %.1f ms (%.1f s)\n", ms, ms/1000.0);
    return 0;
}
