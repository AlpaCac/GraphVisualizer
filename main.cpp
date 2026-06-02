#include "mainwindow.h"
#include "test.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MainWindow w;
    w.show();

    // 实例化我们的动态测试后端
    TestWorker worker;

    // 【重要】：跨线程信号绑定，必须使用 Qt::QueuedConnection
    // 在 main.cpp 中，跟其他的 connect 写在一起：
    QObject::connect(&worker, &TestWorker::requestUpdateEdgeStyle, &w, &MainWindow::updateEdgeStyle, Qt::QueuedConnection);
    QObject::connect(&worker, &TestWorker::requestClear,   &w, &MainWindow::clearGraph, Qt::QueuedConnection);
    QObject::connect(&worker, &TestWorker::requestAddNode, &w, &MainWindow::addNode,    Qt::QueuedConnection);
    QObject::connect(&worker, &TestWorker::requestAddEdge, &w, &MainWindow::addEdge,    Qt::QueuedConnection);
    QObject::connect(&worker, &TestWorker::requestLayout,  &w, &MainWindow::applyLayout,Qt::QueuedConnection);
    QObject::connect(&worker, &TestWorker::requestUpdateNodeStyle, &w, &MainWindow::updateNodeStyle, Qt::QueuedConnection);

    // 启动后端线程，它会自动开始每隔 2.5 秒刷新一次图
    worker.start();

    return a.exec();
}