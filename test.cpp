#include "test.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QMap>
#include <QDir>

TestWorker::TestWorker(QObject *parent) : QObject(parent) {}

void TestWorker::loadGraphFromJson(const QString& fileName) { // 【修改】增加参数
    emit requestClear();
    emit requestClearTasks();

    // ==========================================
    // 智能寻址：动态拼接传进来的文件名
    // ==========================================
    QString filePath = "./data/" + fileName;

    if (!QFile::exists(filePath)) {
        filePath = "../../data/" + fileName;
    }
    if (!QFile::exists(filePath)) {
        filePath = "../data/" + fileName;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "❌ 无法打开 JSON 文件！当前工作目录:" << QDir::currentPath();
        qDebug() << "❌ 最终尝试读取的路径为:" << filePath;
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    // ... 下方保留原有的 JSON 解析代码 (QJsonDocument::fromJson...) ...

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qDebug() << "❌ JSON 格式错误";
        return;
    }

    QJsonObject root = doc.object();
    QMap<int, QString> idToNameMap; // 用于存储全局的 ID -> Name 映射，方便连线

    // ==============================================================
    // 1. 解析 Nodes (节点数组)
    // ==============================================================
    QJsonArray nodesArr = root["nodes"].toArray();
    for (int i = 0; i < nodesArr.size(); ++i) {
        QJsonObject nodeObj = nodesArr[i].toObject();
        int id = nodeObj["id"].toInt();
        QString name = nodeObj["device_name"].toString();

        // 【修改 1】：把真正的数字 id 转化为字符串，作为该节点的核心标识
        QString strId = QString::number(id);
        idToNameMap[id] = strId; // 【修改 2】：映射表里存数字 ID

        // 提取物理属性
        double cpu = nodeObj["cpu_capacity"].toDouble();
        double mem = nodeObj["memory_mb"].toDouble();
        int maxPorts = nodeObj["max_ports"].toInt(); // 【新增】：提取最大接口数
        double rel = nodeObj["reliability"].toDouble();
        QString staticType = nodeObj["static_type"].toString();

        // 【修改】：严格按照需求拼装 6 大节点状态属性
        // 使用 " | " 作为分隔符，前端侦测面板会自动将它转换为整齐的换行排版
        QString label = QString("节点 ID: %1 | 设备名称: %2 | CPU 容量: %3 | 内存容量: %4 MB | 最大接口: %5 | 可靠性: %6")
                            .arg(strId)
                            .arg(name)
                            .arg(cpu)
                            .arg(mem)
                            .arg(maxPorts)
                            .arg(rel);

        QColor color = QColor("#3498db");
        int shape = 0;
        int size = 25;

        if (staticType == "Master" || name.contains("Master")) {
            color = QColor("#f1c40f");
            shape = 3;
            size = 40;
        } else if (name.contains("Node")) {
            color = QColor("#2ecc71");
            shape = 0;
            size = 25;
        }

        // 【新增】：提取 x, y 坐标，默认为 0
        double x = nodeObj.contains("x") ? nodeObj["x"].toDouble() : 0.0;
        double y = nodeObj.contains("y") ? nodeObj["y"].toDouble() : 0.0;

        // 【修改 3】：把发射信号的第一个参数从 name 换成 strId
        // 这样前端画布上渲染的文字就会是这个数字 ID
        emit requestAddNode(strId, 0, label, shape, color, size, x, y);
    }

    // ==============================================================
    // 2. 解析 Links (边数组)
    // ==============================================================
    QJsonArray linksArr = root["links"].toArray();
    for (int i = 0; i < linksArr.size(); ++i) {
        QJsonObject linkObj = linksArr[i].toObject();

        int id = linkObj["id"].toInt();
        int nodeA = linkObj["node_a"].toInt();
        int nodeB = linkObj["node_b"].toInt();

        double bw = linkObj["bandwidth"].toDouble();
        double delay = linkObj["propagation_delay"].toDouble();
        double rel = linkObj["reliability"].toDouble();
        double cost = linkObj["cost"].toDouble();
        QString typeStr = linkObj["type"].toString();

        QString label = QString("链路 ID: %1 | 源节点: %2 | 目标节点: %3 | 链路带宽: %4 Mbps | 传输时延: %5 ms | 链路可靠性: %6 | 链路成本: ¥%7 | 链路类型: %8")
                            .arg(id).arg(nodeA).arg(nodeB).arg(bw, 0, 'f', 1)
                            .arg(delay, 0, 'f', 2).arg(rel, 0, 'f', 4)
                            .arg(cost, 0, 'f', 1).arg(typeStr);

        // 只有当起点和终点都在节点字典里时，才发射连线信号
        if (idToNameMap.contains(nodeA) && idToNameMap.contains(nodeB)) {
            emit requestAddEdge(idToNameMap[nodeA], idToNameMap[nodeB], 0, label);
        }
    }
    // ==============================================================
    // 【终极数据榨汁机】：兼容纯数字、带单位字符串(Gbps/%)的超强提取器
    // ==============================================================
    auto extractNum = [](const QJsonObject& obj, const QString& key, double defaultValue) -> double {
        if (!obj.contains(key)) return defaultValue;

        QJsonValue val = obj.value(key);
        if (val.isDouble()) {
            return val.toDouble(); // 如果是纯数字类型，直接返回
        } else if (val.isString()) {
            QString str = val.toString();
            QString numStr;
            // 剥离所有的字母、空格、%，只保留数字和负号小数点
            for (QChar c : str) {
                if (c.isDigit() || c == '.' || c == '-') numStr.append(c);
            }
            if (!numStr.isEmpty()) return numStr.toDouble();
        }
        return defaultValue;
    };

    // ==============================================================
    // 1. 提取实际大盘指标 (Actual)
    // ==============================================================
    QJsonObject assessObj = root["assess_data"].toObject();

    // 如果取不到，默认给 0.0，防止程序崩溃
    double act_lat  = extractNum(assessObj, "comp_lat", 0.0);      // 例如: 0.31
    double act_conn = extractNum(assessObj, "C_conn_norm", 0.0);   // 例如: 63.95
    double act_thr  = extractNum(assessObj, "E_throughput", 0.0);  // 例如: 111.17
    double act_rel  = extractNum(assessObj, "comp_rel", 0.0);      // 例如: 92.43
    double act_cost = extractNum(assessObj, "cost", 0.0);          // 例如: 14441

    // ==============================================================
    // 2. 解析 Flows (业务流调度队列)
    // ==============================================================
    QJsonArray flowsArr = root["flows"].toArray();
    for (int i = 0; i < flowsArr.size(); ++i) {
        QJsonObject flowObj = flowsArr[i].toObject();

        QString taskId = QString::number(flowObj["id"].toInt());
        QString taskName = flowObj["name"].toString();

        // ==========================================================
        // 【核心修改】：不再进行复杂的五维对比，直接读取 pass 字段
        // pass: 1 表示达标，0 表示未达标
        // ==========================================================
        bool isCompliant = false;
        if (flowObj.contains("pass")) {
            isCompliant = (flowObj["pass"].toInt() == 1);
        }

        // ==========================================================
        // 智能提取业务流的起点和终点
        // ==========================================================
        int srcNode = flowObj.contains("source") ? flowObj["source"].toInt() :
                          (flowObj.contains("src") ? flowObj["src"].toInt() : flowObj["node_a"].toInt());

        int dstNode = flowObj.contains("target") ? flowObj["target"].toInt() :
                          (flowObj.contains("dst") ? flowObj["dst"].toInt() : flowObj["node_b"].toInt());

        QString srcId = QString::number(srcNode);
        QString dstId = QString::number(dstNode);

        // 发射带有起点和终点的完整信号，交由前端渲染徽章和绿色虚线
        emit requestAddTask(taskId, taskName, isCompliant, srcId, dstId);
    }

    // ==============================================================
    // 4. 解析 assess_data (性能与评估指标)
    // ==============================================================
    if (root.contains("assess_data")) {
        QJsonObject assessObj = root["assess_data"].toObject();

        // 前五个为实时指标 (时延、连通度、可靠性、吞吐量、组网成本)
        QString latency = assessObj["comp_lat"].toString() + " ms";
        QString connectivity = assessObj["C_conn_norm"].toString();

        // 【修正】：严格按字段字面含义映射
        QString reliability = assessObj["comp_rel"].toString();
        QString throughput = assessObj["E_throughput"].toString();

        QString cost = "¥ " + assessObj["cost"].toString();

        emit requestUpdateMetrics(latency, connectivity, reliability, throughput, cost);

        // 后两个为算法模型对比跑分
        QString scoreBaseline = assessObj["data1"].toString();
        QString scoreAdvanced = assessObj["data2"].toString();

        emit requestUpdateEvaluation(scoreBaseline, scoreAdvanced);
    }

    // ==============================================================
    // 5. 数据灌装完毕，触发引擎计算物理碰撞和排版
    // ==============================================================
    emit requestLayout();
}