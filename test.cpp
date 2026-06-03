#include "test.h"
#include <QRandomGenerator>
#include <QStringList>

TestWorker::TestWorker(QObject *parent) : QObject(parent) {}

void TestWorker::generateRandomGraph() {
    // 1. 发送清空信号
    emit requestClear();
    emit requestClearTasks();

    int numNodes = 20;
    QStringList nodeIds;

    // 2. 生成节点并赋予随机物理属性
    for (int i = 0; i < numNodes; ++i) {
        // 生成节点 ID，例如 Node_01, Node_02
        QString nodeId = QString("Node_%1").arg(i + 1, 2, 10, QChar('0'));
        nodeIds.append(nodeId);

        // 随机属性生成
        bool isHighPerf = QRandomGenerator::global()->bounded(100) < 25; // 25% 概率是高性能节点
        QString nodeType = isHighPerf ? "高性能节点" : "标准节点";

        // 状态生成 (90% 在线, 10% 异常)
        bool isNormal = QRandomGenerator::global()->bounded(100) < 90;
        QString statusIcon = isNormal ? "🟢" : "🔴";
        QString statusText = isNormal ? "在线" : "异常";

        int cpu = QRandomGenerator::global()->bounded(5, 95);     // CPU负载 5% ~ 95%
        int mem = QRandomGenerator::global()->bounded(15, 90);    // 内存占用 15% ~ 90%
        QString bw = isHighPerf ? "10Gbps" : "1Gbps";             // 物理带宽
        int syncDiff = QRandomGenerator::global()->bounded(200) - 100; // 时钟同步差 -100 ~ 100 us

        // 将这些属性拼装成一个详细的 Label 传给前端 UI 列表
        QString label = QString("%1 [%2] %3 | %4 | CPU: %5% | 内存: %6% | 带宽: %7 | 时钟差: %8us")
                            .arg(statusIcon)
                            .arg(nodeId)
                            .arg(nodeType)
                            .arg(statusText)
                            .arg(cpu, 2)
                            .arg(mem, 2)
                            .arg(bw)
                            .arg(syncDiff);


        // 【核心新增】：根据业务属性决定节点的物理长相
        QColor nodeColor;
        if (!isNormal) {
            nodeColor = QColor("#e74c3c"); // 异常节点：红色
        } else if (isHighPerf) {
            nodeColor = QColor("#f39c12"); // 高性能节点：橙色
        } else {
            nodeColor = QColor("#2ecc71"); // 标准节点：绿色
        }

        // 假设 1 代表菱形/方形 (突出高性能)，0 代表圆形 (标准)
        int nodeShape = isHighPerf ? 1 : 0;

        // 高性能节点体型更大 (比如 60)，标准节点稍小 (比如 40)
        int nodeSize = isHighPerf ? 15 : 10;

        // 强参数发射信号
        emit requestAddNode(nodeId, isHighPerf ? 1 : 0, label, nodeShape, nodeColor, nodeSize);
    }

    // 3. 构建全连通图 (连通主干生成树)
    // 从第 2 个节点开始，每个节点随机向前面的 1 个节点连线，保证整张图绝对没有孤岛
    for (int i = 1; i < numNodes; ++i) {
        int targetIdx = QRandomGenerator::global()->bounded(i);
        emit requestAddEdge(nodeIds[i], nodeIds[targetIdx], 0);
    }

    // 4. 增加随机冗余边 (形成网状结构)
    int extraEdges = 15; // 增加 15 条随机冗余边
    for (int i = 0; i < extraEdges; ++i) {
        int src = QRandomGenerator::global()->bounded(numNodes);
        int dst = QRandomGenerator::global()->bounded(numNodes);
        if (src != dst) {
            emit requestAddEdge(nodeIds[src], nodeIds[dst], 0); // type=0 代表普通边
        }
    }

    // 5. 通知前端执行物理碰撞排版
    emit requestLayout();

    // 6. 顺便注入几个模拟任务流
    emit requestAddTask("01", "实时视频协同", "高", "▶");
    emit requestAddTask("02", "传感数据汇聚", "中", "⏳");
    emit requestAddTask("03", "节点探活同步", "最高", "⚡");
}