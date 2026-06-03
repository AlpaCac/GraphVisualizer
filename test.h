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
    void requestAddEdge(const QString& sourceId, const QString& destId, int type);
    void requestLayout();

    // 【补充漏掉的样式更新信号】
    void requestUpdateNodeStyle(const QString& nodeId, const QColor& color, int shape, int size);
    void requestUpdateEdgeStyle(const QString& sourceId, const QString& destId, const QColor& color, int thickness, int style);

    // 业务队列信号
    void requestAddTask(const QString& taskId, const QString& taskName, const QString& priority, const QString& statusIcon);
    void requestRemoveTask(const QString& taskId);
    void requestClearTasks();
public slots:
    // 【核心接口】：生成随机连通图
    void generateRandomGraph();
};

#endif // TESTWORKER_H