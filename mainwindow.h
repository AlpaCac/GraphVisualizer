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
#include <QFrame>   // 【新增】
#include <QLabel>   // 【新增】
#include <QResizeEvent> // 【新增】
#include <QGraphicsLineItem> // 【新增】：用于绘制逻辑连线
#include <QProcess> // 【新增】

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

signals: // 如果没有 signals: 关键字，请自己加一个
    // 【新增】：请求后端读取指定 JSON 文件的信号
    void requestLoadJson(const QString& fileName);

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();  // 【核心修复：补上这行析构函数的声明】

public slots:
    void addNode(const QString& nodeId, int type, const QString& label, int shape, const QColor& color, int size, double x, double y);
    // 【修改】：末尾增加 const QString& label 默认参数
    // 【修改】：同步增加 const QString& linkType
    // 【恢复】
    void addEdge(const QString& sourceId, const QString& destId, int type, const QString& label = "");
    void applyLayout();
    void clearGraph();

    // 【确保下面这两个声明都在这里，一个都不能少】
    void updateNodeStyle(const QString& nodeId, const QColor& color, int shape, int size = 80);
    void updateEdgeStyle(const QString& sourceId, const QString& destId, const QColor& color, int thickness, int style);

    // 【修改】：同步更新参数
    void addTask(const QString& taskId, const QString& taskName, bool isCompliant, const QString& srcId, const QString& dstId);
    void removeTask(const QString& taskId);
    void clearTasks();

    // 之前加的 5 大指标更新接口
    void updateMetrics(const QString& latency, const QString& connectivity, const QString& reliability, const QString& throughput, const QString& cost);

    // 【新增】：用于更新两个策略效能评估分数的槽函数
    void updateEvaluation(const QString& scoreBaseline, const QString& scoreAdvanced);

protected:
    // 【新增】：重写窗口改变大小事件，确保信息框始终贴在右上角
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // 【新增】：处理图元选中状态改变的槽函数
    void onSceneSelectionChanged();
    // 【新增】：监听列表选中项改变的槽函数
    void onTaskSelectionChanged();
    // 【新增】：监听具体的点击动作
    // void onTaskItemClicked(QListWidgetItem *item);
    // 【新增】：处理两个拓扑按钮的点击
    void onPhysicalTopologyClicked();
    void onLogicalTopologyClicked();
    void onOptimizeTopologyClicked(); // 【新增】

private:
    QGraphicsScene *scene;
    InteractiveGraphicsView *view;
    bool m_isFirstLayout;

    QMap<QString, GraphNode*> m_nodeMap;
    QList<EdgeData> m_edges;
    QMap<QString, GraphEdge*> m_edgeItemMap;

    // 确保下面这三个侧边栏组件都只出现了一次，没有重复：
    QSplitter *m_splitter;
    // QListWidget *m_nodeListWidget; // 【删除这行】
    QListWidget *m_taskListWidget;

    // 【新增】：效能指标数值显示的 Label
    QLabel *m_lblLatency;
    QLabel *m_lblConnectivity;
    QLabel *m_lblReliability;
    QLabel *m_lblThroughput;
    QLabel *m_lblCost;

    // 【新增】：效能评估双策略的评分 Label
    QLabel *m_lblScoreBaseline;
    QLabel *m_lblScoreAdvanced;

    QMap<QString, QListWidgetItem*> m_taskItemMap;

    // 【新增】：右上角悬浮的详细信息面板组件
    QFrame *m_infoBox;
    QLabel *m_infoTitle;
    QLabel *m_infoContent;

    // 【修改为】：管理多条连线的数组
    QList<QGraphicsLineItem*> m_flowEdges;

    // bool m_selectionJustChanged = false;

    QPushButton *m_btnPhysical;
    QPushButton *m_btnLogical;
    QPushButton *m_btnOptimize;
    QPushButton *m_btnDestroy;

    // 【必须添加这一行】：
    QLabel *m_canvasTitle;

    QProcess *m_optimizerProcess; // 【新增】

    QLabel *m_loadingOverlay = nullptr;

    void showLoading(bool visible);
};

#endif // MAINWINDOW_H