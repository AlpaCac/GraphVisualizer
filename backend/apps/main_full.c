/* ============================================================
 * main_full.c —— 完整实验：
 *   1. baseline seed 评估
 *   2. 常规演化 50 代
 *   3. 注入 node 15 故障
 *   4. 重建演化 50 代
 *   5. 全程结果输出到 experiment.json
 *
 * 用法：
 *   ./build/main_full [seed] [out.json]
 *
 * 默认 seed=42，输出 experiment.json
 * ============================================================ */
#include "ga.h"
#include "sb_phys_bind.h"
#include "jsonw.h"
#include "initializer.h"
#include "routing.h"
#include "cost.h"
#include "reliability.h"
#include "connectivity.h"
#include "latency.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

/* ---------- 序列化工具 ---------- */

static void write_metrics(JsonWriter *jw, const Individual *p) {
    jw_obj_open(jw);
      jw_kv_dbl (jw, "comp_lat",        p->m_comp_lat);
      jw_kv_dbl (jw, "max_lat",         p->m_max_lat);
      jw_kv_dbl (jw, "fiedler",         p->m_fiedler);          /* 原始 λ_2 */
      jw_kv_dbl (jw, "conn_norm",       p->m_conn_norm);        /* C_conn 归一化 */
      jw_kv_dbl (jw, "throughput",      p->m_throughput);       /* 系统级加权吞吐 byte/ms */
      jw_kv_dbl (jw, "min_satisfaction",p->m_min_satisfaction); /* min(eff/req) */
      jw_kv_dbl (jw, "comp_rel",        p->m_comp_rel);
      jw_kv_dbl (jw, "cost",            p->m_cost);
      jw_kv_bool(jw, "fully_schedulable", p->is_fully_schedulable);
      jw_kv_int (jw, "rank",            p->rank);
    jw_obj_close(jw);
}

static void write_individual(JsonWriter *jw, const Individual *p) {
    jw_obj_open(jw);
      jw_kv_int_arr(jw, "role_gene", p->role_gene, p->n_roles);
      jw_kv_int_arr(jw, "link_gene", p->link_gene, p->n_links);
      jw_key(jw, "metrics"); write_metrics(jw, p);
    jw_obj_close(jw);
}

/* 重新跑一次评估并把路径详情写出 */
static void write_flow_details(JsonWriter *jw, const Individual *p,
                               GaContext *ctx)
{
    static BusTopology topo;
    ga_decode_to_topology(p, ctx->sb, &topo);
    sb_router_route_all(&ctx->router, &topo);

    CpaAnalyzer cpa;
    sb_cpa_init(&cpa, &topo);
    sb_cpa_analyze(&cpa);
    sb_cpa_write_back(&cpa);
    sb_reliability_evaluate_all(&topo);

    jw_arr_open(jw);
    for (int i = 0; i < topo.flow_count; i++) {
        const Flow *f = &topo.flows[i];
        jw_obj_open(jw);
          jw_kv_int(jw, "flow_id", f->id);
          jw_kv_str(jw, "name",    f->flow_name);
          jw_kv_int(jw, "src",     f->source_node_id);
          jw_kv_int(jw, "dst",     f->target_node_id);
          jw_kv_int(jw, "priority",f->priority);
          jw_kv_dbl(jw, "period",  f->period);
          jw_kv_dbl(jw, "deadline",f->deadline);
          jw_kv_dbl(jw, "msg_size",f->message_size);
          jw_kv_dbl(jw, "e2e_latency",         f->worst_case_delay);
          jw_kv_dbl(jw, "actual_reliability",  f->actual_reliability);
          jw_kv_dbl(jw, "effective_throughput",f->effective_throughput);
          jw_kv_dbl(jw, "throughput_requirement", f->throughput_requirement);
          jw_kv_bool(jw,"is_schedulable",      f->is_schedulable);
          jw_kv_int_arr(jw, "path", f->routing_path, f->path_len);
        jw_obj_close(jw);
    }
    jw_arr_close(jw);
}

