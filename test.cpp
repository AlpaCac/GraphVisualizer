#include "test.h"
#include <QRandomGenerator>
#include <QMetaObject>

TestWorker::TestWorker(QObject *parent) : QThread(parent), m_step(0) {}

void TestWorker::run() {
    generateGraph();

    // 仅保留 10 秒刷新一次的静态主循环
    QTimer layoutTimer;
    connect(&layoutTimer, &QTimer::timeout, this, &TestWorker::onTick);
    layoutTimer.start(10000);

    QMetaObject::invokeMethod(this, "onTick", Qt::QueuedConnection);
    exec();
}

void TestWorker::generateGraph() {
    m_mainNodes.clear();
    m_backupNodes.clear();
    m_leafNodes.clear();
    m_edges.clear();

    m_controlNode = "Control_Center";

    for(int i = 0; i < 10; i++) {
        m_mainNodes.append(QString("Main_%1").arg(i));
        m_backupNodes.append(QString("Backup_%1").arg(i));
    }

    for(int i = 0; i < 10; i++) {
        // 1. 主干双环网：主节点连成外环，备节点连成内环
        m_edges.append({m_mainNodes[i], m_mainNodes[(i + 1) % 10]});
        m_edges.append({m_backupNodes[i], m_backupNodes[(i + 1) % 10]});

        // 2. 主备桥接：主节点和对应的备节点直接相连
        m_edges.append({m_mainNodes[i], m_backupNodes[i]});

        // 3. 终端双线并发 (Dual-homing) + 边缘菊花链 (Daisy Chain)
        int leafGroupSize = 8;
        QList<QString> currentLeafGroup; // 用于记录当前组的叶子，方便后续串联

        for(int j = 0; j < leafGroupSize; j++) {
            QString leaf = QString("Leaf_%1_%2").arg(i).arg(j);
            m_leafNodes.append(leaf);
            currentLeafGroup.append(leaf); // 存入临时列表

            // 连主、连备
            m_edges.append({m_mainNodes[i], leaf});
            m_edges.append({m_backupNodes[i], leaf});
        }

        // 【核心新增】：用一条线把这 8 个叶子节点依次串联起来
        for(int j = 0; j < leafGroupSize - 1; j++) {
            m_edges.append({currentLeafGroup[j], currentLeafGroup[j+1]});
        }
    }

    // 控制中心连在 Main_0 上
    m_edges.append({m_mainNodes[0], m_controlNode});
}
void TestWorker::onTick() {
    emit requestClear();

    emit requestAddNode(m_controlNode, 0, "控制中心\n(Master)");
    // 【巨型尺寸 100】：让控制中心显得不可撼动
    emit requestUpdateNodeStyle(m_controlNode, QColor(255, 215, 0), 3, 100);

    for(const auto& n : m_mainNodes) {
        emit requestAddNode(n, 1, n);
        // 【标准尺寸 80】：主环核心保持原样
        emit requestUpdateNodeStyle(n, QColor(255, 100, 100), 2, 80);
    }

    for(const auto& n : m_backupNodes) {
        emit requestAddNode(n, 1, n);
        // 【中等尺寸 60】：备用节点略小于主节点
        emit requestUpdateNodeStyle(n, QColor(173, 216, 230), 1, 60);
    }

    for(const auto& n : m_leafNodes) {
        emit requestAddNode(n, 1, "");
        // 【玲珑尺寸 35】：叶子节点大幅缩小，组成精致的小圆环
        emit requestUpdateNodeStyle(n, QColor(150, 255, 150), 0, 35);
    }

    for(const auto& e : m_edges) {
        emit requestAddEdge(e.src, e.dst, 0);
    }

    emit requestLayout();
    m_step++;
}