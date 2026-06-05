#include "test.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QMap>
#include <QDir>

TestWorker::TestWorker(QObject *parent) : QObject(parent) {}

void TestWorker::loadGraphFromJson() {
    emit requestClear();
    emit requestClearTasks();

    // ==========================================
    // 智能寻址：适配不同的运行目录环境
    // ==========================================
    QString filePath = "./data/in.json"; // 假设打包发布后，data 和 exe 在同级目录

    if (!QFile::exists(filePath)) {
        // 适配 Qt Creator 默认的 build/build-xxx-Debug/ 结构 (回退两层)
        filePath = "../../data/in.json";
    }
    if (!QFile::exists(filePath)) {
        // 备用：适配普通的单层 build/ 结构 (回退一层)
        filePath = "../data/in.json";
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

        // 【修改 3】：把发射信号的第一个参数从 name 换成 strId
        // 这样前端画布上渲染的文字就会是这个数字 ID
        emit requestAddNode(strId, 0, label, shape, color, size);
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
    // 2. 解析 Flows (业务流数组) 兼 “五维硬核判定”
    // ==============================================================
    QJsonArray flowsArr = root["flows"].toArray();
    for (int i = 0; i < flowsArr.size(); ++i) {
        QJsonObject flowObj = flowsArr[i].toObject();
        QString taskId = QString::number(flowObj["id"].toInt());
        QString taskName = flowObj["name"].toString();

        // --- 提取业务流对 5 大指标的独立要求 (Requirement) ---
        // (如果某个业务流没写某项要求，则赋予一个绝对能达标的默认值)

        // 越小越好指标 (时延、成本) -> 默认给极大值 (999999)
        double req_lat  = extractNum(flowObj, "comp_lat", 999999.0);
        double req_cost = extractNum(flowObj, "cost", 99999999.0);

        // 越大越好指标 (连通度、吞吐量、可靠性) -> 默认给极小值 (0.0)
        double req_conn = extractNum(flowObj, "C_conn_norm", 0.0);
        double req_thr  = extractNum(flowObj, "E_throughput", 0.0);
        double req_rel  = extractNum(flowObj, "comp_rel", 0.0);

        // --- 智能量纲对齐 (防呆设计) ---
        // 防止出现 Actual 是 63.95(%)，而 Flow 要求是 0.6 的对比乌龙
        auto alignPercent = [](double& act, double& req) {
            if (act > 1.0 && req > 0.0 && req <= 1.0) req *= 100.0;
            else if (req > 1.0 && act > 0.0 && act <= 1.0) act *= 100.0;
        };
        double cmp_act_conn = act_conn, cmp_req_conn = req_conn;
        double cmp_act_rel = act_rel, cmp_req_rel = req_rel;
        alignPercent(cmp_act_conn, cmp_req_conn);
        alignPercent(cmp_act_rel, cmp_req_rel);

        // --- 核心校验引擎：五项指标一票否决 ---
        bool isCompliant = true;

        // 1. 时延：必须 小于等于 要求
        if (act_lat > req_lat) isCompliant = false;

        // 2. 成本：必须 小于等于 要求
        if (act_cost > req_cost) isCompliant = false;

        // 3. 连通度：必须 大于等于 要求
        if (cmp_act_conn < cmp_req_conn) isCompliant = false;

        // 4. 吞吐量：必须 大于等于 要求
        if (act_thr < req_thr) isCompliant = false;

        // 5. 可靠性：必须 大于等于 要求
        if (cmp_act_rel < cmp_req_rel) isCompliant = false;

        // ... (前面的提取要求和 isCompliant 判定的代码保持不变) ...

        // ==========================================================
        // 【新增】：智能提取业务流的起点和终点
        // 尝试多种常见的 JSON 字段命名习惯
        // ==========================================================
        int srcNode = flowObj.contains("source") ? flowObj["source"].toInt() :
                          (flowObj.contains("src") ? flowObj["src"].toInt() : flowObj["node_a"].toInt());

        int dstNode = flowObj.contains("target") ? flowObj["target"].toInt() :
                          (flowObj.contains("dst") ? flowObj["dst"].toInt() : flowObj["node_b"].toInt());

        QString srcId = QString::number(srcNode);
        QString dstId = QString::number(dstNode);

        // 【修改】：发射带有起点和终点的完整信号
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