/****************************************************************************
** Meta object code from reading C++ file 'test.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../test.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'test.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10TestWorkerE_t {};
} // unnamed namespace

template <> constexpr inline auto TestWorker::qt_create_metaobjectdata<qt_meta_tag_ZN10TestWorkerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "TestWorker",
        "requestClear",
        "",
        "requestAddNode",
        "nodeId",
        "type",
        "label",
        "shape",
        "QColor",
        "color",
        "size",
        "x",
        "y",
        "requestAddEdge",
        "sourceId",
        "destId",
        "requestLayout",
        "requestUpdateNodeStyle",
        "requestUpdateEdgeStyle",
        "thickness",
        "style",
        "requestAddTask",
        "taskId",
        "taskName",
        "isCompliant",
        "srcId",
        "dstId",
        "routingPath",
        "requestRemoveTask",
        "requestClearTasks",
        "requestUpdateMetrics",
        "latency",
        "connectivity",
        "reliability",
        "throughput",
        "cost",
        "requestUpdateEvaluation",
        "scoreBaseline",
        "scoreAdvanced",
        "loadGraphFromJson",
        "fileName"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'requestClear'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'requestAddNode'
        QtMocHelpers::SignalData<void(const QString &, int, const QString &, int, const QColor &, int, double, double)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 4 }, { QMetaType::Int, 5 }, { QMetaType::QString, 6 }, { QMetaType::Int, 7 },
            { 0x80000000 | 8, 9 }, { QMetaType::Int, 10 }, { QMetaType::Double, 11 }, { QMetaType::Double, 12 },
        }}),
        // Signal 'requestAddEdge'
        QtMocHelpers::SignalData<void(const QString &, const QString &, int, const QString &)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 14 }, { QMetaType::QString, 15 }, { QMetaType::Int, 5 }, { QMetaType::QString, 6 },
        }}),
        // Signal 'requestAddEdge'
        QtMocHelpers::SignalData<void(const QString &, const QString &, int)>(13, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::QString, 14 }, { QMetaType::QString, 15 }, { QMetaType::Int, 5 },
        }}),
        // Signal 'requestLayout'
        QtMocHelpers::SignalData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'requestUpdateNodeStyle'
        QtMocHelpers::SignalData<void(const QString &, const QColor &, int, int)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 4 }, { 0x80000000 | 8, 9 }, { QMetaType::Int, 7 }, { QMetaType::Int, 10 },
        }}),
        // Signal 'requestUpdateEdgeStyle'
        QtMocHelpers::SignalData<void(const QString &, const QString &, const QColor &, int, int)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 14 }, { QMetaType::QString, 15 }, { 0x80000000 | 8, 9 }, { QMetaType::Int, 19 },
            { QMetaType::Int, 20 },
        }}),
        // Signal 'requestAddTask'
        QtMocHelpers::SignalData<void(const QString &, const QString &, bool, const QString &, const QString &, const QStringList &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 22 }, { QMetaType::QString, 23 }, { QMetaType::Bool, 24 }, { QMetaType::QString, 25 },
            { QMetaType::QString, 26 }, { QMetaType::QStringList, 27 },
        }}),
        // Signal 'requestRemoveTask'
        QtMocHelpers::SignalData<void(const QString &)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 22 },
        }}),
        // Signal 'requestClearTasks'
        QtMocHelpers::SignalData<void()>(29, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'requestUpdateMetrics'
        QtMocHelpers::SignalData<void(const QString &, const QString &, const QString &, const QString &, const QString &)>(30, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 31 }, { QMetaType::QString, 32 }, { QMetaType::QString, 33 }, { QMetaType::QString, 34 },
            { QMetaType::QString, 35 },
        }}),
        // Signal 'requestUpdateEvaluation'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(36, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 37 }, { QMetaType::QString, 38 },
        }}),
        // Slot 'loadGraphFromJson'
        QtMocHelpers::SlotData<void(const QString &)>(39, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 40 },
        }}),
        // Slot 'loadGraphFromJson'
        QtMocHelpers::SlotData<void()>(39, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<TestWorker, qt_meta_tag_ZN10TestWorkerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject TestWorker::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10TestWorkerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10TestWorkerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10TestWorkerE_t>.metaTypes,
    nullptr
} };

void TestWorker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TestWorker *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->requestClear(); break;
        case 1: _t->requestAddNode((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<QColor>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[6])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[7])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[8]))); break;
        case 2: _t->requestAddEdge((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4]))); break;
        case 3: _t->requestAddEdge((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        case 4: _t->requestLayout(); break;
        case 5: _t->requestUpdateNodeStyle((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QColor>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[4]))); break;
        case 6: _t->requestUpdateEdgeStyle((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QColor>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[5]))); break;
        case 7: _t->requestAddTask((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[6]))); break;
        case 8: _t->requestRemoveTask((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->requestClearTasks(); break;
        case 10: _t->requestUpdateMetrics((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[5]))); break;
        case 11: _t->requestUpdateEvaluation((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 12: _t->loadGraphFromJson((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 13: _t->loadGraphFromJson(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)()>(_a, &TestWorker::requestClear, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)(const QString & , int , const QString & , int , const QColor & , int , double , double )>(_a, &TestWorker::requestAddNode, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)(const QString & , const QString & , int , const QString & )>(_a, &TestWorker::requestAddEdge, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)()>(_a, &TestWorker::requestLayout, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)(const QString & , const QColor & , int , int )>(_a, &TestWorker::requestUpdateNodeStyle, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)(const QString & , const QString & , const QColor & , int , int )>(_a, &TestWorker::requestUpdateEdgeStyle, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)(const QString & , const QString & , bool , const QString & , const QString & , const QStringList & )>(_a, &TestWorker::requestAddTask, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)(const QString & )>(_a, &TestWorker::requestRemoveTask, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)()>(_a, &TestWorker::requestClearTasks, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)(const QString & , const QString & , const QString & , const QString & , const QString & )>(_a, &TestWorker::requestUpdateMetrics, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)(const QString & , const QString & )>(_a, &TestWorker::requestUpdateEvaluation, 11))
            return;
    }
}

const QMetaObject *TestWorker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TestWorker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10TestWorkerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int TestWorker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 14)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 14;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 14)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 14;
    }
    return _id;
}

// SIGNAL 0
void TestWorker::requestClear()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void TestWorker::requestAddNode(const QString & _t1, int _t2, const QString & _t3, int _t4, const QColor & _t5, int _t6, double _t7, double _t8)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2, _t3, _t4, _t5, _t6, _t7, _t8);
}

// SIGNAL 2
void TestWorker::requestAddEdge(const QString & _t1, const QString & _t2, int _t3, const QString & _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 4
void TestWorker::requestLayout()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void TestWorker::requestUpdateNodeStyle(const QString & _t1, const QColor & _t2, int _t3, int _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 6
void TestWorker::requestUpdateEdgeStyle(const QString & _t1, const QString & _t2, const QColor & _t3, int _t4, int _t5)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2, _t3, _t4, _t5);
}

// SIGNAL 7
void TestWorker::requestAddTask(const QString & _t1, const QString & _t2, bool _t3, const QString & _t4, const QString & _t5, const QStringList & _t6)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2, _t3, _t4, _t5, _t6);
}

// SIGNAL 8
void TestWorker::requestRemoveTask(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void TestWorker::requestClearTasks()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void TestWorker::requestUpdateMetrics(const QString & _t1, const QString & _t2, const QString & _t3, const QString & _t4, const QString & _t5)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1, _t2, _t3, _t4, _t5);
}

// SIGNAL 11
void TestWorker::requestUpdateEvaluation(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 11, nullptr, _t1, _t2);
}
QT_WARNING_POP
