#ifndef TEST_H
#define TEST_H

#include <QThread>
#include <QTimer>
#include <QString>
#include <QList>

struct EdgeDef {
    QString src;
    QString dst;
};

class TestWorker : public QThread
{
    Q_OBJECT
public:
    explicit TestWorker(QObject *parent = nullptr);

protected:
    void run() override;

signals:
    void requestClear();
    void requestAddNode(const QString& id, int type, const QString& label);
    void requestAddEdge(const QString& src, const QString& dst, int type);
    void requestLayout();
    void requestUpdateEdgeStyle(const QString& src, const QString& dst, const QColor& color, int thickness, int style);
    void requestUpdateNodeStyle(const QString& id, const QColor& color, int shape);

private slots:
    void onTick();
    void onFastTick();

private:
    QList<QString> m_highNodes;    // 骨架高级节点 (不可删)
    QList<QString> m_stdNodes;     // 骨架标准节点 (不可删)
    QList<QString> m_dynamicNodes; // 【新增】外围动态节点 (允许安全增删)
    QList<EdgeDef> m_edges;        // 所有的边
    int m_step;

    void generateInitialGraph();
};

#endif // TEST_H