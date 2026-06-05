#ifndef SB_GA_H
#define SB_GA_H

#include "sb_models.h"
#include "initializer.h"
#include "routing.h"
#include "throughput.h"

/* ============================================================
 * NSGA-II 主循环 —— 与 Python simulation/ga_core.py 对齐
 *
 * 个体编码：role_gene[N_nodes] + link_gene[N_links]
 * 适应度（5 维最小化）：
 *   (comp_lat, -C_conn_norm, -E_throughput, -comp_rel, cost)
 * 硬约束：6 条 validate_topology 规则
 * 操作：tournament_select(2) + 均匀交叉 + 位翻转变异(0.01) + Pareto 排序 + 拥挤度
 * ============================================================ */

#define GA_OBJ_COUNT    5
#define GA_POP_SIZE     100
#define GA_MAX_GEN      50

typedef struct {
    int    role_gene[SB_MAX_NODES];   /* 1=core, 0=edge, -1=destroyed */
    int    link_gene[SB_MAX_LINKS];   /* 1=active, 0=inactive, -1=destroyed */
    int    n_roles;                   /* role_gene 实际长度 */
    int    n_links;                   /* link_gene 实际长度 */

    /* 适应度（最小化） */
    double fitness[GA_OBJ_COUNT];
    int    rank;                      /* Pareto 前沿层级，1 最优 */
    double crowding_distance;

    /* metrics（保留物理含义未取反） */
    double m_comp_lat;
    double m_max_lat;
    double m_fiedler;          /* 原始 λ_2 */
    double m_conn_norm;        /* 归一化 C_conn = λ_2 / λ_2_base */
    double m_throughput;       /* 系统级加权吞吐 byte/ms */
    double m_min_satisfaction; /* min(eff_k / req_k) */
    double m_comp_rel;
    double m_cost;

    int    is_fully_schedulable;
    int    evaluated;                 /* 是否已被 evaluate_individual 计算 */
} Individual;

typedef struct {
    Individual items[GA_POP_SIZE * 4];   /* 最大容量：父+子+一些缓冲 */
    int        count;
    int        capacity;
} Population;

/* GA 运行所需的全局上下文（沙箱 + 路由器），不内置 RNG */
typedef struct {
    const Sandbox      *sb;
    DeterministicRouter router;

    /* 评估器配置 */
    MacParams mac;                /* MAC 层吞吐量参数 */
    double    lambda_base;        /* 连通度归一化基线 */

    int       verbose;
} GaContext;

/* ---------- 随机数 ---------- */
void   ga_seed_rng(uint64_t seed);
double ga_rand_uniform(void);        /* [0,1) */
int    ga_rand_int(int n);           /* [0, n) */

/* ---------- 个体级操作 ---------- */
void ga_individual_init_empty(Individual *ind, int n_roles, int n_links);

/* 解码 + 验证（不会修改 ind） */
int  ga_decode_to_topology(const Individual *ind, const Sandbox *sb,
                           BusTopology *out_topo);
int  ga_validate_topology (const BusTopology *topo, const Sandbox *sb);

/* 创建基线种子（MST 构造），role/link gene 写入 seed */
void ga_create_baseline_seed(Individual *seed, const Sandbox *sb);

/* 评估单个个体：调用 decode + route + 4 项指标，结果写入 ind */
void ga_evaluate(Individual *ind, GaContext *ctx);

/* 支配 / 排序 / 拥挤度 */
int  ga_dominates(const Individual *p, const Individual *q);
void ga_fast_non_dominated_sort(Population *pop);   /* 写回 ind.rank，并按 rank 重排 */
void ga_calc_crowding_for_rank  (Population *pop, int target_rank);

/* 选择 / 交叉 / 变异 */
Individual *ga_tournament_select(Population *pop, int tournament_size);
void ga_crossover(const Individual *p1, const Individual *p2,
                  Individual *c1, Individual *c2);
void ga_mutate   (Individual *ind, double rate);

/* 子代生成（含 immigrant） */
void ga_generate_offspring(Population *parents, const Individual *seed,
                           GaContext *ctx, Population *offspring,
                           int pop_size);

/* 环境选择：合并、去重、Pareto 排序、按拥挤度截取 pop_size */
void ga_environmental_select(Population *parents, Population *offspring,
                             Population *next_gen, int pop_size);

/* ---------- 主循环 ---------- */

/* 通用：从给定种子开始演化 max_gen 代 */
void ga_run_from_seed(GaContext *ctx, const Individual *seed,
                      int max_gen, Population *pop_out);

/* 默认：构造 baseline seed 演化 GA_MAX_GEN 代 */
void ga_run(GaContext *ctx, Population *pop_out);

/* ---------- 故障注入 ----------
 * 把 base 复制到 out，并把 failed_node_id 标记为损毁：
 *   - out->role_gene[failed_node_id] = -1
 *   - 与 failed_node_id 相连的所有 link_gene[j] = -1（覆盖原值）
 * 调用方在调用前应自行处理 sb->flow_graph 中的相关流。
 */
void ga_inject_node_failure(const Individual *base, const Sandbox *sb,
                            int failed_node_id, Individual *out);

/* 从 sb->flow_graph 中剔除源/目的为 failed_node_id 的流（就地修改）；
 * 返回被剔除的流数目 */
int  ga_strip_flows_for_failed_node(Sandbox *sb, int failed_node_id);

#endif
