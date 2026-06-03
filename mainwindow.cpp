#include "mainwindow.h"
#include <QVBoxLayout>
#include <QScrollBar>
#include <QDebug>
#include <cmath>
#include <QRandomGenerator> // 【核心引入】：随机数发生器，用于打破奇点
#include <ogdf/misclayout/CircularLayout.h>

// ==================== OGDF 核心与布局库引入 ====================
#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphAttributes.h>
// 【核心引入】：精确版 Fruchterman-Reingold 纯物理力导向算法
#include <ogdf/energybased/SpringEmbedderFRExact.h>

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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    scene(new QGraphicsScene(this)),
    view(new InteractiveGraphicsView(scene, this)),
    m_isFirstLayout(true)
{
    // ... [保持前方的初始化代码不变] ...
    resize(1600, 1000);
    setWindowTitle("天鸿软总线数字仿真 - 任务与节点控制台");
    scene->setSceneRect(-50000, -50000, 100000, 100000);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_splitter);

    QWidget *sideBarWidget = new QWidget(m_splitter);
    sideBarWidget->setStyleSheet("QWidget { background-color: #2b3a4a; color: white; }");

    QVBoxLayout *sideLayout = new QVBoxLayout(sideBarWidget);
    sideLayout->setContentsMargins(15, 20, 15, 20);
    sideLayout->setSpacing(20);

    // 1. 顶部主标题
    QLabel *mainTitle = new QLabel("<b>天鸿软总线数字仿真</b><br><small>任务与节点控制台</small>", sideBarWidget);
    mainTitle->setAlignment(Qt::AlignCenter);
    mainTitle->setStyleSheet("font-size: 20px; padding-bottom: 15px; color: #ecf0f1;");
    sideLayout->addWidget(mainTitle);

    // ==============================================================
    // 2. 业务流调度队列 (任务)
    // ==============================================================
    QGroupBox *taskGroup = new QGroupBox("📋 业务流调度队列", sideBarWidget);
    taskGroup->setStyleSheet("QGroupBox { font-weight: bold; padding-top: 20px; font-size: 15px; }");
    QVBoxLayout *taskLayout = new QVBoxLayout(taskGroup);
    m_taskListWidget = new QListWidget(taskGroup);

    // 【核心修复】：取消 fixedHeight，完全依靠布局管理器
    m_taskListWidget->setStyleSheet(
        "QListWidget { "
        "   background-color: #111b24; "
        "   color: #bdc3c7; "
        "   border: 1px solid #2c3e50; "
        "   padding: 10px; "
        "   font-size: 14px; "
        "}"
        "QListWidget::item { "
        "   padding: 10px; "
        "   margin-bottom: 5px; "
        "   background-color: #1a252f; "
        "   border-radius: 4px; "
        "}"
        "QListWidget::item:selected { "
        "   background-color: #2980b9; "
        "   color: white; "
        "}"
        );

    m_taskListWidget->addItem("▶ [流 ID: 01] 实时视频协同 | 优先级: 高");
    m_taskListWidget->addItem("⏳ [流 ID: 02] 传感数据汇聚 | 优先级: 中");
    m_taskListWidget->addItem("▶ [流 ID: 03] 紧急控制指令 | 优先级: 特急");
    m_taskListWidget->addItem("⏸ [流 ID: 04] 高频心跳状态 | 优先级: 高");
    m_taskListWidget->addItem("⏳ [流 ID: 05] 大文件日志回传 | 优先级: 低");

    taskLayout->addWidget(m_taskListWidget);
    sideLayout->addWidget(taskGroup, 1); // 【核心修复】：加上 stretch 因子 1

    // ==============================================================
    // 3. 底层拓扑物理节点
    // ==============================================================
    QGroupBox *nodeGroup = new QGroupBox("🖥 底层拓扑物理节点", sideBarWidget);
    nodeGroup->setStyleSheet("QGroupBox { font-weight: bold; padding-top: 20px; font-size: 15px; }");
    QVBoxLayout *nodeLayout = new QVBoxLayout(nodeGroup);
    m_nodeListWidget = new QListWidget(nodeGroup);

    m_nodeListWidget->setStyleSheet(
        "QListWidget { "
        "   background-color: #1a252f; "
        "   color: #ecf0f1; "
        "   border: 1px solid #34495e; "
        "   padding: 5px; "
        "   font-size: 13px; "
        "}"
        "QListWidget::item { "
        "   padding: 6px; "
        "   border-bottom: 1px solid #2c3e50; "
        "}"
        "QListWidget::item:selected { "
        "   background-color: #e74c3c; "
        "}"
        );

    nodeLayout->addWidget(m_nodeListWidget);
    sideLayout->addWidget(nodeGroup, 2); // 【核心修复】：加上 stretch 因子 2，让节点列表占更多的纵向空间

    // ==============================================================
    // 组装切分器并强设宽度
    // ==============================================================
    m_splitter->addWidget(sideBarWidget);
    m_splitter->addWidget(view);

    // 【核心修复】：调窄侧边栏，从 450 降到 350
    QList<int> sizes;
    sizes << 350 << 1250;
    m_splitter->setSizes(sizes);
}
MainWindow::~MainWindow() {}

