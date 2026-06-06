#include "mainwindow.h"
#include <QVBoxLayout>
#include <QScrollBar>
#include <QDebug>
#include <cmath>
#include <QRandomGenerator> // 【核心引入】：随机数发生器，用于打破奇点
#include <ogdf/misclayout/CircularLayout.h>
#include <QCoreApplication> // 确保包含了这个头文件
#include <QGraphicsProxyWidget>

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
    m_btnOptimize = new QPushButton("⚡ 拓扑优化", sideBarWidget);
    m_btnDestroy  = new QPushButton("💥 损毁模拟", sideBarWidget);

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

    // 应用样式
    m_btnPhysical->setStyleSheet(btnStyle);
    m_btnLogical->setStyleSheet(btnStyle);
    m_btnOptimize->setStyleSheet(btnStyle);
    m_btnDestroy->setStyleSheet(btnDangerStyle); // 特殊红色涂装

    // 鼠标悬停变小手
    m_btnPhysical->setCursor(Qt::PointingHandCursor);
    m_btnLogical->setCursor(Qt::PointingHandCursor);
    m_btnOptimize->setCursor(Qt::PointingHandCursor);
    m_btnDestroy->setCursor(Qt::PointingHandCursor);

    // 组装到 2x2 网格中
    buttonLayout->addWidget(m_btnPhysical, 0, 0); // 第 1 行，左
    buttonLayout->addWidget(m_btnLogical, 0, 1);  // 第 1 行，右
    buttonLayout->addWidget(m_btnOptimize, 1, 0); // 第 2 行，左
    buttonLayout->addWidget(m_btnDestroy, 1, 1);  // 第 2 行，右

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

    connect(scene, &QGraphicsScene::selectionChanged, this, &MainWindow::onSceneSelectionChanged);
    // 【新增】：监听业务流队列的点击事件
    connect(m_taskListWidget, &QListWidget::itemSelectionChanged, this, &MainWindow::onTaskSelectionChanged);
    // ==========================================================
    // 【新增】：绑定物理与逻辑拓扑按钮的点击事件
    // ==========================================================
    connect(m_btnPhysical, &QPushButton::clicked, this, &MainWindow::onPhysicalTopologyClicked);
    connect(m_btnLogical, &QPushButton::clicked, this, &MainWindow::onLogicalTopologyClicked);

    // 【新增】：连接拓扑优化按钮
    connect(m_btnOptimize, &QPushButton::clicked, this, &MainWindow::onOptimizeTopologyClicked);
    // ==========================================================
    // 【必须补上这一行】：连接损毁模拟按钮！
    // ==========================================================
    connect(m_btnDestroy, &QPushButton::clicked, this, &MainWindow::onDestroyButtonClicked);

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

