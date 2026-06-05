#include "throughput.h"
#include "sb_phys_bind.h"
#include <math.h>
#include <string.h>

/* ============================================================
 *  MAC 参数缺省（典型 802.11g 值）
 * ============================================================ */
void sb_mac_params_default(MacParams *p) {
    p->sigma_us       = 9.0;
    p->sifs_us        = 10.0;
    p->difs_us        = 28.0;
    p->ack_us         = 24.0;
    p->header_us      = 20.0;
    p->tau_prop_us    = 1.0;
    p->ack_timeout_us = 50.0;

    p->p_cap          = 0.0;
    p->p_e_base       = 0.01;

    p->tau_init       = 0.05;
    p->tau_max        = 0.5;
    p->max_iter       = 60;
    p->tol            = 1e-5;
}

/* ============================================================
 *  链路级 MAC 公式求解
 *
 *  完整套用方案 5.1.2 节公式：
 *    Pt = 1 - (1-τ)^N
 *    Ps = (N τ (1-τ)^(N-1) + Pcap) / Pt
 *    Tc = H + PL + ACK_timeout
 *    Te = H + PL + ACK_timeout
 *    Ts = H + PL + SIFS + τp + ACK + DIFS
 *    E[Sts] = (1-Pt)σ + Pt(1-Ps)Tc + Pt·Ps(1-Pe)Ts + Pt·Ps·Pe·Te
 *    q = 1 - exp(-λ · E[Sts])
 *    τ_new = min(τ_max, q)   [归一化为单时隙发送概率]
 *
 *  S（归一化吞吐量，无量纲）= Pt·Ps·(1-Pe)·E[PL] / E[Sts]
 *  最终返回 byte/ms。E[PL] 单位 bytes，E[Sts] 单位 μs，所以 S [byte/μs]
 *  乘 1000 得到 byte/ms。
 * ============================================================ */
static void mac_solve_iter(const MacParams *p,
                           int N, double lambda_byte_per_us,
                           double payload_bytes,
                           /* out */ double *out_S_byte_per_ms,
                           /* out */ double *out_tau,
                           /* out */ double *out_Pt,
                           /* out */ double *out_Ps,
                           /* out */ double *out_E_Sts)
{
    double Tc = p->header_us + payload_bytes + p->ack_timeout_us;
    double Te = p->header_us + payload_bytes + p->ack_timeout_us;
    double Ts = p->header_us + payload_bytes
              + p->sifs_us + p->tau_prop_us + p->ack_us + p->difs_us;

    double tau   = p->tau_init;
    double Pe    = p->p_e_base;
    double Pcap  = p->p_cap;

    /* 退化：N <= 1（单设备占用）→ 无碰撞 */
    if (N <= 1) {
        double Pt = 1.0 - pow(1.0 - tau, N <= 0 ? 1 : N);
        double Ps = 1.0;
        double E_Sts = (1.0 - Pt) * p->sigma_us
                     + Pt * (1.0 - Ps) * Tc
                     + Pt * Ps * (1.0 - Pe) * Ts
                     + Pt * Ps * Pe * Te;
        double S = (E_Sts > 1e-12)
                   ? Pt * Ps * (1.0 - Pe) * payload_bytes / E_Sts
                   : 0.0;
        *out_S_byte_per_ms = S * 1000.0;   /* byte/μs -> byte/ms */
        if (out_tau)   *out_tau   = tau;
        if (out_Pt)    *out_Pt    = Pt;
        if (out_Ps)    *out_Ps    = Ps;
        if (out_E_Sts) *out_E_Sts = E_Sts;
        return;
    }

    /* 不动点迭代：τ ↔ q ↔ E[Sts] ↔ Pt/Ps */
    double Pt = 0, Ps = 0, E_Sts = 0, q;
    for (int iter = 0; iter < p->max_iter; iter++) {
        Pt = 1.0 - pow(1.0 - tau, N);
        if (Pt < 1e-12) Pt = 1e-12;

        double one_minus_tau_pow = pow(1.0 - tau, N - 1);
        Ps = (N * tau * one_minus_tau_pow + Pcap) / Pt;
        if (Ps > 1.0) Ps = 1.0;
        if (Ps < 0.0) Ps = 0.0;

        E_Sts = (1.0 - Pt) * p->sigma_us
              + Pt * (1.0 - Ps) * Tc
              + Pt * Ps * (1.0 - Pe) * Ts
              + Pt * Ps * Pe * Te;
        if (E_Sts < 1e-9) E_Sts = 1e-9;

        /* q = 1 - exp(-λ · E[Sts])：到达率 λ × 时隙时间得平均到达包数；
         * 队列至少有一个包的概率 = 1 - exp(-平均到达数)
         * λ 单位 byte/μs（聚合到达流量），但 q 是概率密度对应于"每时隙到达概率"。
         * 这里把 λ 按 packet/μs 处理：λ_pkt = λ_byte / payload_bytes */
        double lambda_pkt = (payload_bytes > 0)
                          ? lambda_byte_per_us / payload_bytes : 0.0;
        q = 1.0 - exp(-lambda_pkt * E_Sts);
        if (q > 1.0) q = 1.0;
        if (q < 0.0) q = 0.0;

        /* τ_new = q（业务驱动） */
        double tau_new = q;
        if (tau_new > p->tau_max) tau_new = p->tau_max;
        if (tau_new < 1e-6)       tau_new = 1e-6;

        if (fabs(tau_new - tau) < p->tol) {
            tau = tau_new;
            break;
        }
        tau = tau_new;
    }

    /* 最终归一化吞吐 S [byte/μs] */
    double S_byte_per_us = (E_Sts > 1e-12)
        ? (Pt * Ps * (1.0 - Pe) * payload_bytes / E_Sts) : 0.0;

    *out_S_byte_per_ms = S_byte_per_us * 1000.0;
    if (out_tau)   *out_tau   = tau;
    if (out_Pt)    *out_Pt    = Pt;
    if (out_Ps)    *out_Ps    = Ps;
    if (out_E_Sts) *out_E_Sts = E_Sts;
}

