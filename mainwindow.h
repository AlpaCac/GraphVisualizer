#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHash>
#include <QWheelEvent>
#include <QMouseEvent>
#include "graph_items.h"

// ================= 定制的高级交互画布视图 =================
class InteractiveGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit InteractiveGraphicsView(QGraphicsScene *scene, QWidget *parent = nullptr);

protected:
    void wheelEvent(QWheelEvent *event) override;
};

// ================= 主窗口界面声明 =================
struct EdgeData {
    QString sourceId;
    QString destId;
    int type;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

public slots:
    void addNode(const QString& nodeId, int type, const QString& label);
    void addEdge(const QString& sourceId, const QString& destId, int type);
    void applyLayout();
    void clearGraph();

    // 【确保下面这两个声明都在这里，一个都不能少】
    void updateNodeStyle(const QString& nodeId, const QColor& color, int shape, int size = 80);
    void updateEdgeStyle(const QString& sourceId, const QString& destId, const QColor& color, int thickness, int style);

private:
    QGraphicsScene *scene;
    InteractiveGraphicsView *view;

    QHash<QString, GraphNode*> m_nodeMap;
    QList<EdgeData> m_edges;

    QHash<QString, GraphEdge*> m_edgeItemMap;

    bool m_isFirstLayout;
    // 【修改】：已彻底删除 m_layoutCounter 轮播变量
};

#endif // MAINWINDOW_H