# Qt 前端调用说明

## 固定文件约定

正常场景：

```text
输入：wuli.json
输出：luoji.json、youhua1.json、youhua2.json
```

损毁场景：

```text
输入：wuli_sunhui.json
输出：luoji_sunhui.json、youhua1_sunhui.json、youhua2_sunhui.json
```

前端只需通过 `QProcess` 调用独立的 `topoopt`/`topoopt.exe`，并把同一个数据目录传给 `--output-dir`。后端根据输入文件名是否包含 `_sunhui` 自动选择输出名称。

## Qt 6 调用函数

```cpp
#include <QDir>
#include <QFileInfo>
#include <QProcess>

void MainWindow::runTopologyBackend(bool damaged)
{
    const QString backend = QDir::cleanPath(appDir + "/backend/topoopt.exe");
    const QString inputName = damaged ? "wuli_sunhui.json" : "wuli.json";
    const QString input = QDir(dataDir).filePath(inputName);

    auto *process = new QProcess(this);
    process->setProgram(backend);
    process->setArguments({
        "--config", input,
        "--output-dir", dataDir,
        "--pop", "100",
        "--gen", "50",
        "--mutation", "0.01",
        "--seed", "42"
    });
    process->setWorkingDirectory(QFileInfo(backend).absolutePath());
    process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(process, &QProcess::readyReadStandardOutput, this, [process]() {
        const QString text = QString::fromUtf8(process->readAllStandardOutput());
        // 将 text 追加到前端日志区域。
    });

    connect(process, &QProcess::readyReadStandardError, this, [process]() {
        const QString text = QString::fromUtf8(process->readAllStandardError());
        // 显示或保存错误日志。
    });

    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [process, damaged, dataDir = this->dataDir](int exitCode, QProcess::ExitStatus status) {
        if (status != QProcess::NormalExit || exitCode != 0) {
            process->deleteLater();
            return;
        }

        const QStringList names = damaged
            ? QStringList{"luoji_sunhui.json", "youhua1_sunhui.json", "youhua2_sunhui.json"}
            : QStringList{"luoji.json", "youhua1.json", "youhua2.json"};

        for (const QString& name : names) {
            if (!QFileInfo::exists(QDir(dataDir).filePath(name))) {
                process->deleteLater();
                return;
            }
        }

        // names[0]：初始逻辑拓扑
        // names[1]：低时延优化拓扑
        // names[2]：高可靠优化拓扑
        process->deleteLater();
    });

    process->start();
}
```

macOS/Linux 下将可执行文件名改为 `topoopt`。

## 命令行等价调用

正常场景：

```bash
topoopt --config /data/wuli.json --output-dir /data --pop 100 --gen 50 --mutation 0.01 --seed 42
```

损毁场景：

```bash
topoopt --config /data/wuli_sunhui.json --output-dir /data --pop 100 --gen 50 --mutation 0.01 --seed 42
```

## 前端展示映射

| 前端状态 | 正常场景文件 | 损毁场景文件 |
| --- | --- | --- |
| 物理拓扑 | `wuli.json` | `wuli_sunhui.json` |
| 初始逻辑拓扑 | `luoji.json` | `luoji_sunhui.json` |
| 低时延优化拓扑 | `youhua1.json` | `youhua1_sunhui.json` |
| 高可靠优化拓扑 | `youhua2.json` | `youhua2_sunhui.json` |

每次切换文件后重新读取：

- `nodes`：节点及 `R` 移动半径。
- `links`：当前逻辑拓扑链路。
- `flows[].routing_path`：业务流物理承载路径。
- `flows[].pass`：业务流达标状态。
- `assess_data`：五类总体指标及展示分数。

## 调用注意事项

1. 后端、输入文件和输出目录都使用绝对路径。
2. `--output-dir` 不存在时后端会尝试创建，但前端仍应保证父目录可写。
3. 不要同时运行两个写入同一数据目录的后端进程。
4. 必须等待进程正常退出且退出码为 `0` 后再读取结果。
5. stdout/stderr 使用 UTF-8 解码。
6. 正式运行可能持续数十秒，不能阻塞 UI 线程。
7. 前端取消操作可以先调用 `terminate()`，超时后再调用 `kill()`。
8. Windows 前端必须使用 Windows x64 编译生成的 `topoopt.exe`。

## 兼容的自定义基名模式

如其他工具仍需自定义输出名，可继续调用：

```bash
topoopt --config wuli.json --output result.json
```

该模式生成：

```text
result_initial.json
result_low_latency.json
result_high_reliability.json
```

前端正式集成建议统一使用 `--output-dir` 固定命名模式。

## 联调检查清单

- `topoopt --help` 正常输出。
- 正常输入直接生成 `luoji.json`、`youhua1.json`、`youhua2.json`。
- 损毁输入直接生成三个对应的 `_sunhui.json` 文件。
- 进程退出码为 `0`。
- 输出 JSON 包含 `nodes`、`links`、`flows`、`assess_data`。
- 前端能够显示 `routing_path`、`pass` 和总体指标。