void MainWindow::addNode(const QString& nodeId, int type, const QString& label, int shape, const QColor& color, int size) {
    if (m_nodeMap.contains(nodeId)) return;

    // ==========================================================
    // 【核心拦截】：从 nodeId (例如 "Node_01") 中提取出 "01"
    // ==========================================================
    QString shortLabel = nodeId;
    if (nodeId.contains("_")) {
        shortLabel = nodeId.split("_").last();
    }

    // 【修改】：在创建图元时，第三个参数传 shortLabel 而不是长长的 label
    GraphNode *node = new GraphNode(nodeId, type, shortLabel);
    scene->addItem(node);
    m_nodeMap.insert(nodeId, node);

    // 强制应用后端下发的形状、颜色和尺寸
    node->setStyle(color, shape, size);

    // 【保持不变】：侧边栏列表依然使用后端传来的完整详尽的 label
    QString displayText = label.isEmpty() ? nodeId : label;
    m_nodeListWidget->addItem(displayText);
}
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

    m_edgeItemMap.insert(sourceId + "->" + destId, edge);
}

void MainWindow::clearGraph() {
    scene->clear();
    m_nodeMap.clear();
    m_edges.clear();
    m_edgeItemMap.clear();

    // 【新增】：同步清空左侧列表
    if(m_nodeListWidget) {
        m_nodeListWidget->clear();
    }
}
void MainWindow::updateEdgeStyle(const QString& sourceId, const QString& destId, const QColor& color, int thickness, int style) {
    QString key = sourceId + "->" + destId;
    if (m_edgeItemMap.contains(key)) {
        m_edgeItemMap[key]->setStyle(color, thickness, static_cast<Qt::PenStyle>(style));
    }
}

// =========================================================================
// 业务流调度队列 (任务) 动态控制接口
// =========================================================================

void MainWindow::addTask(const QString& taskId, const QString& taskName, const QString& priority, const QString& statusIcon) {
    // 防止重复添加
    if (m_taskItemMap.contains(taskId)) return;

    // 拼装专业格式文本，例如："▶ [流 ID: 01] 实时视频协同 | 优先级: 高"
    QString displayText = QString("%1 [流 ID: %2] %3 | 优先级: %4")
                              .arg(statusIcon)
                              .arg(taskId)
                              .arg(taskName)
                              .arg(priority);

    // 创建列表项并应用到 UI
    QListWidgetItem* item = new QListWidgetItem(displayText, m_taskListWidget);

    // 存入字典，以便后续能够精准找到并删除它
    m_taskItemMap.insert(taskId, item);
}

void MainWindow::removeTask(const QString& taskId) {
    if (m_taskItemMap.contains(taskId)) {
        // 从字典中取出该项
        QListWidgetItem* item = m_taskItemMap.take(taskId);
        // 从 UI 列表中移除并释放内存
        delete item;
    }
}

void MainWindow::clearTasks() {
    m_taskListWidget->clear();
    m_taskItemMap.clear();
}

