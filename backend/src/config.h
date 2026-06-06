#ifndef SB_CONFIG_H
#define SB_CONFIG_H

#include "sb_models.h"
#include "initializer.h"
#include "throughput.h"

/* ============================================================
 *  配置文件驱动的沙箱构造
 *
 *  替代 initializer.c 中硬编码的 20 节点 / 83 链路 / 14 业务流。
 *  通过单个 JSON 配置文件配置全部仿真输入。
 *
 *  配置文件示例（data/sandbox_default.json）：
 *  {
 *    "sandbox_name": "default_20node",
 *    "rng_seed": 42,
 *
 *    "nodes": [
 *      { "id": 0, "x": 100, "y": 200,
 *        "cpu_capacity": 6000, "memory_mb": 16384,
 *        "max_ports": 4, "reliability": 0.99,
 *        "device_name": "GW-00", "static_type": "Gateway"  // optional
 *      },
 *      ...
 *    ],
 *
 *    "link_generation": {
 *      "distance_limit_m": 400,
 *      "bandwidth_a":  100000,   // bandwidth = bw_a / (1 + (d/bw_b)^2)
 *      "bandwidth_b":  100.0,
 *      "delay_const":  0.01,     // delay = delay_const + d * delay_slope
 *      "delay_slope":  0.005,
 *      "cost_const":   10.0,     // cost  = cost_const  + d * cost_slope
 *      "cost_slope":   0.1,
 *      "reliability_base":  0.999,
 *      "reliability_decay": 0.01 // rel = base - decay * (d / dist_limit)^2
 *    },
 *
 *    "flows": [
 *      { "name": "Video_0_to_10", "src": 0, "dst": 10,
 *        "priority": 1, "period": 33, "deadline": 300,
 *        "message_size": 64000, "reliability_req": 0.80 },
 *      ...
 *    ],
 *
 *    "ga_params": {
 *      "pop_size": 100, "max_gen": 50,
 *      "mutation_rate": 0.01
 *    },
 *
 *    "mac_params": {        // 可选；缺省时用 sb_mac_params_default
 *      "sigma_us":  9.0,
 *      "sifs_us":  10.0,
 *      "difs_us":  28.0,
 *      "ack_us":   24.0,
 *      "header_us":20.0,
 *      "p_e_base": 0.01,
 *      "p_cap":    0.0
 *    }
 *  }
 * ============================================================ */

typedef struct {
    /* 链路生成公式参数（可被配置覆盖；缺省 = 原硬编码值） */
    double distance_limit_m;       /* 400 */
    double bandwidth_a, bandwidth_b;   /* 100000, 100 */
    double delay_const, delay_slope;   /* 0.01, 0.005 */
    double cost_const, cost_slope;     /* 10.0, 0.1 */
    double reliability_base;       /* 0.999 */
    double reliability_decay;      /* 0.01 */
} LinkGenParams;

void sb_link_gen_default(LinkGenParams *p);

typedef struct {
    int    pop_size;
    int    max_gen;
    double mutation_rate;
} GaParams;

void sb_ga_params_default(GaParams *p);

/* ============================================================
 *  顶层配置结构
 * ============================================================ */
typedef struct {
    char  sandbox_name[SB_STR_LEN];
    uint64_t rng_seed;

    int   has_link_gen;     LinkGenParams link_gen;
    int   has_ga;           GaParams      ga;
    int   has_mac;          MacParams     mac;

    /* 节点 / 流的存在与否标记 —— 若 = 0 表示加载时未指定，构造时落回 initializer 默认值 */
    int   has_nodes;
    int   has_links;
    int   has_flows;
} SandboxConfig;

/* ============================================================
 *  加载入口
 *
 *  从 JSON 文件读取配置，并按以下规则构造 Sandbox：
 *    - 若 config 内含 nodes 数组，用配置节点；否则用 initializer 默认 20 节点
 *    - 若 config 内含 link_generation，用配置公式；否则用默认参数
 *    - 若 config 内含 flows 数组，用配置流；否则用 initializer 默认 14 流
 *
 *  返回：0 成功；负值表示文件 / JSON 错误。
 * ============================================================ */
int  sb_load_config(const char *path,
                    SandboxConfig *cfg,
                    Sandbox *sb);

/* 把已加载的 SandboxConfig 应用到 sb（含 nodes/links/flows 填充） */
int  sb_apply_config(const SandboxConfig *cfg, Sandbox *sb);

/* 调试：打印加载的配置摘要 */
void sb_config_print(const SandboxConfig *cfg, const Sandbox *sb);

#endif
