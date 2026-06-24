# 示例数据

## 输入

- `input/wuli.json`：场景 1 正常物理拓扑。
- `input/wuli_sunhui.json`：场景 1 节点损毁后的物理拓扑。

## 输出样例

`output_sample/` 中的文件由当前 C++17 后端使用 `pop=60`、`gen=50`、`seed=42` 生成，用于前端在不运行算法时验证 JSON 解析和界面切换。

正常场景：

```text
luoji.json
youhua1.json
youhua2.json
```

损毁场景：

```text
luoji_sunhui.json
youhua1_sunhui.json
youhua2_sunhui.json
```

## 快速联调

macOS/Linux：

```bash
./scripts/run_example.sh
```

Windows：

```bat
scripts\run_example.bat
```

快速联调脚本使用较小的 `pop=20`、`gen=5`，仅用于验证调用链路；正式展示建议使用 `pop=100`、`gen=50`。
