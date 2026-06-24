# 前端对接先看

正式调用使用固定命名模式：

```bash
topoopt --config /data/wuli.json --output-dir /data --pop 100 --gen 50 --mutation 0.01 --seed 42
```

正常输出：

```text
luoji.json
youhua1.json
youhua2.json
```

损毁调用只需将输入改为 `wuli_sunhui.json`，后端自动输出：

```text
luoji_sunhui.json
youhua1_sunhui.json
youhua2_sunhui.json
```

详细构建和字段说明见 `README.md`，Qt 6 `QProcess` 示例见 `FRONTEND_INTEGRATION.md`。

包内 `bin/macos-arm64/topoopt` 只适用于 Apple Silicon Mac；Windows 前端需按 README 编译 `topoopt.exe`。
