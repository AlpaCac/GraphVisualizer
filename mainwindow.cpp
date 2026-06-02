#include "mainwindow.h"
#include <QVBoxLayout>
#include <QScrollBar>
#include <QDebug>
#include <cmath> // 在文件顶部添加

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

// 【新增核心函数】：接收信号并瞬间改变对应节点的外观
void MainWindow::updateNodeStyle(const QString& nodeId, const QColor& color, int shape, int size) {
    if (m_nodeMap.contains(nodeId)) {
        m_nodeMap[nodeId]->setStyle(color, shape, size);
    }
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
    // 【核心排版区】：工业级绝对几何矩阵排版 (法向量双侧阵列)
    // =========================================================================
    int mainCount = 0;
    QHash<QString, int> leafCounts;
    for (ogdf::node v : G.nodes) {
        QString id = ogdfToId[v];
        if (id.startsWith("Main_")) {
            mainCount++;
        } else if (id.startsWith("Leaf_")) {
            leafCounts[id.split("_")[1]]++;
        }
    }

    if (mainCount > 0) {
        double cx = 0, cy = 0;

        // 1. 【核心修改】：大幅扩大主、备双环的半径
        double R_backup = 450.0; // 备用网内环 (原 300)
        double R_main = 750.0;   // 主干网大环 (原 450)

        QHash<QString, QPointF> mainPos;
        QHash<QString, QPointF> backupPos;

        // 第一遍：先算出所有主节点和备用节点的绝对坐标并存下来
        for (ogdf::node v : G.nodes) {
            QString id = ogdfToId[v];
            if (id.startsWith("Main_")) {
                int idx = id.split("_")[1].toInt();
                double rad = (idx * (360.0 / mainCount)) * M_PI / 180.0;
                mainPos[id] = QPointF(cx + R_main * std::cos(rad), cy + R_main * std::sin(rad));
                GA.x(v) = mainPos[id].x();
                GA.y(v) = mainPos[id].y();
            }
            else if (id.startsWith("Backup_")) {
                int idx = id.split("_")[1].toInt();
                double offset = (360.0 / mainCount) / 2.0;
                double rad = (idx * (360.0 / mainCount) + offset) * M_PI / 180.0;
                backupPos[id] = QPointF(cx + R_backup * std::cos(rad), cy + R_backup * std::sin(rad));
                GA.x(v) = backupPos[id].x();
                GA.y(v) = backupPos[id].y();
            }
        }

        // 第二遍：利用主备坐标，进行法向量几何阵列
        for (ogdf::node v : G.nodes) {
            QString id = ogdfToId[v];
            if (id == "Control_Center") {
                GA.x(v) = cx; GA.y(v) = cy; // 控制中心依旧居中
            }
            else if (id.startsWith("Leaf_")) {
                QStringList parts = id.split("_");
                QString mainIdxStr = parts[1];
                int leafIdx = parts[2].toInt();
                int totalLeaves = leafCounts[mainIdxStr];

                QString mId = "Main_" + mainIdxStr;
                QString bId = "Backup_" + mainIdxStr;

                if (mainPos.contains(mId) && backupPos.contains(bId)) {
                    QPointF pm = mainPos[mId];
                    QPointF pb = backupPos[bId];

                    // A. 算出从 备节点 指向 主节点 的方向向量
                    double dx = pm.x() - pb.x();
                    double dy = pm.y() - pb.y();
                    double length = std::sqrt(dx*dx + dy*dy);

                    // B. 算出垂直于这条线的 法向量 (Normal Vector)
                    double nx = -dy / length;
                    double ny = dx / length;

                    // C. 找到主节点和备节点的绝对中心点 (十字交叉路口)
                    double midX = pb.x() + dx * 0.5;
                    double midY = pb.y() + dy * 0.5;

                    // D. 【核心修改：垂直平分线阵列】
                    // 让所有叶子节点在这个法线上居中、对称排布
                    double spacing = 50.0; // 叶子节点之间的间距 (像素)

                    // 核心公式：计算当前叶子在法线上的偏移距离，保证整体居中
                    double offsetDist = (leafIdx - (totalLeaves - 1) / 2.0) * spacing;

                    GA.x(v) = midX + nx * offsetDist;
                    GA.y(v) = midY + ny * offsetDist;
                }
            }
        }
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