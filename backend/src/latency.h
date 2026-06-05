#ifndef SB_LATENCY_H
#define SB_LATENCY_H

#include "sb_models.h"

/* ============================================================
 * CPA (Compositional Performance Analysis) 端到端最坏时延评估
 *
 * 与 Python evaluation/latency.py 等价：
 *   - 内层：solve_queueing_delay 时间步长逼近，超时 200000ms
 *   - 外层：global_delays 不动点迭代，最多 100 轮，eps=0.001
 *   - 节点角色互斥：source / sink / relay 三选一
 *   - phase 选择：source->TX, sink->RX, relay->RX+TX
 *
 * 简化点：
 *   - global_delays 用三维数组 [flow_idx][path_idx][phase] 而不是 dict
 *   - 节点角色一次性预计算成 role[flow_idx][path_idx]
 *
 * 调用前需 sb_phys_bind(p_nodes, c_links)；
 * 调用前应已完成路由（flow.routing_path 已就绪）。
 * ============================================================ */

#define CPA_PH_RX 0
#define CPA_PH_TX 1

#define CPA_ROLE_NONE   -1
#define CPA_ROLE_SOURCE  0
#define CPA_ROLE_SINK    1
#define CPA_ROLE_RELAY   2

/* INF：所有时延上界；用 DBL_MAX/4 避免后续加法溢出 */
#define CPA_INF (1e300)

typedef struct {
    /* delays[fi][k][ph]:
     *   fi = 流在 topo.flows 中的下标
     *   k  = 节点在 flow.routing_path 中的位置（0..path_len-1）
     *   ph = CPA_PH_RX / CPA_PH_TX
     *   未使用的 (fi,k,ph) 组合保持 0.0，外部访问时按 role 控制不要碰它们
     */
    double delays[SB_MAX_FLOWS][SB_MAX_PATH_HOPS][2];

    /* role[fi][k]: 流 fi 在路径第 k 跳节点上的角色（CPA_ROLE_*） */
    int    role  [SB_MAX_FLOWS][SB_MAX_PATH_HOPS];
} CpaState;

typedef struct {
    BusTopology *topo;
    CpaState     state;
    int          max_outer;
    double       outer_eps;
    double       inner_timeout;
} CpaAnalyzer;

/* 初始化（绑定 topology，重置 max_outer/outer_eps/inner_timeout 默认值） */
void   sb_cpa_init   (CpaAnalyzer *a, BusTopology *topo);

/* 跑完整个不动点；不返回值，结果写入 a->state */
void   sb_cpa_analyze(CpaAnalyzer *a);

/* 取某条流（按 topo.flows 下标）的端到端时延；过载返回 CPA_INF */
double sb_cpa_e2e_of (const CpaAnalyzer *a, int flow_idx);

/* 把结果写回 flow.worst_case_delay 和 is_schedulable
 * （注意：is_schedulable 是覆盖式赋值，对齐 Python ga_core 行为） */
void   sb_cpa_write_back(CpaAnalyzer *a);

#endif
