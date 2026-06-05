# 天鸿软总线拓扑评估及优化策略仿真 (C 实现)

面向物联集群的天鸿软总线拓扑建模、效能评估与 NSGA-II 优化算法的**纯 C 实现**。
完全对应 Python 参考算法（`evaluation/`、`routing/`、`simulation/`），单线程
全工程不依赖任何第三方库，仅需 `libc + libm`。

## 1. 工程结构

```
tianhong-softbus-sim/
├── README.md                 本文档
├── Makefile                  构建脚本
├── src/                      核心库
│   ├── task_data.{h,c}        需求文档表2 任务数据（含 JSON/CSV 解析）
│   ├── sb_models.{h,c}        建模：物理层 / 软总线层 / 业务流层 / 拓扑层
│   ├── sb_phys_bind.{h,c}     物理对象绑定（被各评估器共享）
│   ├── initializer.{h,c}      沙箱：20 节点 / 83 候选链路 / 14 业务流
│   ├── routing.{h,c}          确定性 Dijkstra 路由器
│   ├── cost.{h,c}             组网成本评估
│   ├── reliability.{h,c}      端到端可靠性评估
│   ├── connectivity.{h,c}     代数连通度 (Fiedler 值, Jacobi 旋转)
│   ├── latency.{h,c}          CPA 端到端最坏时延评估
│   ├── ga.{h,c}               NSGA-II 主循环（含故障注入与重建入口）
│   └── jsonw.{h,c}            极简增量式 JSON writer
├── apps/                     可执行入口
│   ├── main_ga.c              拓扑优化常规演化 demo
│   ├── main_full.c            完整实验（常规 + 故障 + 重建 + JSON 输出）
│   └── main_demo.c            数据模型 + TaskData 演示
├── tests/                    与 Python 参考实现的等价性测试
├── bench/                    性能基准
└── data/                     示例 JSON / CSV 任务数据
```

## 2. 编译与运行

需要 `gcc` 或兼容的 C99 编译器：

```bash
make                     # 编译全部
make run-ga              # 跑一次 NSGA-II 常规演化（约 4 秒）
make run-full            # 完整实验：常规 + 故障注入 + 重建 + JSON 输出（约 8 秒）
make run-demo            # 演示数据模型
make run-tests           # 等价性测试
make bench               # 性能基准
make clean               # 清理
```

主程序接受可选种子参数：

```bash
./build/main_ga    42
./build/main_full  42  experiment.json
```

## 3. 完整实验流水线 (`main_full`)

`main_full` 实现需求文档 3.1.2 节"节点损毁重构场景"的端到端流程：

```
1. 构造 baseline seed（5 个核心节点的 MST 拓扑）
        │
2. 常规演化 50 代 (NSGA-II 在正常网络上爬 Pareto 前沿)
        │
3. 取 best_lat（时延最优解）作为基础
        │
4. 故障注入：node 15 损毁
   ├─ role_gene[15] = -1
   ├─ 与 node 15 相连的所有候选链路 link_gene[j] = -1
   └─ sb.flow_graph 剔除源/目的为 node 15 的流
        │
5. 即时评估损毁后的网络（routing 可能失败，fitness 退化）
        │
6. 以 damaged_ind 为新种子，重建演化 50 代
        │
7. 全程结果写入 experiment.json
```

输出 JSON 顶层结构：

```json
{
  "config":                  { ... 实验配置 ... },
  "seed":                    { ... baseline seed 个体 ... },
  "snapshots_seed_topology": { "nodes": [...], "links": [...] },

  "normal_evolution": [
    { "generation": 0,  "sched_total": 0,   "min_comp_lat": null, ... },
    { "generation": 1,  "sched_total": 0,   ... },
    ...
    { "generation": 50, "sched_total": 100, "min_comp_lat": 0.21, ... }
  ],
  "normal_final": {
    "min_latency":     { "individual": {...}, "flows": [...], "topology": {...} },
    "max_fiedler":     { ... },
    "max_reliability": { ... },
    "min_cost":        { ... }
  },

  "disaster": {
    "type": "node",
    "target_id": 15,
    "cascading_links_destroyed": 13,
    "flows_annihilated_count": 1,
    "post_disaster_individual": {...},
    "post_disaster_flows":      [...],
    "post_disaster_topology":   {...}
  },

  "reconstruct_evolution": [ ... 51 条同结构 ... ],
  "reconstruct_final":     { ... 同 normal_final ... },

  "total_time_ms": 21430.2
}
```

文件约 100 KB，可被任意 JSON 库读取（Python `json.tool`、jq、前端 D3 等）。

## 3. 算法等价性

各模块与 Python 参考实现的对照结果：