/* 写当前 topology 的节点 / 链路 snapshot */
static void write_topology_snapshot(JsonWriter *jw, const Individual *p,
                                    GaContext *ctx)
{
    static BusTopology topo;
    ga_decode_to_topology(p, ctx->sb, &topo);

    jw_obj_open(jw);
      jw_key(jw, "nodes");
      jw_arr_open(jw);
      for (int i = 0; i < topo.node_count; i++) {
          const BusNode *n = &topo.nodes[i];
          const PhysicalNode *pn = &ctx->sb->p_nodes[n->physical_node_idx];
          jw_obj_open(jw);
            jw_kv_int (jw, "id",      n->id);
            jw_kv_bool(jw, "is_core", n->is_core);
            jw_kv_dbl (jw, "x",       pn->x);
            jw_kv_dbl (jw, "y",       pn->y);
            jw_kv_dbl (jw, "cpu",     pn->cpu_capacity);
            jw_kv_dbl (jw, "reliability", pn->reliability);
            jw_kv_int (jw, "max_ports",   pn->max_physical_ports);
          jw_obj_close(jw);
      }
      jw_arr_close(jw);

      jw_key(jw, "links");
      jw_arr_open(jw);
      for (int i = 0; i < topo.link_count; i++) {
          const BusLink *l = &topo.links[i];
          const PhysicalLink *pl = &ctx->sb->c_links[l->physical_link_idx];
          jw_obj_open(jw);
            jw_kv_int(jw, "id", l->id);
            jw_kv_int(jw, "a",  l->node_a_id);
            jw_kv_int(jw, "b",  l->node_b_id);
            jw_kv_dbl(jw, "bandwidth", pl->bandwidth);
            jw_kv_dbl(jw, "prop_delay", pl->propagation_delay);
            jw_kv_dbl(jw, "cost",      pl->cost);
            jw_kv_dbl(jw, "reliability", pl->reliability);
          jw_obj_close(jw);
      }
      jw_arr_close(jw);
    jw_obj_close(jw);
}

/* 写一代演化统计 */
static void write_generation_stats(JsonWriter *jw, int gen, const Population *pop) {
    int sched_total = 0, r1_feasible = 0;
    double min_lat = INFINITY, max_cn = -INFINITY, max_tp = -INFINITY,
           max_rel = -INFINITY, min_cost = INFINITY;
    for (int i = 0; i < pop->count; i++) {
        const Individual *p = &pop->items[i];
        if (p->is_fully_schedulable) sched_total++;
        if (p->rank == 1 && p->is_fully_schedulable) {
            r1_feasible++;
            if (p->m_comp_lat   < min_lat) min_lat = p->m_comp_lat;
            if (p->m_conn_norm  > max_cn)  max_cn  = p->m_conn_norm;
            if (p->m_throughput > max_tp)  max_tp  = p->m_throughput;
            if (p->m_comp_rel   > max_rel) max_rel = p->m_comp_rel;
            if (p->m_cost       < min_cost) min_cost = p->m_cost;
        }
    }
    jw_obj_open(jw);
      jw_kv_int(jw, "generation",      gen);
      jw_kv_int(jw, "sched_total",     sched_total);
      jw_kv_int(jw, "rank1_feasible",  r1_feasible);
      jw_kv_dbl(jw, "min_comp_lat",    min_lat);
      jw_kv_dbl(jw, "max_conn_norm",   max_cn);
      jw_kv_dbl(jw, "max_throughput",  max_tp);
      jw_kv_dbl(jw, "max_comp_rel",    max_rel);
      jw_kv_dbl(jw, "min_cost",        min_cost);
    jw_obj_close(jw);
}

/* 找 rank=1 可行解里的 5 个极值 idx */
static void find_pareto_extremes(const Population *pop,
                                 int *out_lat, int *out_conn, int *out_tp,
                                 int *out_rel, int *out_cost)
{
    *out_lat = *out_conn = *out_tp = *out_rel = *out_cost = -1;
    double min_lat = INFINITY, max_cn = -INFINITY, max_tp = -INFINITY,
           max_rel = -INFINITY, min_cost = INFINITY;
    for (int i = 0; i < pop->count; i++) {
        const Individual *p = &pop->items[i];
        if (p->rank != 1 || !p->is_fully_schedulable) continue;
        if (p->m_comp_lat   < min_lat) { min_lat = p->m_comp_lat;   *out_lat  = i; }
        if (p->m_conn_norm  > max_cn)  { max_cn  = p->m_conn_norm;  *out_conn = i; }
        if (p->m_throughput > max_tp)  { max_tp  = p->m_throughput; *out_tp   = i; }
        if (p->m_comp_rel   > max_rel) { max_rel = p->m_comp_rel;   *out_rel  = i; }
        if (p->m_cost       < min_cost){ min_cost = p->m_cost;      *out_cost = i; }
    }
}

