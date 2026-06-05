#ifndef SB_THROUGHPUT_H
#define SB_THROUGHPUT_H

#include "sb_models.h"

/* ============================================================
 *  软总线吞吐量评估（MAC 层概率模型 + 路径瓶颈 + 系统级加权）
 *
 *  实现方案文档 5.1.2 节的完整 MAC 层公式：
 *
 *    S = [Pt × Ps × (1 - Pe) × E[PL]]
 *      / [(1 - Pt) σ + Pt(1 - Ps)Tc + Pt·Ps(1 - Pe)Ts + Pt·Ps·Pe·Te]
 *
 *  其中：
 *    Pt = 1 - (1 - τ)^N
 *    Ps = (N τ (1 - τ)^(N-1) + Pcap) / Pt
 *    τ  ← 不动点迭代：τ = min(τ_max, q · λ · E[Sts])
 *    q  = 1 - exp(-λ · E[Sts])
 *
 *  在仿真中，τ 不是独立参数，而是与到达率 λ、平均时隙 E[Sts] 形成
 *  循环依赖（τ → Pt → E[Sts] → q → τ），因此求解需要不动点迭代。
 * ============================================================ */

/* MAC 层参数（典型 802.11 值，可被配置文件覆盖） */
typedef struct {
    /* PHY/MAC 时间参数（μs） */
    double sigma_us;           /* 空闲时隙：典型 9 μs */
    double sifs_us;            /* SIFS：典型 10 μs */
    double difs_us;            /* DIFS：典型 28-50 μs */
    double ack_us;             /* ACK 持续时间 */
    double header_us;          /* PHY+MAC 头部时间 */
    double tau_prop_us;        /* 物理传播时延 */
    double ack_timeout_us;     /* ACK 超时 */

    /* 信道概率参数 */
    double p_cap;              /* 捕获效应概率（多设备同时发仍能解出）：典型 0.0~0.5 */
    double p_e_base;           /* 信道基础误码率：典型 0.01~0.05 */

    /* τ 求解参数 */
    double tau_init;           /* τ 初值：典型 0.05 */
    double tau_max;            /* τ 上限：典型 0.5 */
    int    max_iter;           /* 不动点最大迭代次数 */
    double tol;                /* 收敛容差 */
} MacParams;

void sb_mac_params_default(MacParams *p);

/* 单链路单点估计：给定 N（争用域设备数）、λ（链路总到达率 byte/μs）、
 * payload_bytes（平均有效载荷），求解 MAC 归一化吞吐量 S（byte/ms）。
 * 内部完成 τ 不动点迭代。
 *   - 若 N <= 1：退化为纯传输极限（无碰撞）
 *   - 若 λ 趋于 0：吞吐量趋于 0
 *   - 若 λ 趋于无穷：吞吐量趋于 N=∞ 的极限
 * 返回 byte/ms 的链路 MAC 层吞吐量 S。 */
double sb_mac_link_throughput(const MacParams *p,
                              int N, double lambda_byte_per_us,
                              double payload_bytes);

/* ============================================================
 *  系统级吞吐量评估
 *
 *  流程：
 *    1) 对每条激活链路，估计争用域 N 与到达率 λ
 *    2) 调用 sb_mac_link_throughput 得到链路层 MAC 吞吐量
 *    3) 对每条业务流 f_k，路径瓶颈吞吐
 *           T_k = min{ link_throughput(e) for e in path(f_k) }
 *    4) 有效吞吐 = min(T_k, message_size_k / period_k)
 *    5) 系统级（加权）：
 *           E_throughput = Σ w_k × eff_k / Σ w_k
 *
 *  写回字段：
 *    - flow.effective_throughput      （byte/ms）
 *    - flow.throughput_requirement    （byte/ms，由 message_size/period 推算）
 * ============================================================ */
typedef struct {
    /* 系统级 */
    double system_weighted_throughput;     /* 全网加权平均吞吐 */
    double system_min_satisfaction;        /* min(eff_k / req_k)，即最差业务流满意度 */

    /* 链路级（调试用，可选保留） */
    double link_throughput[SB_MAX_LINKS];

    /* 流级（调试用，可选保留） */
    double flow_path_bottleneck[SB_MAX_FLOWS];
    double flow_effective      [SB_MAX_FLOWS];
    double flow_requirement    [SB_MAX_FLOWS];
} ThroughputResult;

void sb_throughput_evaluate(BusTopology *topo,
                            const MacParams *p,
                            ThroughputResult *out);

#endif
