#ifndef TESTWORKER_H
#define TESTWORKER_H

#include <QObject>
#include <QString>
#include <QColor>

class TestWorker : public QObject {
    Q_OBJECT
public:
    explicit TestWorker(QObject *parent = nullptr);

signals:
    // 拓扑图基础结构信号
    void requestClear();
    void requestAddNode(const QString& nodeId, int type, const QString& label, int shape, const QColor& color, int size);
    // 【修改】：末尾增加 const QString& label 默认参数
    // 【修改】：在末尾增加 const QString& linkType
    // 【恢复】
    void requestAddEdge(const QString& sourceId, const QString& destId, int type, const QString& label = "");
    void requestLayout();

    void requestUpdateNodeStyle(const QString& nodeId, const QColor& color, int shape, int size);
    void requestUpdateEdgeStyle(const QString& sourceId, const QString& destId, const QColor& color, int thickness, int style);

    // 业务队列信号
    // 【修改】：末尾增加 srcId 和 dstId
    void requestAddTask(const QString& taskId, const QString& taskName, bool isCompliant, const QString& srcId, const QString& dstId);
    void requestRemoveTask(const QString& taskId);
    void requestClearTasks();

    // 【新增】：效能指标与评估模型数据推送信号
    void requestUpdateMetrics(const QString& latency, const QString& connectivity, const QString& reliability, const QString& throughput, const QString& cost);
    void requestUpdateEvaluation(const QString& scoreBaseline, const QString& scoreAdvanced);

public slots:
    // 【核心接口】：从 JSON 文件读取并生成整套拓扑与指标
    void loadGraphFromJson();
};

#endif // TESTWORKER_H