double sb_mac_link_throughput(const MacParams *p,
                              int N, double lambda_byte_per_us,
                              double payload_bytes)
{
    double S; double tau, Pt, Ps, E_Sts;
    mac_solve_iter(p, N, lambda_byte_per_us, payload_bytes,
                   &S, &tau, &Pt, &Ps, &E_Sts);
    return S;
}

/* ============================================================
 *  辅助：估计争用域 N
 *
 *  软总线场景下，争用域可由链路的物理介质类型决定：
 *    - 以太网点对点（IF_ETHERNET / IF_SERIAL_RS422 / IF_USB / IF_CAN）：N=2
 *    - 无线广播（IF_WIFI / IF_5G / IF_BLUETOOTH）：N = a 节点的链路数 + b 节点的链路数
 *      （近似为两端节点的邻居链路并集，反映共享同一射频信道）
 *
 *  在配置文件中可被覆盖（每条链路指定 contention_n）。
 * ============================================================ */
static int estimate_contention_N(const BusTopology *topo,
                                 const BusLink *bl,
                                 const PhysicalLink *pl)
{
    InterfaceType m = pl->medium;
    /* 点对点有线 */
    if (m == IF_ETHERNET || m == IF_SERIAL_RS422 ||
        m == IF_USB || m == IF_CAN) {
        return 2;
    }
    /* 无线 / 共享介质 */
    int a_idx = topo->node_id2idx[bl->node_a_id];
    int b_idx = topo->node_id2idx[bl->node_b_id];
    int Na = (a_idx >= 0) ? topo->nodes[a_idx].link_count : 1;
    int Nb = (b_idx >= 0) ? topo->nodes[b_idx].link_count : 1;
    int N = Na + Nb;
    if (N < 2) N = 2;
    return N;
}

/* ============================================================
 *  估计链路总到达率 λ（byte/μs）= Σ_{f 经过该链路} (msg_size_k / period_k)
 *
 *  period 单位 ms → 转换为 μs（× 1000）
 *  msg_size 单位 byte
 *  λ_k = msg_size_k / (period_k × 1000)  [byte/μs]
 * ============================================================ */
static double estimate_link_arrival_rate(const BusTopology *topo,
                                         int link_idx)
{
    const BusLink *bl = &topo->links[link_idx];
    double lambda = 0.0;
    for (int i = 0; i < bl->passing_count; i++) {
        int fi = bl->passing_flows[i];
        if (fi < 0 || fi >= topo->flow_count) continue;
        const Flow *f = &topo->flows[fi];
        if (f->period <= 0) continue;
        lambda += f->message_size / (f->period * 1000.0);   /* byte/μs */
    }
    return lambda;
}

