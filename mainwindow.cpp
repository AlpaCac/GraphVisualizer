#include "mainwindow.h"
#include <QVBoxLayout>
#include <QScrollBar>
#include <QDebug>
#include <cmath>
#include <QRandomGenerator> // 【核心引入】：随机数发生器，用于打破奇点
#include <ogdf/misclayout/CircularLayout.h>
#include <QCoreApplication> // 确保包含了这个头文件
#include <QGraphicsProxyWidget>

// 在开头补充需要的头文件
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QtCore/qmath.h>
#include <QDir>

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

    // ==========================================================
    // 【修改】：将 SingleSelection 改为 MultiSelection
    // 在这个模式下，鼠标点击任何项都会自动进行状态切换（选中/反选），且支持多选
    m_taskListWidget->setSelectionMode(QAbstractItemView::MultiSelection);

    // 【核心修复】：极致压缩行高 + 深度定制悬浮式科幻滚动条
    m_taskListWidget->setStyleSheet(
        "QListWidget { "
        "   background-color: #111b24; "
        "   color: #bdc3c7; "
        "   border: 1px solid #2c3e50; "
        "   padding: 4px; "
        "   font-size: 13px; "
        "   outline: none; "
        "}"
        "QListWidget::item { "
        "   padding: 4px 8px; "
        "   margin-bottom: 2px; "
        "   background-color: #1a252f; "
        "   border-radius: 3px; "
        "   border: none; "
        "}"
        "QListWidget::item:selected { "
        "   background-color: #2980b9; "
        "   color: white; "
        "}"
        /* ======================================================= */
        /* 【新增】：极简科技风滚动条 (隐藏原版粗糙的上下箭头，改为细长圆角条) */
        /* ======================================================= */
        "QScrollBar:vertical { "
        "   border: none; "
        "   background-color: transparent; " // 轨道透明
        "   width: 8px; "                    // 滚动条极窄化，仅 8 像素宽
        "   margin: 0px 0px 0px 0px; "
        "}"
        "QScrollBar::handle:vertical { "
        "   background-color: #34495e; "     // 滑块颜色：高级深灰蓝
        "   min-height: 30px; "              // 滑块最小高度
        "   border-radius: 4px; "            // 圆角设计
        "}"
        "QScrollBar::handle:vertical:hover { "
        "   background-color: #3498db; "     // 鼠标悬浮时亮起科技蓝
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
        "   border: none; "
        "   background: none; "
        "   height: 0px; "                   // 彻底隐藏上下默认的点击小箭头
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
        "   background: none; "              // 隐藏轨道背景，防止出现白色底块
        "}"
        );


    taskLayout->addWidget(m_taskListWidget);
    sideLayout->addWidget(taskGroup, 1); // 【核心修复】：加上 stretch 因子 1

    // ==============================================================
    // 3. 综合效能实时指标
    // ==============================================================
    QGroupBox *metricsGroup = new QGroupBox("📊 综合效能实时指标", sideBarWidget);
    metricsGroup->setStyleSheet("QGroupBox { font-weight: bold; padding-top: 20px; font-size: 15px; color: #ecf0f1; border: 1px solid #34495e; border-radius: 5px;}");
    QVBoxLayout *metricsLayout = new QVBoxLayout(metricsGroup);
    metricsLayout->setSpacing(12);
    metricsLayout->setContentsMargins(15, 25, 15, 15);

    // 内部 Lambda 辅助函数：用于快速生成统一风格的指标行
    auto createMetricRow = [](const QString& icon, const QString& name, const QString& initValue, QLabel*& valueLabel, QVBoxLayout* parentLayout) {
        QHBoxLayout *row = new QHBoxLayout();

        // 左侧指标名
        QLabel *nameLabel = new QLabel(QString("%1 %2").arg(icon, name));
        nameLabel->setStyleSheet("color: #95a5a6; font-size: 14px;");

        // 右侧数值 (使用等宽字体和亮绿色突出显示)
        valueLabel = new QLabel(initValue);
        valueLabel->setStyleSheet("color: #2ecc71; font-weight: bold; font-size: 16px; font-family: Consolas, monospace;");
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        row->addWidget(nameLabel);
        row->addStretch();
        row->addWidget(valueLabel);
        parentLayout->addLayout(row);

        // 底部加一条暗色分割线
        QFrame *line = new QFrame();
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        line->setStyleSheet("background-color: #2c3e50; max-height: 1px;");
        parentLayout->addWidget(line);
    };

    // 依次创建 5 个核心指标
    createMetricRow("⏱", "网络平均时延", "12.4 ms", m_lblLatency, metricsLayout);
    createMetricRow("🔗", "全网连通度", "100.0 %", m_lblConnectivity, metricsLayout);
    createMetricRow("🛡", "链路可靠性", "99.999 %", m_lblReliability, metricsLayout);
    createMetricRow("🌊", "实时吞吐量", "125.0 Gbps", m_lblThroughput, metricsLayout);
    createMetricRow("💰", "预估组网成本", "¥ 42,500", m_lblCost, metricsLayout);

    // 去掉内部拉伸，并去掉权重参数 2，让 GroupBox 完美紧贴内容的高度
    sideLayout->addWidget(metricsGroup);

    // ==============================================================
    // 4. 综合效能评估模型 (双策略对比)
    // ==============================================================
    QGroupBox *evalGroup = new QGroupBox("📈 综合效能评估模型", sideBarWidget);
    evalGroup->setStyleSheet("QGroupBox { font-weight: bold; padding-top: 20px; font-size: 15px; color: #ecf0f1; border: 1px solid #34495e; border-radius: 5px;}");
    QVBoxLayout *evalLayout = new QVBoxLayout(evalGroup);
    evalLayout->setSpacing(15);
    evalLayout->setContentsMargins(15, 25, 15, 15);

    // 内部 Lambda 辅助函数：生成高逼格的策略对比卡片 (修复高度与对齐)
    auto createStrategyBlock = [](const QString& title, const QString& initScore, const QString& scoreColor, QLabel*& scoreLabel, QVBoxLayout* parentLayout) {
        QFrame *card = new QFrame();
        // 【修改 1】：去掉 QFrame 自带的 padding，完全交由 Layout 的 Margin 精确控制
        card->setStyleSheet("QFrame { background-color: #1a252f; border-radius: 6px; }");

        QHBoxLayout *cardLayout = new QHBoxLayout(card);
        // 【修改 2】：大幅度压缩上下的边距 (从 10 降到 6)，让高度变得紧凑
        cardLayout->setContentsMargins(15, 6, 15, 6);

        // 左侧策略名称
        QLabel *titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("color: #bdc3c7; font-size: 13px; font-weight: normal;");
        // 【修改 3】：明确要求左侧文字垂直居中
        titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        // 右侧策略跑分
        scoreLabel = new QLabel(initScore);
        scoreLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 16px; font-family: Consolas, monospace;").arg(scoreColor));
        // 【修改 3】：明确要求右侧数字垂直居中
        scoreLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        // 【修改 4】：在加入布局时，再次施加 Qt::AlignVCenter 锚点约束，确保绝对齐平
        cardLayout->addWidget(titleLabel, 1, Qt::AlignVCenter);
        cardLayout->addWidget(scoreLabel, 0, Qt::AlignVCenter);

        parentLayout->addWidget(card);
    };

    // 【修改 5】：去掉之前代码里带的换行符 \n(Baseline)，直接用纯中文字符串，高度更纯粹
    createStrategyBlock("场景驱动的多指标运行效能评估", "78.5", "#f39c12", m_lblScoreBaseline, evalLayout);
    createStrategyBlock("结构驱动的拓扑鲁棒性效能评估", "96.2", "#00d2d3", m_lblScoreAdvanced, evalLayout);

    evalLayout->addStretch();
    sideLayout->addWidget(evalGroup);
    sideLayout->addStretch();

    // ==============================================================
    // 7. 底部核心控制操作台 (2x2 网格)
    // ==============================================================
    QGridLayout *buttonLayout = new QGridLayout();
    buttonLayout->setSpacing(12); // 按钮之间的间距

    m_btnPhysical = new QPushButton("🌐 物理拓扑", sideBarWidget);
    m_btnLogical  = new QPushButton("🔗 逻辑拓扑", sideBarWidget);
    // 【新增】：两个全新的优化策略按钮
    m_btnOptimizeLatency     = new QPushButton("⚡ 低时延优先", sideBarWidget);
    m_btnOptimizeReliability = new QPushButton("🛡 高可靠优先", sideBarWidget);
    m_btnDestroy  = new QPushButton("💥 损毁模拟", sideBarWidget);

    // 【新增】：切换场景按钮
    m_btnSwitchScene = new QPushButton("🗺️ 切换场景 (当前: 场景 1)", sideBarWidget);

    m_btnSimulateMove = new QPushButton("🚶 模拟移动", sideBarWidget);

    // 标准科技蓝按钮样式
    QString btnStyle =
        "QPushButton { "
        "   background-color: #2980b9; "
        "   color: white; "
        "   border: 1px solid #3498db; "
        "   border-radius: 6px; "
        "   padding: 12px; "
        "   font-weight: bold; "
        "   font-size: 14px; "
        "   font-family: 'Microsoft YaHei'; "
        "}"
        "QPushButton:hover { "
        "   background-color: #3498db; "
        "   border: 1px solid #5dade2; "
        "}"
        "QPushButton:pressed { "
        "   background-color: #1f618d; "
        "}";

    // 危险操作红色警告样式
    QString btnDangerStyle =
        "QPushButton { "
        "   background-color: rgba(192, 57, 43, 0.8); "
        "   color: #ecf0f1; "
        "   border: 1px solid #e74c3c; "
        "   border-radius: 6px; "
        "   padding: 12px; "
        "   font-weight: bold; "
        "   font-size: 14px; "
        "   font-family: 'Microsoft YaHei'; "
        "}"
        "QPushButton:hover { "
        "   background-color: #e74c3c; "
        "   border: 1px solid #ff7675; "
        "}"
        "QPushButton:pressed { "
        "   background-color: #922b21; "
        "}";

    // 【新增】：给场景切换按钮一个更深邃的高级灰蓝色涂装
    QString btnSceneStyle =
        "QPushButton { "
        "   background-color: #34495e; "
        "   color: white; "
        "   border: 1px solid #7f8c8d; "
        "   border-radius: 6px; "
        "   padding: 12px; "
        "   font-weight: bold; "
        "   font-size: 14px; "
        "   font-family: 'Microsoft YaHei'; "
        "}"
        "QPushButton:hover { background-color: #7f8c8d; }"
        "QPushButton:pressed { background-color: #2c3e50; }";

    // 应用样式
    m_btnPhysical->setStyleSheet(btnStyle);
    m_btnLogical->setStyleSheet(btnStyle);
    m_btnOptimizeLatency->setStyleSheet(btnStyle);       // 新按钮应用样式
    m_btnOptimizeReliability->setStyleSheet(btnStyle);   // 新按钮应用样式
    m_btnDestroy->setStyleSheet(btnDangerStyle); // 特殊红色涂装
    m_btnSwitchScene->setStyleSheet(btnSceneStyle); // 应用新样式
    m_btnSimulateMove->setStyleSheet(btnStyle); // 复用常规按钮的样式

    // 鼠标悬停变小手
    m_btnPhysical->setCursor(Qt::PointingHandCursor);
    m_btnLogical->setCursor(Qt::PointingHandCursor);
    m_btnOptimizeLatency->setCursor(Qt::PointingHandCursor);
    m_btnOptimizeReliability->setCursor(Qt::PointingHandCursor);
    m_btnDestroy->setCursor(Qt::PointingHandCursor);
    m_btnSwitchScene->setCursor(Qt::PointingHandCursor);
    m_btnSimulateMove->setCursor(Qt::PointingHandCursor);

    // ==============================================================
    // 重新排列极其对称的 4x2 网格布局
    // ==============================================================
    buttonLayout->addWidget(m_btnSwitchScene,         0, 0, 1, 2); // 第 0 行，横跨 2 列
    buttonLayout->addWidget(m_btnPhysical,            1, 0);       // 第 1 行左
    buttonLayout->addWidget(m_btnLogical,             1, 1);       // 第 1 行右
    buttonLayout->addWidget(m_btnOptimizeLatency,     2, 0);       // 第 2 行左
    buttonLayout->addWidget(m_btnOptimizeReliability, 2, 1);       // 第 2 行右
    buttonLayout->addWidget(m_btnDestroy,             3, 0);       // 第 3 行左
    buttonLayout->addWidget(m_btnSimulateMove,        3, 1);       // 第 3 行右

    // 将网格添加到侧边栏最底部
    sideLayout->addLayout(buttonLayout);
    // ==============================================================
    // 组装切分器并强设宽度
    // ==============================================================
    m_splitter->addWidget(sideBarWidget);
    m_splitter->addWidget(view);

    // 【核心修复】：调窄侧边栏，从 450 降到 350
    QList<int> sizes;
    sizes << 350 << 1250;
    m_splitter->setSizes(sizes);

    // ==============================================================
    // 5. 画布左上角悬浮标题
    // ==============================================================
    m_canvasTitle = new QLabel("物理拓扑", view);
    // 设置半透明深色背景和高亮文字，极具科幻感
    m_canvasTitle->setStyleSheet(
        "QLabel { "
        "   color: #34495e; "  /* 高级的深蓝灰色，比纯黑更柔和 */
        "   font-size: 32px; " /* 稍微放大一点显得更大气 */
        "   font-weight: 900; " /* 超粗体 */
        "   font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif; "
        "   background-color: transparent; " /* 【核心修复】：彻底去掉背景色，完全透明融入画布 */
        "   letter-spacing: 4px; " /* 增加一点字间距，提升呼吸感 */
        "}"
        );
    // 将它固定在距离画布左上角 (20, 20) 的位置
    m_canvasTitle->move(20, 20);

    // ==============================================================
    // 6. 右上角图元属性悬浮侦察面板 (高颜值重构版)
    // ==============================================================
    m_infoBox = new QFrame(view);
    // 增加毛玻璃质感、更现代的边框和圆角
    m_infoBox->setStyleSheet(
        "QFrame { "
        "   background-color: rgba(15, 23, 30, 245); " // 更深邃的暗蓝灰底色，带微透明
        "   border: 1px solid rgba(52, 152, 219, 0.5); " // 带有通透感的科技蓝边框
        "   border-radius: 10px; " // 更柔和的圆角
        "}"
        );
    // 【修改】：将 setFixedSize(320, 290) 换成 setFixedWidth，解除高度的死锁
    m_infoBox->setFixedWidth(320);
    m_infoBox->hide();

    QVBoxLayout *infoLayout = new QVBoxLayout(m_infoBox);
    infoLayout->setContentsMargins(18, 15, 18, 15); // 增加内边距，让内容有呼吸感
    infoLayout->setSpacing(5);

    m_infoTitle = new QLabel("详细信息", m_infoBox);
    // 标题美化：亮青色文字，搭配科技感底部渐变/实线分割
    m_infoTitle->setStyleSheet(
        "QLabel { "
        "   font-weight: bold; "
        "   font-size: 16px; "
        "   color: #00d2d3; " // 科幻感亮青色
        "   border: none; "
        "   border-bottom: 2px solid rgba(52, 152, 219, 0.3); "
        "   padding-bottom: 8px; "
        "}"
        );

    m_infoContent = new QLabel("", m_infoBox);
    // 内容容器美化：去掉背景和边框，完全融入底层 Frame
    m_infoContent->setStyleSheet(
        "QLabel { "
        "   background: transparent; "
        "   border: none; "
        "   font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "}"
        );
    m_infoContent->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    infoLayout->addWidget(m_infoTitle);
    infoLayout->addWidget(m_infoContent);
