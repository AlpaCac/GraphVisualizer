#ifndef TEST_H
#define TEST_H

#include <QThread>
#include <QTimer>
#include <QString>
#include <QList>
#include <QColor>

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

    // 【关键修复】：确保下面这行是唯一的！删掉原来那个没有 size 参数的重载
    void requestUpdateNodeStyle(const QString& id, const QColor& color, int shape, int size);

private slots:
    void onTick();
    // (已删除 onFastTick)

private:
    QString m_controlNode;
    QList<QString> m_mainNodes;
    QList<QString> m_backupNodes;
    QList<QString> m_leafNodes;
    QList<EdgeDef> m_edges;

    int m_step;
    void generateGraph();
};

#endif // TEST_H