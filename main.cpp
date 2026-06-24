#include "mainwindow.h"
#include "test.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MainWindow w;
    w.show();

    TestWorker worker;

    QObject::connect(&worker, &TestWorker::requestClear,   &w, &MainWindow::clearGraph, Qt::QueuedConnection);
    QObject::connect(&worker, &TestWorker::requestAddNode, &w, &MainWindow::addNode,    Qt::QueuedConnection);
    QObject::connect(&worker, &TestWorker::requestAddEdge, &w, &MainWindow::addEdge,    Qt::QueuedConnection);
    QObject::connect(&worker, &TestWorker::requestLayout,  &w, &MainWindow::applyLayout,Qt::QueuedConnection);
    QObject::connect(&worker, &TestWorker::requestUpdateNodeStyle, &w, &MainWindow::updateNodeStyle, Qt::QueuedConnection);

    QObject::connect(&worker, &TestWorker::requestAddTask, &w, &MainWindow::addTask, Qt::QueuedConnection);
    QObject::connect(&worker, &TestWorker::requestRemoveTask, &w, &MainWindow::removeTask, Qt::QueuedConnection);
    QObject::connect(&worker, &TestWorker::requestClearTasks, &w, &MainWindow::clearTasks, Qt::QueuedConnection);


    // 【新增】：将后端解析出的评估指标信号与 MainWindow 连接
    QObject::connect(&worker, &TestWorker::requestUpdateMetrics, &w, &MainWindow::updateMetrics, Qt::QueuedConnection);
    QObject::connect(&worker, &TestWorker::requestUpdateEvaluation, &w, &MainWindow::updateEvaluation, Qt::QueuedConnection);

    // ==========================================================
    // 【新增】：将前端界面的“加载 JSON 请求”与后端解析器连接起来
    // ==========================================================
    QObject::connect(&w, &MainWindow::requestLoadJson, &worker, &TestWorker::loadGraphFromJson, Qt::QueuedConnection);

    // 【修改】：程序刚启动时，默认读取场景 1 的 wuli.json
    worker.loadGraphFromJson("1/wuli.json");

    return a.exec();
}