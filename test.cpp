#include "test.h"
#include <QRandomGenerator>
#include <QMetaObject>
#include <algorithm>
#include <QColor>

TestWorker::TestWorker(QObject *parent) : QThread(parent), m_step(0) {}

void TestWorker::run() {
    generateInitialGraph();

    // 依然保持 10 秒算一次全图布局
    QTimer layoutTimer;
    connect(&layoutTimer, &QTimer::timeout, this, &TestWorker::onTick);
    layoutTimer.start(10000);

    // 【新增】：创建一个 0.5秒 执行一次的极速特效定时器
    QTimer styleTimer;
    connect(&styleTimer, &QTimer::timeout, this, &TestWorker::onFastTick);
    styleTimer.start(500);

    QMetaObject::invokeMethod(this, "onTick", Qt::QueuedConnection);
    exec();
}

// 【新增】：极速修改边样式接口演示
void TestWorker::onFastTick() {
    // 50% 概率改连线，50% 概率改节点
    int target = QRandomGenerator::global()->bounded(2);

    if (target == 0 && !m_edges.isEmpty()) {
        // ======== 改变边的样式 ========
        int idx = QRandomGenerator::global()->bounded(m_edges.size());
        EdgeDef e = m_edges[idx];
        int randStyle = QRandomGenerator::global()->bounded(3);
        if (randStyle == 0) {
            emit requestUpdateEdgeStyle(e.src, e.dst, QColor(Qt::red), 4, 2);
        } else if (randStyle == 1) {
            emit requestUpdateEdgeStyle(e.src, e.dst, QColor(0, 200, 0), 5, 1);
        } else {
            emit requestUpdateEdgeStyle(e.src, e.dst, QColor(Qt::darkMagenta), 3, 3);
        }
    }
    else if (target == 1 && !m_stdNodes.isEmpty()) {
        // ======== 改变节点的样式 ========
        int idx = QRandomGenerator::global()->bounded(m_stdNodes.size());
        QString n = m_stdNodes[idx];

        // 随机 0 到 3 产生四种不同的形状
        int randShape = QRandomGenerator::global()->bounded(4);

        if (randShape == 0) {
            // 0: 圆形 + 粉色
            emit requestUpdateNodeStyle(n, QColor(255, 182, 193), 0);
        }
        else if (randShape == 1) {
            // 1: 正方形 + 浅蓝色
            emit requestUpdateNodeStyle(n, QColor(173, 216, 230), 1);
        }
        else if (randShape == 2) {
            // 2: 菱形 + 鲜红色 (危险警报)
            emit requestUpdateNodeStyle(n, QColor(255, 100, 100), 2);
        }
        else {
            // 3: 六边形 + 浅绿色 (安全节点)
            emit requestUpdateNodeStyle(n, QColor(150, 255, 150), 3);
        }
    }
}
void TestWorker::generateInitialGraph() {
    for(int i = 1; i <= 4; i++) m_highNodes.append(QString("H%1").arg(i));
    for(int i = 1; i <= 16; i++) m_stdNodes.append(QString("S%1").arg(i));

    // 建立初始骨架连通
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            m_edges.append({m_highNodes[i], m_stdNodes[i * 4 + j]});
        }
    }
    // 横向串联标准节点，确保彻底打通
    for(int i = 0; i < 15; i++) {
        m_edges.append({m_stdNodes[i], m_stdNodes[i + 1]});
    }
}

void TestWorker::onTick() {
    // 每次微调幅度极小
    if (m_step > 0) {
        int action = QRandomGenerator::global()->bounded(3);

        if (action == 0 && !m_dynamicNodes.isEmpty()) {
            // 【大幅度优化】：只允许删除外围动态节点，核心骨架不动 -> 保证整张图绝对连通
            QString removed = m_dynamicNodes.takeLast();
            m_edges.erase(std::remove_if(m_edges.begin(), m_edges.end(), [&](const EdgeDef& e){
                              return e.src == removed || e.dst == removed;
                          }), m_edges.end());

        } else if (action == 1) {
            // 添加 1 个新节点
            QString newNode = QString("D_New_%1").arg(m_step);
            m_dynamicNodes.append(newNode);
            // 【安全机制】：强制连回一个随机的标准骨架节点 -> 保证新节点绝不孤立，维持全图连通
            QString randS = m_stdNodes[QRandomGenerator::global()->bounded(m_stdNodes.size())];
            m_edges.append({randS, newNode});

        } else {
            // 仅仅是随机在现有的节点之间建立一条新连线（不改变节点，完全安全）
            QString s1 = m_stdNodes[QRandomGenerator::global()->bounded(m_stdNodes.size())];
            QString s2 = m_stdNodes[QRandomGenerator::global()->bounded(m_stdNodes.size())];
            if (s1 != s2) m_edges.append({s1, s2});
        }
    }

    emit requestClear();

    // 发送全量拓扑
    for(const auto& n : m_highNodes)    emit requestAddNode(n, 0, n + "\n(High)");
    for(const auto& n : m_stdNodes)     emit requestAddNode(n, 1, n);
    for(const auto& n : m_dynamicNodes) emit requestAddNode(n, 1, n + "\n(Dyn)"); // 动态节点显示为标准颜色
    for(const auto& e : m_edges)        emit requestAddEdge(e.src, e.dst, 0);

    emit requestLayout();
    m_step++;
}