/* ============================================================
 *  Verbose 跑外加 JSON 输出。整段被简化为：
 *    1. baseline seed 评估，写 seed snapshot
 *    2. 常规演化 50 代（边演化边把每代 stats 推入 normal_evolution 数组）
 *    3. 选 best_lat，写 normal_final
 *    4. 故障注入 + 即时评估
 *    5. 重建演化 50 代
 *    6. 写 reconstruct_final
 * ============================================================ */

/* 自定义版本的 run_from_seed：暴露每代统计回调
 * （为了把 stats 实时写入 JSON）。简单做法：直接复制 ga_run_from_seed 的内部循环。 */
static void run_with_stats(GaContext *ctx, const Individual *seed_in,
                           int max_gen, Population *pop_out,
                           JsonWriter *jw)
{
    sb_phys_bind(ctx->sb->p_nodes, ctx->sb->c_links);

    Individual seed = *seed_in;
    ga_evaluate(&seed, ctx);

    static Population pop;
    pop.count = 0;
    pop.items[pop.count++] = seed;

    int attempts = 0;
    static BusTopology topo;
    while (pop.count < GA_POP_SIZE) {
        attempts++;
        Individual cand = seed;
        ga_mutate(&cand, 0.1);
        ga_decode_to_topology(&cand, ctx->sb, &topo);
        if (ga_validate_topology(&topo, ctx->sb)) pop.items[pop.count++] = cand;
        if (attempts > 10000000) break;
    }
    if (ctx->verbose) printf("[init pop] %d valid in %d attempts\n", pop.count, attempts);

    for (int i = 0; i < pop.count; i++) ga_evaluate(&pop.items[i], ctx);

    ga_fast_non_dominated_sort(&pop);
    for (int r = 1; r <= GA_POP_SIZE; r++) ga_calc_crowding_for_rank(&pop, r);

    /* gen 0：初始种群 stats */
    if (jw) write_generation_stats(jw, 0, &pop);

    static Population offspring, next_gen;
    for (int gen = 0; gen < max_gen; gen++) {
        ga_generate_offspring(&pop, &seed, ctx, &offspring, GA_POP_SIZE);
        for (int i = 0; i < offspring.count; i++) ga_evaluate(&offspring.items[i], ctx);
        ga_environmental_select(&pop, &offspring, &next_gen, GA_POP_SIZE);
        pop = next_gen;
        ga_fast_non_dominated_sort(&pop);
        for (int r = 1; r <= GA_POP_SIZE; r++) ga_calc_crowding_for_rank(&pop, r);

        if (jw) write_generation_stats(jw, gen + 1, &pop);

        if (ctx->verbose) {
            int r1 = 0, sched = 0;
            double mnl = INFINITY, mxc = -INFINITY, mxt = -INFINITY;
            for (int i = 0; i < pop.count; i++) {
                Individual *p = &pop.items[i];
                if (p->is_fully_schedulable) sched++;
                if (p->rank == 1 && p->is_fully_schedulable) {
                    r1++;
                    if (p->m_comp_lat   < mnl) mnl = p->m_comp_lat;
                    if (p->m_conn_norm  > mxc) mxc = p->m_conn_norm;
                    if (p->m_throughput > mxt) mxt = p->m_throughput;
                }
            }
            printf("  gen %2d: sched=%d r1=%d  lat=%.4f C_conn=%.4f thpt=%.2f\n",
                   gen + 1, sched, r1, mnl, mxc, mxt);
        }
    }
    *pop_out = pop;
}

static void write_extremes_block(JsonWriter *jw, const char *label,
                                 const Population *pop, int idx, GaContext *ctx)
{
    jw_key(jw, label);
    if (idx < 0) { jw_null(jw); return; }
    jw_obj_open(jw);
      jw_key(jw, "individual"); write_individual(jw, &pop->items[idx]);
      jw_key(jw, "flows");      write_flow_details(jw, &pop->items[idx], ctx);
      jw_key(jw, "topology");   write_topology_snapshot(jw, &pop->items[idx], ctx);
    jw_obj_close(jw);
}

