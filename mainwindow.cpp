#include "mainwindow.h"
#include <QVBoxLayout>
#include <QScrollBar>
#include <QDebug>

// ==================== OGDF 核心与布局库引入 ====================
#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphAttributes.h>
#include <ogdf/misclayout/CircularLayout.h> // 仅保留圆形布局头文件

// ================= InteractiveGraphicsView 实现 =================
InteractiveGraphicsView::InteractiveGraphicsView(QGraphicsScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
{
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void InteractiveGraphicsView::wheelEvent(QWheelEvent *event) {
    double factor = 1.15;
    if (event->angleDelta().y() < 0) factor = 1.0 / factor;
    scale(factor, factor);
}

// ================= MainWindow 实现 =================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    scene(new QGraphicsScene(this)),
    view(new InteractiveGraphicsView(scene, this)),
    m_isFirstLayout(true)
{
    resize(1024, 768);
    setWindowTitle("OGDF 动态智能拓扑网络 - 环状布局版");
    scene->setSceneRect(-50000, -50000, 100000, 100000);
    setCentralWidget(view);
}

void MainWindow::addNode(const QString& nodeId, int type, const QString& label) {
    if (m_nodeMap.contains(nodeId)) return;
    GraphNode *node = new GraphNode(nodeId, type, label);
    scene->addItem(node);
    m_nodeMap.insert(nodeId, node);
}

void MainWindow::addEdge(const QString& sourceId, const QString& destId, int type) {
    if (!m_nodeMap.contains(sourceId) || !m_nodeMap.contains(destId)) return;
    GraphNode *source = m_nodeMap[sourceId];
    GraphNode *dest = m_nodeMap[destId];
    GraphEdge *edge = new GraphEdge(source, dest, type);
    scene->addItem(edge);
    source->addEdge(edge);
    dest->addEdge(edge);
    m_edges.append({sourceId, destId, type});

    // 【新增】：将实例存入字典，Key 为 "起点->终点"
    m_edgeItemMap.insert(sourceId + "->" + destId, edge);
}

void MainWindow::clearGraph() {
    scene->clear();
    m_nodeMap.clear();
    m_edges.clear();
    m_edgeItemMap.clear(); // 【新增】
}

// 【新增核心函数】：接收信号并瞬间改变对应连线的外观
void MainWindow::updateEdgeStyle(const QString& sourceId, const QString& destId, const QColor& color, int thickness, int style) {
    QString key = sourceId + "->" + destId;
    if (m_edgeItemMap.contains(key)) {
        // 使用 static_cast 巧妙绕过跨线程传递枚举的元类型注册问题
        m_edgeItemMap[key]->setStyle(color, thickness, static_cast<Qt::PenStyle>(style));
    }
}

void MainWindow::applyLayout() {
    if (m_nodeMap.isEmpty()) return;

    ogdf::Graph G;
    ogdf::GraphAttributes GA(G, ogdf::GraphAttributes::nodeGraphics | ogdf::GraphAttributes::edgeGraphics);

    QHash<QString, ogdf::node> idToOgdf;
    QHash<ogdf::node, QString> ogdfToId;

    for (auto it = m_nodeMap.begin(); it != m_nodeMap.end(); ++it) {
        ogdf::node v = G.newNode();
        idToOgdf[it.key()] = v;
        ogdfToId[v] = it.key();
        GA.width(v) = 120.0;
        GA.height(v) = 80.0;
    }

    for (const EdgeData& edge : qAsConst(m_edges)) {
        if (idToOgdf.contains(edge.sourceId) && idToOgdf.contains(edge.destId)) {
            G.newEdge(idToOgdf[edge.sourceId], idToOgdf[edge.destId]);
        }
    }

    // =========================================================================
    // 【核心排版区】：固定使用 CircularLayout
    // =========================================================================
    qDebug() << "[Layout Engine] 当前运行方案：【固定方案：标准圆形/环状布局 (已扩容)】";

    ogdf::CircularLayout layout;
    layout.call(GA);

    // 【扩容Hack】：将生成的正圆形排版按比例撑大，防止节点遮挡中心连线
    // 如果你觉得还是挤，可以把 3.5 改成 4.0 或 5.0
    double scaleFactor = 3.5;
    for (ogdf::node v : G.nodes) {
        GA.x(v) *= scaleFactor;
        GA.y(v) *= scaleFactor;
    }
    // =========================================================================

    // 应用坐标回 Qt 界面
    for (ogdf::node v : G.nodes) {
        QString nodeId = ogdfToId[v];
        if (m_nodeMap.contains(nodeId)) {
            m_nodeMap[nodeId]->setPos(GA.x(v), GA.y(v));
        }
    }

    if (m_isFirstLayout) {
        view->fitInView(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
        m_isFirstLayout = false;
    }
}