| 模块 | 验证方式 | 结果 |
|---|---|---|
| `initializer`     | 20 节点 / 83 链路 / 14 流逐项 diff | 完全一致 |
| `routing`         | 100 个随机拓扑路径逐字节 diff | 完全一致 |
| `cost`            | `total_cost = 3142.962329` 等 | 6 位小数一致 |
| `reliability`     | 14 条流可靠性 | 8 位小数一致 |
| `connectivity`    | 100 个随机拓扑 Fiedler 值 | 10 位小数一致 |
| `latency` (CPA)   | 200 个不同密度的随机拓扑 | 6 位小数一致 |
| `ga` (NSGA-II)    | 演化曲线 + 最终 Pareto 极值量级 | 一致（RNG 不同，统计等价） |

## 4. 性能（NSGA-II 50 代 × 100 个体）

| 实现 | 总耗时 | 单次评估 |
|---|---|---|
| Python 参考 | 135 秒 | ~25 ms |
| **C (O2)**  | **3.9 秒** | **~0.7 ms** |
| 加速比 | **~35×** | |

测试机器：单核运行，无并行。

## 5. 数据模型扩展（需求文档 → 代码）

| 需求文档 | 代码字段 |
|---|---|
| **表1 节点数据**（基础标识 / 类型 / 计算资源 / 通信资源 / 运行状态 / 拓扑位置） | `PhysicalNode.meta: PhysicalNodeMeta`（6 个子结构）|
| **表2 任务数据**（基础 / 通信 / 质量 / 资源 / 优先级与约束） | `Flow.meta: TaskData`（5 个子结构）|
| **表3 场景数据** | 当前由 `Sandbox` 字段 + GA 故障注入接口承载 |

加载 JSON / CSV 任务：

```c
TaskSet set;
task_load_json_file("data/tasks.json", &set);
task_load_csv_file ("data/tasks.csv",  &set);
```

## 6. NSGA-II 优化指标

四维适应度（**全部最小化**）：

| 指标 | 含义 | 公式 |
|---|---|---|
| `comp_lat`  | 加权时延比 | `Σ (prio × e2e/deadline) / Σ prio` |
| `-fiedler`  | 代数连通度（取反） | 拉普拉斯矩阵第二小特征值 |
| `-comp_rel` | 加权可靠性（取反） | `Σ (prio × reliability) / Σ prio` |
| `cost`      | 总成本 | `Σ link.cost` |

硬约束（6 条 `validate_topology` 规则）：

1. 核心节点数 ∈ [4, 7]
2. 每节点链路数 ≤ `max_physical_ports`
3. 边缘节点连 1~2 个核心
4. 边缘-边缘链路总数 ≤ 4
5. 全图连通
6. 核心子图连通

可行解（`is_fully_schedulable=1`）永远支配不可行解，是支配比较的最高优先级规则。

## 7. 已知差异 / 注意事项

- **PRNG 不同**：Python 用 Mersenne Twister，C 用 xoshiro256\*\*。同一种子下两边产生的随机序列**不同**，因此具体每一代的种群成员不会逐字节一致，但**统计行为和最终 Pareto 极值量级一致**。
- **沙箱里业务流的链路时延** = `propagation_delay + message_size / bandwidth`（与 flow 相关，与 `ga_core.calculate_topology_latency` 一致）。
- **故障重建后**链路 ID 会**从 0 重新编号**（与 Python `init_candidate_links` 一致），故障前后的 `link_gene[i]` 指代不同对象。
- 路径回溯失败的流（`path_len == 0`）**仍然作为 source/sink 挂在节点上**，CPA 会把它当作 cross-flow competitor 计算（与 Python 通过 `dict.get(..., 0.0)` 默认值的行为一致）。

## 8. 常用 API 速览

```c
/* 构建沙箱 */
Sandbox sb; sb_build_sandbox(&sb);

/* 故障重建 */
sb_rebuild_with_damage(&sb, /*failed_node_id=*/15);

/* 绑定物理对象表（所有评估器之前必须做一次） */
sb_phys_bind(sb.p_nodes, sb.c_links);

/* 路由 */
DeterministicRouter rt; sb_router_init(&rt);
sb_router_route_all(&rt, &topo);

/* 四项评估 */
double cost      = sb_cost_evaluate(&topo);
double fiedler   = sb_connectivity_evaluate(&topo);
sb_reliability_evaluate_all(&topo);  /* 写回 flow.actual_reliability */

CpaAnalyzer cpa;
sb_cpa_init(&cpa, &topo);
sb_cpa_analyze(&cpa);
sb_cpa_write_back(&cpa);             /* 写回 flow.worst_case_delay */

/* NSGA-II 主循环 */
GaContext ctx = { .sb = &sb, .verbose = 1 };
sb_router_init(&ctx.router);
ga_seed_rng(42);
Population pop;
ga_run(&ctx, &pop);
```