// infoLayout->addStretch(); // 【把这行彻底删掉！】取消多余的推挤空白

    m_optimizerProcess = new QProcess(this);

    // 绑定槽函数 (删掉旧的 Optimize 绑定，加入新的)
    connect(m_btnPhysical, &QPushButton::clicked, this, &MainWindow::onPhysicalTopologyClicked);
    connect(m_btnLogical, &QPushButton::clicked, this, &MainWindow::onLogicalTopologyClicked);
    connect(m_btnOptimizeLatency, &QPushButton::clicked, this, &MainWindow::onOptimizeLatencyClicked);
    connect(m_btnOptimizeReliability, &QPushButton::clicked, this, &MainWindow::onOptimizeReliabilityClicked);
    connect(m_btnDestroy, &QPushButton::clicked, this, &MainWindow::onDestroyButtonClicked);
    connect(m_btnSwitchScene, &QPushButton::clicked, this, &MainWindow::onSwitchSceneClicked);
    connect(m_btnSimulateMove, &QPushButton::clicked, this, &MainWindow::onSimulateMoveClicked);
    connect(m_taskListWidget, &QListWidget::itemSelectionChanged, this, &MainWindow::onTaskSelectionChanged);
    connect(scene, &QGraphicsScene::selectionChanged, this, &MainWindow::onSceneSelectionChanged);

} // 构造函数结束
MainWindow::~MainWindow() {}

