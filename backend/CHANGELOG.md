# Changelog

## 2026.06.24

- 形成前端正式交付版 C++17 命令行接口。
- 一次调用输出初始逻辑拓扑、低时延优化拓扑和高可靠优化拓扑三份 JSON。
- 增加 `--output-dir` 前端固定命名模式，直接生成 `luoji.json`、`youhua1.json`、`youhua2.json` 及对应损毁文件。
- NSGA-II 使用五目标联合优化，并从 Pareto 前沿执行双策略选择。
- 变异更新为泊松采样的 Add / Remove / Swap 离散拓扑动作。
- 删除最终剪枝后处理，输出保持为 Pareto 解。
- 固定主节点角色，不参与遗传变化。
- 保持输入/输出 JSON 字段格式，输出业务流 `routing_path` 和 `pass`。
- 节点配置支持并透传移动半径 `R`。
- 增加 `--help`、未知参数检查、异常错误信息和退出码。
- 更新 README、Qt `QProcess` 示例和正常/损毁输入输出样例。