static const char *node_type_name(const BusNode *bn) {
    return (bn && bn->is_core) ? "Master" : "Compute";
}

static const char *link_type_name(InterfaceType medium) {
    switch (medium) {
        case IF_ETHERNET:  return "光纤";
        case IF_WIFI:      return "wifi";
        case IF_BLUETOOTH: return "蓝牙";
        case IF_5G:        return "5G";
        case IF_USB:       return "USB";
        case IF_CAN:       return "CAN";
        default:           return "星闪";
    }
}

static void fmt_num(char *buf, size_t cap, double v) {
    if (isnan(v) || isinf(v)) snprintf(buf, cap, "null");
    else snprintf(buf, cap, "%.2f", v);
}

static void fmt_percent(char *buf, size_t cap, double v) {
    if (isnan(v) || isinf(v)) snprintf(buf, cap, "null");
    else snprintf(buf, cap, "%.2f %%", v * 100.0);
}

static void fmt_gbps(char *buf, size_t cap, double byte_per_ms) {
    if (isnan(byte_per_ms) || isinf(byte_per_ms)) snprintf(buf, cap, "null");
    else snprintf(buf, cap, "%.6f Gbps", byte_per_ms * 8e-6);
}

static void write_output_nodes(JsonWriter *jw, const BusTopology *topo,
                               const GaContext *ctx)
{
    jw_key(jw, "nodes");
    jw_arr_open(jw);
    for (int i = 0; i < topo->node_count; i++) {
        const BusNode *bn = &topo->nodes[i];
        const PhysicalNode *pn = &ctx->sb->p_nodes[bn->physical_node_idx];
        char dev_name[SB_STR_LEN];
        const char *src_name = pn->meta.identity.deviceName;
        if (src_name[0]) {
            snprintf(dev_name, sizeof(dev_name), "%s", src_name);
        } else {
            snprintf(dev_name, sizeof(dev_name), "Node-%02d", pn->id);
        }

        jw_obj_open(jw);
          jw_kv_int(jw, "id", pn->id);
          jw_kv_dbl(jw, "x", pn->x);
          jw_kv_dbl(jw, "y", pn->y);
          jw_kv_dbl(jw, "cpu_capacity", pn->cpu_capacity);
          jw_kv_dbl(jw, "memory_mb", pn->memory_capacity);
          jw_kv_int(jw, "max_ports", pn->max_physical_ports);
          jw_kv_dbl(jw, "reliability", pn->reliability);
          jw_kv_str(jw, "device_name", dev_name);
          jw_kv_str(jw, "static_type", node_type_name(bn));
        jw_obj_close(jw);
    }
    jw_arr_close(jw);
}

static void write_output_links(JsonWriter *jw, const BusTopology *topo,
                               const GaContext *ctx)
{
    jw_key(jw, "links");
    jw_arr_open(jw);
    for (int i = 0; i < topo->link_count; i++) {
        const BusLink *bl = &topo->links[i];
        const PhysicalLink *pl = &ctx->sb->c_links[bl->physical_link_idx];
        jw_obj_open(jw);
          jw_kv_int(jw, "id", pl->id);
          jw_kv_int(jw, "node_a", pl->node_a_id);
          jw_kv_int(jw, "node_b", pl->node_b_id);
          jw_kv_dbl(jw, "bandwidth", pl->bandwidth);
          jw_kv_dbl(jw, "propagation_delay", pl->propagation_delay);
          jw_kv_dbl(jw, "reliability", pl->reliability);
          jw_kv_dbl(jw, "cost", pl->cost);
          jw_kv_str(jw, "type", link_type_name(pl->medium));
        jw_obj_close(jw);
    }
    jw_arr_close(jw);
}

