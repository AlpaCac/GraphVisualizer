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

    // 替换为这行代码：
    worker.generateRandomGraph();

    return a.exec();
}