void MainWindow::addTask(const QString& taskId, const QString& taskName, bool isCompliant, const QString& srcId, const QString& dstId) {
    Q_UNUSED(taskId);
    if (!m_taskListWidget) return;

    QListWidgetItem *item = new QListWidgetItem(m_taskListWidget);
    item->setSizeHint(QSize(0, 32));

    // ==========================================================
    // 【核心新增】：利用 Qt 的 UserRole 隐藏字典，将起点和终点存入列表项
    // ==========================================================
    item->setData(Qt::UserRole, srcId);
    item->setData(Qt::UserRole + 1, dstId);

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
void MainWindow::onTaskSelectionChanged() {
    // 1. 每次发生选择改变时，先清除图上之前画的所有绿线
    for (QGraphicsLineItem* edge : m_flowEdges) {
        scene->removeItem(edge);
        delete edge;
    }
    m_flowEdges.clear();

    // 2. 获取当前所有被选中的任务项 (可能是一个，也可能是多个)
    QList<QListWidgetItem*> items = m_taskListWidget->selectedItems();
    if (items.isEmpty()) return;

    // 3. 遍历所有选中的任务，为它们一一拉起绿色虚线
    for (QListWidgetItem* item : items) {
        QString srcId = item->data(Qt::UserRole).toString();
        QString dstId = item->data(Qt::UserRole + 1).toString();

        if (m_nodeMap.contains(srcId) && m_nodeMap.contains(dstId)) {
            GraphNode* srcNode = m_nodeMap[srcId];
            GraphNode* dstNode = m_nodeMap[dstId];

            QPen glowPen(QColor("#9b59b6"), 4, Qt::DashLine, Qt::RoundCap);
            QGraphicsLineItem* flowEdge = new QGraphicsLineItem(QLineF(srcNode->scenePos(), dstNode->scenePos()));
            flowEdge->setPen(glowPen);
            flowEdge->setZValue(5);

            scene->addItem(flowEdge);
            // 加入到列表中统一管理，方便下次清除
            m_flowEdges.append(flowEdge);
        }
    }
}
// ==========================================================
// 底部控制台按钮联动逻辑
// ==========================================================
// 确保函数名前面带有 MainWindow::
void MainWindow::onPhysicalTopologyClicked() {
    if (m_canvasTitle) {
        m_canvasTitle->setText("物理拓扑");
    }
    emit requestLoadJson("wuli.json");
}

void MainWindow::onLogicalTopologyClicked() {
    if (m_canvasTitle) {
        m_canvasTitle->setText("逻辑拓扑");
    }
    emit requestLoadJson("luoji.json");
}

// ==========================================================
// 拓扑优化按钮点击逻辑
// ==========================================================
void MainWindow::onOptimizeTopologyClicked() {
    if (m_canvasTitle) m_canvasTitle->setText("拓扑优化");

    // 1. 弹出遮罩层（因为下面是异步操作，界面绝对不会卡顿）
    showLoading(true);

    // 2. 准备执行程序路径与参数
    QString rootDir = "F:/items/tuopuyouhua/GraphVisualizer";
    QString program = rootDir + "/backend/build/main_full.exe";
    QString dataPath = rootDir + "/data/luoji.json";

    m_optimizerProcess->setWorkingDirectory(rootDir + "/backend/build");

    QStringList arguments;
    arguments << "-c" << dataPath << "42" << rootDir + "/data/youhua.json";

    // ==========================================================
    // 【核心修复】：全面拥抱异步！断开旧连接，使用 Lambda 监听结束信号
    // ==========================================================

    // 断开之前的绑定，防止用户多次点击按钮导致重复触发
    m_optimizerProcess->disconnect();

    // 监听程序【正常结束】信号
    connect(m_optimizerProcess, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        // 判断程序是否完美运行完毕
        if (status == QProcess::NormalExit && exitCode == 0) {
            emit requestLoadJson("youhua.json");
        } else {
            qDebug() << "❌ 优化程序执行异常，退出代码:" << exitCode;
        }
        // 核心：无论成功还是失败，程序一结束，立刻隐蔽遮罩！
        showLoading(false);
    });

    // 监听程序【启动失败】信号（比如路径写错了，根本没跑起来）
    connect(m_optimizerProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        qDebug() << "❌ 外部程序启动失败，错误类型:" << error;
        // 启动失败也要把遮罩关掉，防止死锁
        showLoading(false);
    });

    // 3. 启动后台进程 (非阻塞启动，主界面继续如丝般顺滑)
    qDebug() << "正在后台异步执行:" << program;
    m_optimizerProcess->start(program, arguments);
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
    if (!m_isDestroyed) {
        // ======================================================
        // 当前为【正常状态】 -> 触发【损毁模拟】
        // ======================================================
        if (m_canvasTitle) m_canvasTitle->setText("物理拓扑 (链路故障)");

        // 1. 读取损毁文件
        emit requestLoadJson("wuli_sunhui.json");

        // 2. 将按钮改造成“绿色恢复按钮”
        m_btnDestroy->setText("✨ 故障恢复");
        QString btnSuccessStyle =
            "QPushButton { "
            "   background-color: rgba(46, 204, 113, 0.8); " // 半透明微光绿
            "   color: #ffffff; "
            "   border: 1px solid #2ecc71; "
            "   border-radius: 6px; "
            "   padding: 12px; "
            "   font-weight: bold; "
            "   font-size: 14px; "
            "   font-family: 'Microsoft YaHei'; "
            "}"
            "QPushButton:hover { "
            "   background-color: #2ecc71; "
            "   border: 1px solid #58d68d; "
            "}"
            "QPushButton:pressed { "
            "   background-color: #27ae60; "
            "}";
        m_btnDestroy->setStyleSheet(btnSuccessStyle);

        // 3. 翻转状态记忆
        m_isDestroyed = true;

    } else {
        // ======================================================
        // 当前为【损毁状态】 -> 触发【故障恢复】
        // ======================================================
        if (m_canvasTitle) m_canvasTitle->setText("物理拓扑");

        // 1. 重新读取正常文件
        emit requestLoadJson("wuli.json");

        // 2. 将按钮变回“红色损毁按钮”
        m_btnDestroy->setText("💥 损毁模拟");
        QString btnDangerStyle =
            "QPushButton { "
            "   background-color: rgba(192, 57, 43, 0.8); " // 半透明微光红
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
        m_btnDestroy->setStyleSheet(btnDangerStyle);

        // 3. 翻转状态记忆
        m_isDestroyed = false;
    }
}