static void write_output_flows(JsonWriter *jw, const BusTopology *topo,
                               const Individual *ind)
{
    char buf[64];
    jw_key(jw, "flows");
    jw_arr_open(jw);
    for (int i = 0; i < topo->flow_count; i++) {
        const Flow *f = &topo->flows[i];
        jw_obj_open(jw);
          jw_kv_int(jw, "id", f->id);
          jw_kv_str(jw, "name", f->flow_name);
          jw_kv_int(jw, "src", f->source_node_id);
          jw_kv_int(jw, "dst", f->target_node_id);
          jw_kv_int_arr(jw, "routing_path", f->routing_path, f->path_len);
          jw_kv_int(jw, "priority", f->priority);
          jw_kv_dbl(jw, "period", f->period);
          jw_kv_dbl(jw, "deadline", f->deadline);
          jw_kv_dbl(jw, "message_size", f->message_size);
          jw_kv_dbl(jw, "reliability_req", f->reliability_requirement);

          fmt_num(buf, sizeof(buf), f->deadline > 0.0 ? f->worst_case_delay / f->deadline : 0.0);
          jw_kv_str(jw, "comp_lat", buf);
          fmt_percent(buf, sizeof(buf), ind->m_conn_norm);
          jw_kv_str(jw, "C_conn_norm", buf);
          fmt_gbps(buf, sizeof(buf), f->effective_throughput);
          jw_kv_str(jw, "E_throughput", buf);
          fmt_percent(buf, sizeof(buf), f->actual_reliability);
          jw_kv_str(jw, "comp_rel", buf);
          fmt_num(buf, sizeof(buf), ind->m_cost);
          jw_kv_str(jw, "cost", buf);
        jw_obj_close(jw);
    }
    jw_arr_close(jw);
}

static void write_output_assess_data(JsonWriter *jw, const Individual *ind) {
    char buf[64];
    jw_key(jw, "assess_data");
    jw_obj_open(jw);
      fmt_num(buf, sizeof(buf), ind->m_comp_lat);
      jw_kv_str(jw, "comp_lat", buf);
      fmt_percent(buf, sizeof(buf), ind->m_conn_norm);
      jw_kv_str(jw, "C_conn_norm", buf);
      fmt_gbps(buf, sizeof(buf), ind->m_throughput);
      jw_kv_str(jw, "E_throughput", buf);
      fmt_percent(buf, sizeof(buf), ind->m_comp_rel);
      jw_kv_str(jw, "comp_rel", buf);
      fmt_num(buf, sizeof(buf), ind->m_cost);
      jw_kv_str(jw, "cost", buf);
      fmt_percent(buf, sizeof(buf), ind->m_min_satisfaction);
      jw_kv_str(jw, "data1", buf);
      fmt_num(buf, sizeof(buf), ind->m_max_lat);
      jw_kv_str(jw, "data2", buf);
    jw_obj_close(jw);
}

static void write_output_ga_params(JsonWriter *jw) {
    jw_key(jw, "ga_params");
    jw_obj_open(jw);
      jw_kv_int(jw, "pop_size", GA_POP_SIZE);
      jw_kv_int(jw, "max_gen", GA_MAX_GEN);
      jw_kv_dbl(jw, "mutation_rate", 0.01);
    jw_obj_close(jw);
}

static void write_output_mac_params(JsonWriter *jw, const MacParams *mac) {
    jw_key(jw, "mac_params");
    jw_obj_open(jw);
      jw_kv_dbl(jw, "sigma_us", mac->sigma_us);
      jw_kv_dbl(jw, "sifs_us", mac->sifs_us);
      jw_kv_dbl(jw, "difs_us", mac->difs_us);
      jw_kv_dbl(jw, "ack_us", mac->ack_us);
      jw_kv_dbl(jw, "header_us", mac->header_us);
      jw_kv_dbl(jw, "p_cap", mac->p_cap);
      jw_kv_dbl(jw, "p_e_base", mac->p_e_base);
    jw_obj_close(jw);
}

static int write_config_style_output(const char *path, const char *name,
                                     uint64_t seed, const Individual *ind,
                                     GaContext *ctx)
{
    static BusTopology topo;
    Individual evaluated = *ind;
    ga_evaluate(&evaluated, ctx);
    ga_decode_to_topology(&evaluated, ctx->sb, &topo);
    sb_router_route_all(&ctx->router, &topo);

    CpaAnalyzer cpa;
    sb_cpa_init(&cpa, &topo);
    sb_cpa_analyze(&cpa);
    sb_cpa_write_back(&cpa);
    sb_reliability_evaluate_all(&topo);

    ThroughputResult tput;
    sb_throughput_evaluate(&topo, &ctx->mac, &tput);

    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    JsonWriter jw;
    jw_init(&jw, fp);

    jw_obj_open(&jw);
      jw_kv_str(&jw, "sandbox_name", name);
      jw_kv_int(&jw, "rng_seed", (long long)seed);
      write_output_nodes(&jw, &topo, ctx);
      write_output_links(&jw, &topo, ctx);
      write_output_flows(&jw, &topo, &evaluated);
      write_output_assess_data(&jw, &evaluated);
      write_output_ga_params(&jw);
      write_output_mac_params(&jw, &ctx->mac);
    jw_obj_close(&jw);

    jw_done(&jw);
    fclose(fp);
    return 0;
}

