#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHash>
#include <QWheelEvent>
#include <QMouseEvent>
#include "graph_items.h"
#include <QSplitter>
#include <QListWidget>
#include <QLabel>

#include <QListWidgetItem>
#include <QMap>
#include <QSplitter>
#include <QListWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>

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
    ~MainWindow();  // 【核心修复：补上这行析构函数的声明】

public slots:
public slots:
    void addNode(const QString& nodeId, int type, const QString& label, int shape, const QColor& color, int size);
    void addEdge(const QString& sourceId, const QString& destId, int type);
    void applyLayout();
    void clearGraph();

    // 【确保下面这两个声明都在这里，一个都不能少】
    void updateNodeStyle(const QString& nodeId, const QColor& color, int shape, int size = 80);
    void updateEdgeStyle(const QString& sourceId, const QString& destId, const QColor& color, int thickness, int style);

    void addTask(const QString& taskId, const QString& taskName, const QString& priority, const QString& statusIcon = "▶");
    void removeTask(const QString& taskId);
    void clearTasks();

private:
    QGraphicsScene *scene;
    InteractiveGraphicsView *view;
    bool m_isFirstLayout;

    QMap<QString, GraphNode*> m_nodeMap;
    QList<EdgeData> m_edges;
    QMap<QString, GraphEdge*> m_edgeItemMap;

    // 确保下面这三个侧边栏组件都只出现了一次，没有重复：
    QSplitter *m_splitter;
    QListWidget *m_nodeListWidget;
    QListWidget *m_taskListWidget;

    QMap<QString, QListWidgetItem*> m_taskItemMap;
};

#endif // MAINWINDOW_H