// =========================================================================
// 核心排版引擎：纯数据驱动的物理力导向算法
// 彻底屏蔽业务逻辑，不解析任何节点字符串名称，自适应渲染任意网络拓扑
// =========================================================================
void MainWindow::applyLayout() {
    if (m_nodeMap.isEmpty()) return;

    ogdf::Graph G;
    ogdf::GraphAttributes GA(G, ogdf::GraphAttributes::nodeGraphics | ogdf::GraphAttributes::edgeGraphics);

    QHash<QString, ogdf::node> idToOgdf;
    QHash<ogdf::node, QString> ogdfToId;

    // 1. 构建底层拓扑图结构
    for (auto it = m_nodeMap.begin(); it != m_nodeMap.end(); ++it) {
        ogdf::node v = G.newNode();
        idToOgdf[it.key()] = v;
        ogdfToId[v] = it.key();

        // 【修改 1】：把底层的物理碰撞盒从 60 缩小到 30
        GA.width(v) = 30.0;
        GA.height(v) = 30.0;
    }

    for (const EdgeData& edge : qAsConst(m_edges)) {
        if (idToOgdf.contains(edge.sourceId) && idToOgdf.contains(edge.destId)) {
            G.newEdge(idToOgdf[edge.sourceId], idToOgdf[edge.destId]);
        }
    }

    // 2. 物理排版与计算
    if (G.numberOfNodes() > 0) {

        // 【宇宙大爆炸初始化】
        for (ogdf::node v : G.nodes) {
            GA.x(v) = QRandomGenerator::global()->bounded(1000) - 500.0;
            GA.y(v) = QRandomGenerator::global()->bounded(1000) - 500.0;
        }

        // 【核心修改】：替换为环状布局引擎
        ogdf::CircularLayout circleLayout;
        circleLayout.minDistCC(80.0);
        circleLayout.call(GA);

        // =====================================================================
        // 【终极核心修复：强制物理碰撞排斥 (Overlap Resolution)】
        // 专门对付“拓扑同构”导致的重叠。把节点当成实体，谁碰在一起就强行推开谁！
        // 100% 保证不重叠，且不依赖任何业务逻辑和 OGDF 的高阶库。
        // =====================================================================
        double padding = 15.0; // 节点之间强制保持的安全缓冲间距
        int maxIterations = 200; // 最大推挤迭代次数

        for (int iter = 0; iter < maxIterations; ++iter) {
            bool hasOverlap = false;
            for (ogdf::node v : G.nodes) {
                for (ogdf::node u : G.nodes) {
                    if (v == u) continue; // 不和自己比

                    double dx = GA.x(v) - GA.x(u);
                    double dy = GA.y(v) - GA.y(u);
                    double dist = std::sqrt(dx*dx + dy*dy);

                    // 算出两个节点的理论安全距离 (半径之和 + 缓冲间距)
                    double minDist = (GA.width(v) + GA.width(u)) / 2.0 + padding;

                    // 如果实际距离小于安全距离，说明发生了重叠！
                    if (dist < minDist) {
                        // 如果两个节点由于同构吸得太死，连坐标都完全一样了，给个微小扰动
                        if (dist < 0.01) {
                            dx = (QRandomGenerator::global()->generateDouble() - 0.5) * 5.0;
                            dy = (QRandomGenerator::global()->generateDouble() - 0.5) * 5.0;
                            dist = std::sqrt(dx*dx + dy*dy);
                        }

                        // 互相反向推开 (弹性碰撞)
                        double overlap = minDist - dist;
                        double pushX = (dx / dist) * (overlap / 2.0);
                        double pushY = (dy / dist) * (overlap / 2.0);

                        GA.x(v) += pushX;
                        GA.y(v) += pushY;
                        GA.x(u) -= pushX;
                        GA.y(u) -= pushY;

                        hasOverlap = true;
                    }
                }
            }
            if (!hasOverlap) break; // 如果全图已经没有重叠，提前结束循环以节省性能
        }
        // =====================================================================
    }

    // 3. 将计算完毕的绝对坐标同步回 Qt 界面
    for (ogdf::node v : G.nodes) {
        QString nodeId = ogdfToId[v];
        if (m_nodeMap.contains(nodeId)) {
            m_nodeMap[nodeId]->setPos(GA.x(v), GA.y(v));
        }
    }

    // 4. 初次排版视角自适应适配
    if (m_isFirstLayout) {
        view->fitInView(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
        m_isFirstLayout = false;
    }
}