/* ============================================================
 * main
 * ============================================================ */
int main(int argc, char **argv) {
    /* 命令行用法：
     *   ./main_full [seed] [out.json] [-c config.json]
     *   或：./main_full -c config.json [seed] [out.json]
     */
    uint64_t seed = 42ULL;
    const char *out_path = "experiment.json";
    const char *cfg_path = NULL;

    int pos = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-c") && i + 1 < argc) {
            cfg_path = argv[++i];
        } else if (pos == 0) {
            seed = (uint64_t)atoll(argv[i]); pos++;
        } else if (pos == 1) {
            out_path = argv[i]; pos++;
        }
    }

    ga_seed_rng(seed);

    static Sandbox sb;
    SandboxConfig cfg;
    if (cfg_path) {
        int rc = sb_load_config(cfg_path, &cfg, &sb);
        if (rc != 0) {
            fprintf(stderr, "[config] failed to load %s (rc=%d)\n", cfg_path, rc);
            return 1;
        }
        printf("[config] loaded: %s\n", cfg_path);
        sb_config_print(&cfg, &sb);
        /* 配置里的 seed 覆盖命令行（除非命令行已显式提供） */
        if (pos == 0) {   /* 命令行没指定 seed */
            seed = cfg.rng_seed;
            ga_seed_rng(seed);
        }
    } else {
        sb_build_sandbox(&sb);
        printf("[config] using built-in default sandbox\n");
    }

    GaContext ctx;
    ctx.sb = &sb;
    sb_router_init(&ctx.router);
    if (cfg_path) ctx.mac = cfg.mac;
    else          sb_mac_params_default(&ctx.mac);
    ctx.verbose = 1;
    sb_phys_bind(sb.p_nodes, sb.c_links);

    /* 连通度归一化基线 */
    {
        static BusTopology baseline_topo;
        Individual all_on;
        ga_individual_init_empty(&all_on, sb.b_node_count, sb.c_link_count);
        for (int i = 0; i < sb.b_node_count; i++)  all_on.role_gene[i] = 1;
        for (int j = 0; j < sb.c_link_count; j++)  all_on.link_gene[j] = 1;
        ga_decode_to_topology(&all_on, &sb, &baseline_topo);
        ctx.lambda_base = sb_connectivity_evaluate(&baseline_topo);
        printf("[baseline] lambda_2 (all-active) = %.6f\n", ctx.lambda_base);
    }

    FILE *fp = tmpfile();
    if (!fp) {
#ifdef _WIN32
        fp = fopen("NUL", "w");
#else
        fp = fopen("/dev/null", "w");
#endif
    }
    if (!fp) { fprintf(stderr, "open temporary JSON sink failed\n"); return 1; }
    JsonWriter jw; jw_init(&jw, fp);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    /* ---- JSON 根 ---- */
    jw_obj_open(&jw);

    /* config */
    jw_key(&jw, "config");
    jw_obj_open(&jw);
      jw_kv_int(&jw, "rng_seed",            (long long)seed);
      jw_kv_int(&jw, "pop_size",            GA_POP_SIZE);
      jw_kv_int(&jw, "max_gen_normal",      GA_MAX_GEN);
      jw_kv_int(&jw, "max_gen_reconstruct", GA_MAX_GEN);
      jw_kv_dbl(&jw, "mutation_rate",       0.01);
      jw_kv_int(&jw, "n_physical_nodes",    sb.p_node_count);
      jw_kv_int(&jw, "n_candidate_links",   sb.c_link_count);
      jw_kv_int(&jw, "n_flows",             sb.flow_graph.count);
    jw_obj_close(&jw);

    /* ============================================================
     * 阶段 1：baseline seed
     * ============================================================ */
    Individual base_seed;
    ga_create_baseline_seed(&base_seed, &sb);
    ga_evaluate(&base_seed, &ctx);
    printf("\n=== Baseline seed ===\n");
    printf("[seed] sched=%d lat=%.4f C_conn=%.4f thpt=%.2f rel=%.4f cost=%.2f\n",
           base_seed.is_fully_schedulable, base_seed.m_comp_lat,
           base_seed.m_conn_norm, base_seed.m_throughput,
           base_seed.m_comp_rel, base_seed.m_cost);

    jw_key(&jw, "seed"); write_individual(&jw, &base_seed);

    jw_key(&jw, "snapshots_seed_topology");
    write_topology_snapshot(&jw, &base_seed, &ctx);

    /* ============================================================
     * 阶段 2：常规演化 50 代
     * ============================================================ */
    printf("\n=== Normal evolution (%d generations) ===\n", GA_MAX_GEN);
    jw_key(&jw, "normal_evolution");
    jw_arr_open(&jw);
    static Population pop_normal;
    run_with_stats(&ctx, &base_seed, GA_MAX_GEN, &pop_normal, &jw);
    jw_arr_close(&jw);

    /* normal_final：5 个极值方案 */
    int idx_lat, idx_conn, idx_tp, idx_rel, idx_cost;
    find_pareto_extremes(&pop_normal,
                         &idx_lat, &idx_conn, &idx_tp, &idx_rel, &idx_cost);

    jw_key(&jw, "normal_final");
    jw_obj_open(&jw);
      write_extremes_block(&jw, "min_latency",     &pop_normal, idx_lat,  &ctx);
      write_extremes_block(&jw, "max_connectivity",&pop_normal, idx_conn, &ctx);
      write_extremes_block(&jw, "max_throughput",  &pop_normal, idx_tp,   &ctx);
      write_extremes_block(&jw, "max_reliability", &pop_normal, idx_rel,  &ctx);
      write_extremes_block(&jw, "min_cost",        &pop_normal, idx_cost, &ctx);
    jw_obj_close(&jw);

    printf("\nNormal phase done. Pareto extremes:\n");
    if (idx_lat  >= 0) printf("  [min_lat]    lat=%.4f max=%.2f\n",
                              pop_normal.items[idx_lat].m_comp_lat,
                              pop_normal.items[idx_lat].m_max_lat);
    if (idx_conn >= 0) printf("  [max_conn]   C_conn=%.4f\n",
                              pop_normal.items[idx_conn].m_conn_norm);
    if (idx_tp   >= 0) printf("  [max_thpt]   thpt=%.2f\n",
                              pop_normal.items[idx_tp].m_throughput);
    if (idx_rel  >= 0) printf("  [max_rel]    rel=%.4f\n",
                              pop_normal.items[idx_rel].m_comp_rel);
    if (idx_cost >= 0) printf("  [min_cost]   cost=%.2f\n",
                              pop_normal.items[idx_cost].m_cost);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double normal_ms = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1e6;

    jw_obj_close(&jw);   /* 根对象 */
    jw_done(&jw);
    fclose(fp);

    const Individual *normal_final_out = (idx_lat >= 0) ? &pop_normal.items[idx_lat] : &base_seed;
    const char *normal_final_name = (idx_lat >= 0) ? "optimized_normal_topology" : "baseline_topology";
    if (write_config_style_output(out_path, normal_final_name, seed, normal_final_out, &ctx) != 0) {
        fprintf(stderr, "write %s failed\n", out_path);
        return 1;
    }

    printf("\n=== Config JSON written to: %s ===\n", out_path);
    printf("Total time: %.1f ms (%.1f s)\n", normal_ms, normal_ms / 1000.0);
    return 0;

    /* ============================================================
     * 阶段 3：故障注入 + 即时评估
     * ============================================================ */
    int failed_node = 15;
    printf("\n=== Disaster injection: node %d failure ===\n", failed_node);

    /* 复制最优时延解，作为损毁版基础 */
    Individual damaged;
    if (idx_lat >= 0) ga_inject_node_failure(&pop_normal.items[idx_lat], &sb,
                                              failed_node, &damaged);
    else              ga_inject_node_failure(&base_seed, &sb,
                                              failed_node, &damaged);

    int n_cascading = 0;
    for (int j = 0; j < sb.c_link_count; j++)
        if (damaged.link_gene[j] == -1) n_cascading++;

    int n_stripped = ga_strip_flows_for_failed_node(&sb, failed_node);
    printf("  cascading links destroyed: %d\n", n_cascading);
    printf("  flows annihilated: %d\n", n_stripped);

    /* 即时评估损毁版 */
    ga_evaluate(&damaged, &ctx);

    jw_key(&jw, "disaster");
    jw_obj_open(&jw);
      jw_kv_str(&jw, "type",          "node");
      jw_kv_int(&jw, "target_id",     failed_node);
      jw_kv_int(&jw, "cascading_links_destroyed", n_cascading);
      jw_kv_int(&jw, "flows_annihilated_count",   n_stripped);
      jw_kv_int(&jw, "remaining_flow_count",      sb.flow_graph.count);
      jw_key(&jw, "post_disaster_individual"); write_individual(&jw, &damaged);
      jw_key(&jw, "post_disaster_flows");      write_flow_details(&jw, &damaged, &ctx);
      jw_key(&jw, "post_disaster_topology");   write_topology_snapshot(&jw, &damaged, &ctx);
    jw_obj_close(&jw);

    printf("  damaged instant: sched=%d lat=%.4f thpt=%.2f cost=%.2f\n",
           damaged.is_fully_schedulable, damaged.m_comp_lat,
           damaged.m_throughput, damaged.m_cost);

    /* ============================================================
     * 阶段 4：重建演化 50 代
     * ============================================================ */
    printf("\n=== Reconstruction evolution (%d generations) ===\n", GA_MAX_GEN);
    jw_key(&jw, "reconstruct_evolution");
    jw_arr_open(&jw);
    static Population pop_recon;
    run_with_stats(&ctx, &damaged, GA_MAX_GEN, &pop_recon, &jw);
    jw_arr_close(&jw);

    int r_lat, r_conn, r_tp, r_rel, r_cost;
    find_pareto_extremes(&pop_recon, &r_lat, &r_conn, &r_tp, &r_rel, &r_cost);

    jw_key(&jw, "reconstruct_final");
    jw_obj_open(&jw);
      write_extremes_block(&jw, "min_latency",     &pop_recon, r_lat,  &ctx);
      write_extremes_block(&jw, "max_connectivity",&pop_recon, r_conn, &ctx);
      write_extremes_block(&jw, "max_throughput",  &pop_recon, r_tp,   &ctx);
      write_extremes_block(&jw, "max_reliability", &pop_recon, r_rel,  &ctx);
      write_extremes_block(&jw, "min_cost",        &pop_recon, r_cost, &ctx);
    jw_obj_close(&jw);

    printf("\nReconstruct phase done. Pareto extremes:\n");
    if (r_lat  >= 0) printf("  [min_lat]    lat=%.4f max=%.2f\n",
                            pop_recon.items[r_lat].m_comp_lat,
                            pop_recon.items[r_lat].m_max_lat);
    if (r_conn >= 0) printf("  [max_conn]   C_conn=%.4f\n",
                            pop_recon.items[r_conn].m_conn_norm);
    if (r_tp   >= 0) printf("  [max_thpt]   thpt=%.2f\n",
                            pop_recon.items[r_tp].m_throughput);
    if (r_rel  >= 0) printf("  [max_rel]    rel=%.4f\n",
                            pop_recon.items[r_rel].m_comp_rel);
    if (r_cost >= 0) printf("  [min_cost]   cost=%.2f\n",
                            pop_recon.items[r_cost].m_cost);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1e6;

    jw_kv_dbl(&jw, "total_time_ms", ms);
    jw_obj_close(&jw);   /* 根对象 */
    jw_done(&jw);
    fclose(fp);

    const Individual *final_out = NULL;
    const char *final_name = "optimized_reconstruct_topology";
    if (r_lat >= 0) {
        final_out = &pop_recon.items[r_lat];
    } else if (idx_lat >= 0) {
        final_out = &pop_normal.items[idx_lat];
        final_name = "optimized_normal_topology";
    } else {
        final_out = &base_seed;
        final_name = "baseline_topology";
    }

    if (write_config_style_output(out_path, final_name, seed, final_out, &ctx) != 0) {
        fprintf(stderr, "write %s failed\n", out_path);
        return 1;
    }

    printf("\n=== Config JSON written to: %s ===\n", out_path);
    printf("Total time: %.1f ms (%.1f s)\n", ms, ms / 1000.0);
    return 0;
}
