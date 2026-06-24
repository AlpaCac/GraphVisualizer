# 天鸿软总线拓扑评估与优化后端（C++17）

`topoopt` 是供仿真前端调用的独立命令行后端。它读取一份物理拓扑 JSON，构建初始逻辑拓扑，执行五目标 NSGA-II 优化，并从 Pareto 前沿中分别选择低时延策略和高可靠策略。

## 后端输入与输出

输入：

- 一份统一格式的物理拓扑 JSON。
- `nodes` 表示物理节点。
- `links` 表示物理链路。
- `flows` 表示业务流。

前端固定命名模式一次运行输出三份 JSON：

| 输出文件 | 含义 |
| --- | --- |
| `luoji.json` | 根据正常物理拓扑构建的初始逻辑拓扑 |
| `youhua1.json` | 正常场景低时延优化逻辑拓扑 |
| `youhua2.json` | 正常场景高可靠优化逻辑拓扑 |
| `luoji_sunhui.json` | 损毁场景初始逻辑拓扑 |
| `youhua1_sunhui.json` | 损毁场景低时延优化逻辑拓扑 |
| `youhua2_sunhui.json` | 损毁场景高可靠优化逻辑拓扑 |

输入文件名包含 `_sunhui` 时，后端自动使用损毁文件名。推荐前端使用 `--output-dir`。原 `--output result.json` 基名模式仍保留作兼容接口。

输出 JSON 保持输入配置的整体字段格式和顺序；其中 `links` 改为对应阶段的逻辑链路，`flows[].routing_path` 和 `flows[].pass` 更新为本次评估结果，`assess_data` 更新为总体指标。

## 快速调用

```bash
./topoopt \
  --config /absolute/path/wuli.json \
  --output-dir /absolute/path/data \
  --pop 100 \
  --gen 50 \
  --mutation 0.01 \
  --seed 42
```

前端随后读取：

```text
/absolute/path/data/luoji.json
/absolute/path/data/youhua1.json
/absolute/path/data/youhua2.json
```

查看完整参数：

```bash
./topoopt --help
```

## 命令行参数

| 参数 | 是否必需 | 说明 |
| --- | --- | --- |
| `--config <path>` | 正常调用必需 | 物理拓扑输入 JSON |
| `--output-dir <dir>` | 前端推荐 | 固定命名输出目录；自动区分正常/损毁 |
| `--output <path>` | 兼容模式 | 自定义输出基名，生成三个带策略后缀的文件 |
| `--pop <n>` | 可选 | 种群规模，覆盖 `ga_params.pop_size` |
| `--gen <n>` | 可选 | 迭代代数，覆盖 `ga_params.max_gen` |
| `--mutation <rate>` | 可选 | 基础变异率，覆盖 `ga_params.mutation_rate` |
| `--seed <n>` | 可选 | 随机种子，覆盖输入中的 `rng_seed` |
| `--demo` | 可选 | 运行内置快速演示，不读取配置文件 |
| `-h`, `--help` | 可选 | 显示帮助 |

命令行参数优先于配置文件中的对应参数。建议正式展示使用 `--pop 100 --gen 50`；联调时可使用 `--pop 20 --gen 5` 缩短等待时间。

## 构建

### CMake（推荐）

macOS / Linux：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Windows（Visual Studio 2022）：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

生成位置通常为：

```text
macOS/Linux: build/topoopt
Windows:     build/Release/topoopt.exe
```

### Make（macOS / Linux）

```bash
make
./topoopt --help
```

要求：支持 C++17 的编译器和 CMake 3.12 及以上版本。后端只使用 C++ 标准库，无第三方运行时依赖。

## 输入配置关键字段

顶层字段：

| 字段 | 说明 |
| --- | --- |
| `sandbox_name` | 场景名称 |
| `rng_seed` | 默认随机种子 |
| `nodes` | 物理节点数组 |
| `links` | 物理链路数组，至少保证物理网络可连通 |
| `flows` | 业务流数组 |
| `ga_params` | 默认 GA 参数 |
| `mac_params` | MAC 层时延与误码参数 |
| `assess_data` | 输入可保留；运行后由后端覆盖 |

节点常用字段：

```json
{
  "id": 0,
  "x": 120.0,
  "y": 150.0,
  "cpu_capacity": 7000.0,
  "memory_mb": 16384.0,
  "max_ports": 5,
  "reliability": 0.995,
  "device_name": "Node-00",
  "static_type": "Master",
  "R": 0
}
```

- `static_type` 可为 `Master`、`Backup` 或省略为普通节点。
- `R` 是前端使用的节点移动半径；后端保持该字段，不用它改变当前优化算法。

物理链路常用字段：

```json
{
  "id": 0,
  "node_a": 0,
  "node_b": 1,
  "bandwidth": 10.0,
  "propagation_delay": 0.3,
  "reliability": 0.99,
  "cost": 10.0,
  "type": "光纤"
}
```

`type` 当前场景使用：`星闪`、`wifi`、`蓝牙`、`光纤`。

业务流常用字段：

```json
{
  "id": 0,
  "name": "视频传输任务1（0到10）",
  "src": 0,
  "dst": 10,
  "priority": 10,
  "period": 20,
  "deadline": 800,
  "message_size": 128,
  "reliability_req": 0.7,
  "routing_path": [],
  "pass": 0
}
```

## 输出指标

`assess_data`：

| 字段 | 含义 |
| --- | --- |
| `comp_lat` | 综合时延占比，越低越好 |
| `C_conn_norm` | 相对初始逻辑拓扑的归一化连通度 |
| `E_throughput` | 有效吞吐量，单位 Gbps |
| `comp_rel` | 综合可靠性 |
| `cost` | 归一化组网成本 |
| `data1` | 当前版本的低时延策略得分 |
| `data2` | 当前版本的可靠性策略得分 |

每条业务流：

- `routing_path`：本次评估得到的物理承载路径节点 ID 数组。
- `pass`：`1` 表示时延、吞吐量、可靠性和可达性硬约束全部满足；否则为 `0`。

## 退出码与日志

| 退出码 | 含义 |
| --- | --- |
| `0` | 运行成功，三份输出已写出 |
| `1` | 参数、输入 JSON、文件访问或算法执行异常 |

标准输出包含初始化、每代 Pareto 前沿、硬约束达标数、最佳指标、输出路径和业务流详情。标准错误输出包含失败原因。前端应同时收集 stdout/stderr，并以进程退出码作为成功判断依据。

## 前端对接

Qt `QProcess` 对接示例、输出文件检查和路径处理见 [FRONTEND_INTEGRATION.md](FRONTEND_INTEGRATION.md)。

## 当前算法说明

- 五目标：时延、吞吐量、代数连通度、可靠性、组网成本。
- 硬约束：逐业务流执行 `is_schedulable` 判断。
- 变异：以 `lambda = N * Pm` 为期望进行泊松采样，至少执行一次 Add / Remove / Swap 拓扑动作。
- 不执行最终剪枝后处理，输出保持为 NSGA-II Pareto 解。
- 主节点角色不参与遗传变化。
- 低时延和高可靠结果均从同一次五目标优化的 Pareto 前沿中选择。

## 目录

```text
TopoOptV2_CPP/
├── CMakeLists.txt
├── Makefile
├── README.md
├── FRONTEND_INTEGRATION.md
├── include/
├── src/
└── examples/
    ├── input/
    └── output_sample/
```