void MainWindow::addNode(const QString& nodeId, int type, const QString& label, int shape, const QColor& color, int size, double x, double y) {
    if (m_nodeMap.contains(nodeId)) return;

    GraphNode *node = new GraphNode(nodeId, type, nodeId); // 或使用 shortLabel
    node->setData(0, label);
    node->setStyle(color, shape, size);

    // ==========================================================
    // 【核心】：不再等待布局引擎，直接设置坐标
    // ==========================================================
    node->setPos(x, y);

    scene->addItem(node);
    m_nodeMap.insert(nodeId, node);
}
void MainWindow::updateNodeStyle(const QString& nodeId, const QColor& color, int shape, int size) {
    if (m_nodeMap.contains(nodeId)) {
        m_nodeMap[nodeId]->setStyle(color, shape, size);
    }
}

void MainWindow::addEdge(const QString& sourceId, const QString& destId, int type, const QString& label) {
    if (!m_nodeMap.contains(sourceId) || !m_nodeMap.contains(destId)) return;
    GraphNode *source = m_nodeMap[sourceId];
    GraphNode *dest = m_nodeMap[destId];
    // 【修改】：将 linkType 传给新建的 GraphEdge
    GraphEdge *edge = new GraphEdge(source, dest, type);

    // ==========================================================
    // 【核心新增】：把后端传过来的 8 项链路详细指标，藏进连线的 0 号位字典
    // ==========================================================
    edge->setData(0, label);

    scene->addItem(edge);
    source->addEdge(edge);
    dest->addEdge(edge);
    m_edges.append({sourceId, destId, type});

    m_edgeItemMap.insert(sourceId + "->" + destId, edge);
}
void MainWindow::clearGraph() {
    scene->clear(); // 这句会物理销毁图上的所有线和节点
    m_nodeMap.clear();
    m_edges.clear();
    m_edgeItemMap.clear();

    // ==========================================================
    // 【关键修复】：清除旧的发光绿线指针，防止切换图表时崩溃
    // ==========================================================
    m_flowEdges.clear();
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

// 【修改】：函数签名增加 const QStringList& routingPath
void MainWindow::addTask(const QString& taskId, const QString& taskName, bool isCompliant, const QString& srcId, const QString& dstId, const QStringList& routingPath) {
    Q_UNUSED(taskId);
    if (!m_taskListWidget) return;

    QListWidgetItem *item = new QListWidgetItem(m_taskListWidget);
    item->setSizeHint(QSize(0, 32));

    // ==========================================================
    // 【核心新增】：利用 Qt 的 UserRole 隐藏字典，将起点和终点存入列表项
    // ==========================================================
    item->setData(Qt::UserRole, srcId);
    item->setData(Qt::UserRole + 1, dstId);
    item->setData(Qt::UserRole + 2, routingPath);

    // ... (下面创建 QWidget 和徽章的代码保持不变) ...

    // 2. 创建自定义 Widget 容器
    QWidget *rowWidget = new QWidget();
    // 设置背景透明，这样鼠标选中时原本的 #2980b9 蓝色高亮就能透出来
    rowWidget->setStyleSheet("background: transparent;");

    // 3. 使用水平布局实现“左边文字，右边徽章”
    QHBoxLayout *layout = new QHBoxLayout(rowWidget);
    layout->setContentsMargins(8, 2, 8, 2); // 紧凑的内边距
    layout->setSpacing(5);

    // 4. 左侧：极简业务流名称
    QLabel *nameLabel = new QLabel("· " + taskName);
    nameLabel->setStyleSheet("color: #bdc3c7; font-family: Consolas, monospace; font-size: 13px; border: none;");

    // 5. 右侧：状态达标徽章 (Badge)
    QLabel *badgeLabel = new QLabel(isCompliant ? "达标" : "未达标");
    badgeLabel->setAlignment(Qt::AlignCenter);

    // ==========================================================
    // 【核心修复】：强制锁定徽章的宽度为 55 像素，确保所有徽章完美右对齐
    // ==========================================================
    badgeLabel->setFixedWidth(55);

    // 给达标/未达标赋予不同的赛博朋克发光样式
    if (isCompliant) {
        // ... 原有的样式代码保持不变 ...
        badgeLabel->setStyleSheet(
            "background-color: rgba(46, 204, 113, 0.15); " // 半透明微光绿背景
            "color: #2ecc71; "                             // 荧光绿文字
            "border: 1px solid #2ecc71; "                  // 实体绿边框
            "border-radius: 4px; "
            "padding: 1px 6px; "
            "font-size: 11px; "
            "font-weight: bold;"
            );
    } else {
        badgeLabel->setStyleSheet(
            "background-color: rgba(231, 76, 60, 0.15); "  // 半透明微光红背景
            "color: #e74c3c; "                             // 荧光红文字
            "border: 1px solid #e74c3c; "                  // 实体红边框
            "border-radius: 4px; "
            "padding: 1px 6px; "
            "font-size: 11px; "
            "font-weight: bold;"
            );
    }

    // 6. 组装：让文字占据所有多余空间(stretch=1)，徽章靠右(stretch=0)
    layout->addWidget(nameLabel, 1);
    layout->addWidget(badgeLabel, 0);

    // 7. 将这个自定义的复杂 Widget 塞进 List 项里
    m_taskListWidget->addItem(item);
    m_taskListWidget->setItemWidget(item, rowWidget);
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
    // 清空列表
    if (m_taskListWidget) {
        m_taskListWidget->clear();
    }
}

void MainWindow::applyLayout() {
    // 既然坐标由 JSON 决定，这里不需要计算
    // 只需要确保画布视口覆盖所有节点即可
    if (!m_nodeMap.isEmpty()) {
        view->fitInView(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
    }
}
void MainWindow::updateMetrics(const QString& latency, const QString& connectivity, const QString& reliability, const QString& throughput, const QString& cost) {
    if (m_lblLatency) m_lblLatency->setText(latency);
    if (m_lblConnectivity) m_lblConnectivity->setText(connectivity);
    if (m_lblReliability) m_lblReliability->setText(reliability);
    if (m_lblThroughput) m_lblThroughput->setText(throughput);
    if (m_lblCost) m_lblCost->setText(cost);
}
void MainWindow::updateEvaluation(const QString& scoreBaseline, const QString& scoreAdvanced) {
    if (m_lblScoreBaseline) m_lblScoreBaseline->setText(scoreBaseline);
    if (m_lblScoreAdvanced) m_lblScoreAdvanced->setText(scoreAdvanced);
}
// ==========================================================
// 处理点击选中图元的逻辑
// ==========================================================
void MainWindow::onSceneSelectionChanged() {
    QList<QGraphicsItem*> items = scene->selectedItems();

    if (items.isEmpty()) {
        m_infoBox->hide(); // 依然保留：点击空白处隐藏右上角的详细信息面板

        // ==========================================================
        // 【修改】：删掉或注释掉 m_taskListWidget->clearSelection();
        // 这样点击拓扑图空白处时，左侧的业务流会继续保持选中，绿线也会死死钉在图上！
        // ==========================================================

        return;
    }

    // ... 下面是点中节点和连线的原有代码保持不变 ...

    // ... 下面是点中节点和连线的原有代码 ...
    QGraphicsItem *item = items.first();

    if (item->type() == GraphNode::Type) {
        // --- 点中了节点 ---
        GraphNode *node = qgraphicsitem_cast<GraphNode*>(item);
        m_infoTitle->setText("🖥 节点详细状态");

        QString fullLabel = node->data(0).toString();
        // 以 " | " 切割出每一项
        QStringList parts = fullLabel.split(" | ");

        // 【核心视觉升级】：使用 HTML Table 实现完美对齐和行距
        QString html = "<table width='100%' style='margin-top: 8px; line-height: 1.8; font-size: 13px;'>";
        for (const QString& part : parts) {
            QStringList kv = part.split(": ");
            if (kv.size() >= 2) {
                // Key (浅灰色靠左) + Value (亮白色/加粗靠右)
                html += QString("<tr>"
                                "<td style='color: #95a5a6;'>%1</td>"
                                "<td style='color: #ecf0f1; font-weight: bold; text-align: right;'>%2</td>"
                                "</tr>").arg(kv[0], kv[1]);
            }
        }
        html += "</table>";
        m_infoContent->setText(html);

    } else if (item->type() == GraphEdge::Type) {
        // --- 点中了连线 ---
        GraphEdge *edge = qgraphicsitem_cast<GraphEdge*>(item);
        m_infoTitle->setText("🔗 物理链路状态");

        // 从连线中读取刚刚藏好的 8 项数据文本
        QString fullLabel = edge->data(0).toString();

        QString html = "<table width='100%' style='margin-top: 8px; line-height: 1.8; font-size: 13px;'>";
        // 增加一行默认在线状态
        html += "<tr><td style='color: #95a5a6;'>实时状态</td><td style='color: #2ecc71; font-weight: bold; text-align: right;'>🟢 链路正常</td></tr>";

        if (!fullLabel.isEmpty()) {
            QStringList parts = fullLabel.split(" | ");
            for (const QString& part : parts) {
                QStringList kv = part.split(": ");
                if (kv.size() >= 2) {
                    html += QString("<tr>"
                                    "<td style='color: #95a5a6;'>%1</td>"
                                    "<td style='color: #ecf0f1; font-weight: bold; text-align: right;'>%2</td>"
                                    "</tr>").arg(kv[0], kv[1]);
                }
            }
        }

        html += "</table>";
        m_infoContent->setText(html);
    }

    // ==========================================================
    // 【核心新增】：让 Qt 根据刚刚塞进去的 HTML 文字，自动收缩并贴合高度
    // ==========================================================
    m_infoBox->adjustSize();

    // 将面板移动到右上角，并保持 25 像素的边距
    m_infoBox->move(view->width() - m_infoBox->width() - 25, 25);
    m_infoBox->show();
}

// ==========================================================
// 确保改变窗口大小时，悬浮框依然死死钉在右上角
// ==========================================================
void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (m_infoBox && m_infoBox->isVisible() && view) {
        m_infoBox->move(view->width() - m_infoBox->width() - 25, 25);
    }
}
// ==========================================================
// 处理业务流队列的点击联动 (支持同时绘制多条发光连线)
// ==========================================================
// ==========================================================
// 处理业务流队列的点击联动 (支持同时绘制多条发光连线)
// ==========================================================
void MainWindow::onTaskSelectionChanged() {
    // 1. 每次发生选择改变时，先清除图上之前画的所有连线
    for (QGraphicsLineItem* edge : m_flowEdges) {
        scene->removeItem(edge);
        delete edge;
    }
    m_flowEdges.clear();

    // 2. 获取当前所有被选中的任务项 (可能是一个，也可能是多个)
    QList<QListWidgetItem*> items = m_taskListWidget->selectedItems();
    if (items.isEmpty()) return;

    //统一的高亮画笔配置（紫色虚线，圆滑端点）
    QPen glowPen(QColor("#9b59b6"), 4, Qt::DashLine, Qt::RoundCap);

    // 3. 遍历所有选中的任务，为它们一一拉起高亮轨迹
    for (QListWidgetItem* item : items) {
        // 从列表项中提取出刚刚存进去的路径数组
        QStringList routingPath = item->data(Qt::UserRole + 2).toStringList();

        // 【智能降级】：如果后端算法没有给出路由路径（或路径为空/长度不足），退化为起点连终点的直线
        if (routingPath.size() < 2) {
            QString srcId = item->data(Qt::UserRole).toString();
            QString dstId = item->data(Qt::UserRole + 1).toString();

            if (m_nodeMap.contains(srcId) && m_nodeMap.contains(dstId)) {
                GraphNode* srcNode = m_nodeMap[srcId];
                GraphNode* dstNode = m_nodeMap[dstId];
                QGraphicsLineItem* flowEdge = new QGraphicsLineItem(QLineF(srcNode->scenePos(), dstNode->scenePos()));
                flowEdge->setPen(glowPen);
                flowEdge->setZValue(5);
                scene->addItem(flowEdge);
                m_flowEdges.append(flowEdge);
            }
        }
        // 【核心渲染】：如果有完整的路由路径，则逐段连接
        else {
            for (int i = 0; i < routingPath.size() - 1; ++i) {
                QString currNodeId = routingPath[i];
                QString nextNodeId = routingPath[i + 1];

                // 确保两个途经节点在当前画布上都存在
                if (m_nodeMap.contains(currNodeId) && m_nodeMap.contains(nextNodeId)) {
                    GraphNode* currNode = m_nodeMap[currNodeId];
                    GraphNode* nextNode = m_nodeMap[nextNodeId];

                    QGraphicsLineItem* segmentEdge = new QGraphicsLineItem(QLineF(currNode->scenePos(), nextNode->scenePos()));
                    segmentEdge->setPen(glowPen);
                    segmentEdge->setZValue(5);

                    scene->addItem(segmentEdge);
                    m_flowEdges.append(segmentEdge); // 加入列表统一管理，方便销毁
                }
            }
        }
    }
}
// ==========================================================
// 底部控制台按钮联动逻辑
// ==========================================================
// 确保函数名前面带有 MainWindow::
void MainWindow::onPhysicalTopologyClicked() {
    if (m_canvasTitle) m_canvasTitle->setText(m_isDestroyed ? "物理拓扑" : "物理拓扑");
    // 【修改】：拼装文件夹前缀
    QString fileName = QString::number(m_currentSceneId) + "/" + (m_isDestroyed ? "wuli_sunhui.json" : "wuli.json");
    emit requestLoadJson(fileName);
}

void MainWindow::onLogicalTopologyClicked() {
    if (m_canvasTitle) m_canvasTitle->setText(m_isDestroyed ? "逻辑拓扑" : "逻辑拓扑");
    QString fileName = QString::number(m_currentSceneId) + "/" + (m_isDestroyed ? "luoji_sunhui.json" : "luoji.json");
    // 【修改】：不再直接发信号，而是调用底层计算后再读取
    runBackendAndLoad(fileName);
}
// ==========================================================
// 拓扑优化：低时延优先
// ==========================================================
void MainWindow::onOptimizeLatencyClicked() {
    if (m_canvasTitle) m_canvasTitle->setText(m_isDestroyed ? "低时延优化" : "低时延优化");
    QString fileName = QString::number(m_currentSceneId) + "/" + (m_isDestroyed ? "youhua1_sunhui.json" : "youhua1.json");
    // 【修改】：调度后端后读取
    runBackendAndLoad(fileName);
}

// ==========================================================
// 拓扑优化：高可靠优先
// ==========================================================
void MainWindow::onOptimizeReliabilityClicked() {
    if (m_canvasTitle) m_canvasTitle->setText(m_isDestroyed ? "高可靠优化" : "高可靠优化");
    QString fileName = QString::number(m_currentSceneId) + "/" + (m_isDestroyed ? "youhua2_sunhui.json" : "youhua2.json");
    // 【修改】：调度后端后读取
    runBackendAndLoad(fileName);
}
void MainWindow::showLoading(bool visible) {
    if (visible) {
        // 1. 如果还没有创建，就创建一个贴在 view 表面的 Label
        if (!m_loadingOverlay) {
            m_loadingOverlay = new QLabel("⚡ 优化计算中，请稍候...", view);
            m_loadingOverlay->setStyleSheet(
                "QLabel { "
                "   color: #ecf0f1; "
                "   font-size: 24px; "
                "   font-weight: bold; "
                "   background: rgba(15, 23, 30, 220); " // 带有科幻感的半透明深色背景
                "   border: 2px solid #3498db; "         // 科技蓝边框
                "   padding: 30px; "
                "   border-radius: 12px; "
                "}"
                );
            m_loadingOverlay->setAlignment(Qt::AlignCenter);
        }

        // 2. 每次显示前，重新计算并居中（防止窗口大小改变过）
        m_loadingOverlay->adjustSize();
        int x = (view->width() - m_loadingOverlay->width()) / 2;
        int y = (view->height() - m_loadingOverlay->height()) / 2;
        m_loadingOverlay->move(x, y);

        // 3. 强制显示并提升到最顶层
        m_loadingOverlay->show();
        m_loadingOverlay->raise();

    } else {
        // 4. 隐藏遮罩，但不销毁它，下次可以直接复用
        if (m_loadingOverlay) {
            m_loadingOverlay->hide();
        }
    }
}
// ==========================================================
// 损毁与恢复按钮的 Toggle 联动逻辑
// ==========================================================
void MainWindow::onDestroyButtonClicked() {
    QString prefix = QString::number(m_currentSceneId) + "/";
    if (!m_isDestroyed) {
        // 当前为【正常状态】 -> 触发【损毁模拟】
        if (m_canvasTitle) m_canvasTitle->setText("物理拓扑 (链路故障)");
        m_btnDestroy->setText("✨ 故障恢复");
        // (注：此处保留您原有的按钮样式配置代码 btnSuccessStyle ...)
        QString btnSuccessStyle = "QPushButton { background-color: rgba(46, 204, 113, 0.8); color: #ffffff; border: 1px solid #2ecc71; border-radius: 6px; padding: 12px; font-weight: bold; font-size: 14px; font-family: 'Microsoft YaHei'; } QPushButton:hover { background-color: #2ecc71; border: 1px solid #58d68d; } QPushButton:pressed { background-color: #27ae60; }";
        m_btnDestroy->setStyleSheet(btnSuccessStyle);

        m_isDestroyed = true;
        // 【修改】：此时已是损毁状态，后端需要读取 wuli_sunhui.json 作为 config 进行计算，最后加载 wuli_sunhui.json
        runBackendAndLoad(prefix + "wuli_sunhui.json");

    } else {
        // 当前为【损毁状态】 -> 触发【故障恢复】
        if (m_canvasTitle) m_canvasTitle->setText("物理拓扑");
        m_btnDestroy->setText("💥 损毁模拟");
        // (注：此处保留您原有的按钮样式配置代码 btnDangerStyle ...)
        QString btnDangerStyle = "QPushButton { background-color: rgba(192, 57, 43, 0.8); color: #ecf0f1; border: 1px solid #e74c3c; border-radius: 6px; padding: 12px; font-weight: bold; font-size: 14px; font-family: 'Microsoft YaHei'; } QPushButton:hover { background-color: #e74c3c; border: 1px solid #ff7675; } QPushButton:pressed { background-color: #922b21; }";
        m_btnDestroy->setStyleSheet(btnDangerStyle);

        m_isDestroyed = false;
        // 【修改】：此时已恢复正常，后端需要以 wuli.json 作为 config 计算，最后加载 wuli.json
        runBackendAndLoad(prefix + "wuli.json");
    }
}
// ==========================================================
// 场景切换逻辑
// ==========================================================
void MainWindow::onSwitchSceneClicked() {
    // 1. 文件夹编号从 1 循环到 3
    m_currentSceneId++;
    if (m_currentSceneId > 3) {
        m_currentSceneId = 1;
    }

    // 2. 更新按钮文字
    m_btnSwitchScene->setText(QString("🗺️ 切换场景 (当前: 场景 %1)").arg(m_currentSceneId));

    // 3. 丝滑联动：切换场景后，自动重新加载当前正在查看的视图图层
    if (m_canvasTitle->text().contains("逻辑")) {
        onLogicalTopologyClicked();
    } else if (m_canvasTitle->text().contains("时延")) {      // 【新增】判断时延
        onOptimizeLatencyClicked();
    } else if (m_canvasTitle->text().contains("可靠")) {      // 【新增】判断可靠
        onOptimizeReliabilityClicked();
    } else {
        onPhysicalTopologyClicked();
    }
}
// ==========================================================
// 模拟移动逻辑 (读取当前文件 R 属性并产生随机抖动)
// ==========================================================
void MainWindow::onSimulateMoveClicked() {
    // 1. 智能推断当前应该读取哪个 JSON 文件
    // 1. 智能推断当前应该读取哪个 JSON 文件
    QString fileName = "";
    QString title = m_canvasTitle->text();
    if (title.contains("物理")) {
        fileName = QString::number(m_currentSceneId) + "/" + (m_isDestroyed ? "wuli_sunhui.json" : "wuli.json");
    } else if (title.contains("逻辑")) {
        fileName = QString::number(m_currentSceneId) + "/" + (m_isDestroyed ? "luoji_sunhui.json" : "luoji.json");
    } else if (title.contains("时延")) {  // 【修改】：对应 youhua1
        fileName = QString::number(m_currentSceneId) + "/" + (m_isDestroyed ? "youhua1_sunhui.json" : "youhua1.json");
    } else if (title.contains("可靠")) {  // 【修改】：对应 youhua2
        fileName = QString::number(m_currentSceneId) + "/" + (m_isDestroyed ? "youhua2_sunhui.json" : "youhua2.json");
    }
    // ==========================================================
    // 2. 智能拼接文件路径 (完美兼容 Qt 开发环境与打包部署环境)
    // ==========================================================
    // 获取 .exe 主程序当前所在的绝对路径
    QString basePath = QCoreApplication::applicationDirPath();

    // 优先尝试 1：打包发布时的标准路径 (exe 旁边的 data 文件夹)
    QString filePath = basePath + "/data/" + fileName;

    if (!QFile::exists(filePath)) {
        // 备用尝试 2：Qt Creator 默认的 build 目录 (往上跳两级)
        filePath = basePath + "/../../data/" + fileName;
    }
    if (!QFile::exists(filePath)) {
        // 备用尝试 3：部分自定义构建目录 (往上跳一级)
        filePath = basePath + "/../data/" + fileName;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "❌ 模拟移动失败: 无法打开文件读取 R 属性 ->" << filePath;
        qDebug() << "🛑 失败原因:" << file.errorString();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    QJsonArray nodesArr = doc.object()["nodes"].toArray();

    // 3. 遍历 JSON，提取 R 属性，并在画布上找到对应的节点进行移动
    for (int i = 0; i < nodesArr.size(); ++i) {
        QJsonObject nodeObj = nodesArr[i].toObject();
        QString idStr = QString::number(nodeObj["id"].toInt());

        // 兼容大写 R 或小写 r
        double r = 0.0;
        if (nodeObj.contains("R")) {
            r = nodeObj["R"].toDouble();
        } else if (nodeObj.contains("r")) {
            r = nodeObj["r"].toDouble();
        }

        // 如果没有配置 R 或者 R 为 0，则不移动该节点
        if (r <= 0) continue;

        // ==========================================================
        // 【已修正】：将 m_nodes 替换为您真实的变量名 m_nodeMap
        // ==========================================================
        if (m_nodeMap.contains(idStr)) {
            auto nodeItem = m_nodeMap[idStr];

            // 使用极坐标公式，在半径 r 的圆形范围内生成纯随机偏移量
            double angle = QRandomGenerator::global()->generateDouble() * 2 * M_PI; // 0 到 360 度随机方向
            double radius = QRandomGenerator::global()->generateDouble() * r;       // 0 到 r 的随机距离

            double dx = radius * std::cos(angle);
            double dy = radius * std::sin(angle);

            // 获取图元当前位置，叠加随机偏移后重新设置回去
            QPointF currentPos = nodeItem->pos();
            nodeItem->setPos(currentPos.x() + dx, currentPos.y() + dy);
        }
    }
}
// ==========================================================
// 核心机制：异步调度底层 topoopt 算法，完成后自动刷新前端
// ==========================================================
void MainWindow::runBackendAndLoad(const QString& nextFileName) {
    // 1. 防止用户疯狂连点导致进程重入崩溃
    if (m_optimizerProcess->state() == QProcess::Running) {
        qDebug() << "⚠️ 后端算法正在运行中，请耐心等待...";
        return;
    }

    // 2. 显示极具科幻感的加载遮罩
    showLoading(true);

    // 3. 智能解析路径 (完美兼容打包发布与 Qt Creator 调试)
    QString basePath = QCoreApplication::applicationDirPath();
    QString sceneDir = QString::number(m_currentSceneId);

    // 【核心】：根据当前是否处于损毁状态，决定 config 输入哪个文件
    QString configFileName = m_isDestroyed ? "wuli_sunhui.json" : "wuli.json";

    QString exePath = basePath + "/backend/bin/topoopt";
    QString configPath = basePath + "/data/" + sceneDir + "/" + configFileName;
    QString outDir = basePath + "/data/" + sceneDir;

#ifdef Q_OS_WIN
    exePath += ".exe";
#endif

    // 若当前路径找不到配置文件，自动回退到 ../../ 的开发模式路径
    if (!QFile::exists(configPath)) {
        basePath = basePath + "/../..";
        exePath = basePath + "/backend/bin/topoopt";
#ifdef Q_OS_WIN
        exePath += ".exe";
#endif
        configPath = basePath + "/data/" + sceneDir + "/" + configFileName;
        outDir = basePath + "/data/" + sceneDir;
    }

    // 4. 严格按照要求拼接命令行参数
    QStringList args;
    args << "--config" << configPath
         << "--output-dir" << outDir
         << "--pop" << "100"
         << "--gen" << "50"
         << "--mutation" << "0.01"
         << "--seed" << "42";

    // 5. 断开之前可能残留的完成信号绑定，防止多重触发
    m_optimizerProcess->disconnect();

    // 6. 绑定进程结束信号（使用异步 Lambda 表达式）
    connect(m_optimizerProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, nextFileName](int exitCode, QProcess::ExitStatus exitStatus) {

                showLoading(false); // 算法结束，立刻隐藏遮罩

                if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                    qDebug() << "✅ 底层算力引擎执行成功！";
                } else {
                    qDebug() << "❌ 底层算力引擎执行异常！";
                    qDebug() << "错误输出:" << m_optimizerProcess->readAllStandardError();
                }

                // 算法无论如何跑完，最后都向 TestWorker 发射读取新 JSON 文件的信号
                emit requestLoadJson(nextFileName);
            });

    // 7. 正式启动隐藏的 C++ 算法黑盒
    qDebug() << "🚀 正在拉起后端引擎:" << exePath << args.join(" ");
    m_optimizerProcess->start(exePath, args);
}