/* ============================================================
 *  估计加权平均报文大小 payload_bytes
 *  E[PL] = Σ λ_k × msg_size_k / Σ λ_k
 * ============================================================ */
static double estimate_link_avg_payload(const BusTopology *topo,
                                        int link_idx)
{
    const BusLink *bl = &topo->links[link_idx];
    double num = 0.0, den = 0.0;
    for (int i = 0; i < bl->passing_count; i++) {
        int fi = bl->passing_flows[i];
        if (fi < 0 || fi >= topo->flow_count) continue;
        const Flow *f = &topo->flows[fi];
        if (f->period <= 0) continue;
        double rate = f->message_size / (f->period * 1000.0);
        num += rate * f->message_size;
        den += rate;
    }
    return (den > 1e-12) ? num / den : 0.0;
}

/* ============================================================
 *  系统级吞吐量评估
 * ============================================================ */
void sb_throughput_evaluate(BusTopology *topo,
                            const MacParams *p,
                            ThroughputResult *out)
{
    memset(out, 0, sizeof(*out));

    /* Step 1: 每条激活链路的 MAC 层吞吐 */
    for (int li = 0; li < topo->link_count; li++) {
        const BusLink *bl = &topo->links[li];
        const PhysicalLink *pl = sb_phys_link(bl->physical_link_idx);
        if (!pl) { out->link_throughput[li] = 0.0; continue; }

        int N = estimate_contention_N(topo, bl, pl);
        double lambda = estimate_link_arrival_rate(topo, li);
        double payload = estimate_link_avg_payload(topo, li);

        /* 若没有业务流经过该链路（lambda=0），用其物理带宽作为容量上限
         * （此时 MAC 模型退化）。我们直接取 PhysicalLink.bandwidth */
        double mac_S;
        if (lambda < 1e-9 || payload < 1e-9) {
            mac_S = pl->bandwidth;
        } else {
            mac_S = sb_mac_link_throughput(p, N, lambda, payload);
            /* MAC 层不能超过物理带宽上限 */
            if (mac_S > pl->bandwidth) mac_S = pl->bandwidth;
        }
        out->link_throughput[li] = mac_S;
    }

    /* Step 2 & 3: 每条业务流的路径瓶颈吞吐 + 有效吞吐 */
    double sys_num = 0.0, sys_den = 0.0;
    double min_sat = 1.0;

    for (int fi = 0; fi < topo->flow_count; fi++) {
        Flow *f = &topo->flows[fi];

        /* 路径瓶颈 */
        double bottleneck = INFINITY;
        if (f->path_len < 2) {
            bottleneck = 0.0;   /* 无路径 */
        } else {
            for (int k = 0; k < f->path_len - 1; k++) {
                int u_id = f->routing_path[k];
                int v_id = f->routing_path[k + 1];
                BusLink *bl = sb_topology_get_link(topo, u_id, v_id);
                if (!bl) { bottleneck = 0.0; break; }
                int li2 = (int)(bl - topo->links);   /* 由指针推回 link_idx */
                if (out->link_throughput[li2] < bottleneck)
                    bottleneck = out->link_throughput[li2];
            }
        }
        out->flow_path_bottleneck[fi] = bottleneck;

        /* 业务流需求（byte/ms）：msg_size / period */
        double req = (f->period > 0) ? (f->message_size / f->period) : 0.0;
        out->flow_requirement[fi] = req;

        /* 有效吞吐 = min(瓶颈, 需求) */
        double eff = (bottleneck < req) ? bottleneck : req;
        if (eff < 0) eff = 0;
        out->flow_effective[fi] = eff;

        /* 写回 Flow */
        f->effective_throughput = eff;
        f->throughput_requirement = req;

        /* 系统级加权 */
        double w = (double)f->priority;
        sys_num += w * eff;
        sys_den += w;

        /* 满意度 = eff / req */
        if (req > 1e-9) {
            double sat = eff / req;
            if (sat < min_sat) min_sat = sat;
        }
    }

    out->system_weighted_throughput =
        (sys_den > 1e-9) ? (sys_num / sys_den) : 0.0;
    out->system_min_satisfaction = min_sat;
}
