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
        "id",
        "type",
        "label",
        "requestAddEdge",
        "src",
        "dst",
        "requestLayout",
        "requestUpdateEdgeStyle",
        "QColor",
        "color",
        "thickness",
        "style",
        "requestUpdateNodeStyle",
        "shape",
        "onTick",
        "onFastTick"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'requestClear'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'requestAddNode'
        QtMocHelpers::SignalData<void(const QString &, int, const QString &)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 4 }, { QMetaType::Int, 5 }, { QMetaType::QString, 6 },
        }}),
        // Signal 'requestAddEdge'
        QtMocHelpers::SignalData<void(const QString &, const QString &, int)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 }, { QMetaType::QString, 9 }, { QMetaType::Int, 5 },
        }}),
        // Signal 'requestLayout'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'requestUpdateEdgeStyle'
        QtMocHelpers::SignalData<void(const QString &, const QString &, const QColor &, int, int)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 }, { QMetaType::QString, 9 }, { 0x80000000 | 12, 13 }, { QMetaType::Int, 14 },
            { QMetaType::Int, 15 },
        }}),
        // Signal 'requestUpdateNodeStyle'
        QtMocHelpers::SignalData<void(const QString &, const QColor &, int)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 4 }, { 0x80000000 | 12, 13 }, { QMetaType::Int, 17 },
        }}),
        // Slot 'onTick'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onFastTick'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<TestWorker, qt_meta_tag_ZN10TestWorkerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject TestWorker::staticMetaObject = { {
    QMetaObject::SuperData::link<QThread::staticMetaObject>(),
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
        case 1: _t->requestAddNode((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 2: _t->requestAddEdge((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        case 3: _t->requestLayout(); break;
        case 4: _t->requestUpdateEdgeStyle((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QColor>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[5]))); break;
        case 5: _t->requestUpdateNodeStyle((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QColor>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        case 6: _t->onTick(); break;
        case 7: _t->onFastTick(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)()>(_a, &TestWorker::requestClear, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)(const QString & , int , const QString & )>(_a, &TestWorker::requestAddNode, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)(const QString & , const QString & , int )>(_a, &TestWorker::requestAddEdge, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)()>(_a, &TestWorker::requestLayout, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)(const QString & , const QString & , const QColor & , int , int )>(_a, &TestWorker::requestUpdateEdgeStyle, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (TestWorker::*)(const QString & , const QColor & , int )>(_a, &TestWorker::requestUpdateNodeStyle, 5))
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
    return QThread::qt_metacast(_clname);
}

int TestWorker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QThread::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void TestWorker::requestClear()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void TestWorker::requestAddNode(const QString & _t1, int _t2, const QString & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2, _t3);
}

// SIGNAL 2
void TestWorker::requestAddEdge(const QString & _t1, const QString & _t2, int _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3);
}

// SIGNAL 3
void TestWorker::requestLayout()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void TestWorker::requestUpdateEdgeStyle(const QString & _t1, const QString & _t2, const QColor & _t3, int _t4, int _t5)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2, _t3, _t4, _t5);
}

// SIGNAL 5
void TestWorker::requestUpdateNodeStyle(const QString & _t1, const QColor & _t2, int _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
