/****************************************************************************
** Meta object code from reading C++ file 'ImageView.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/ImageView.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ImageView.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.0. It"
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
struct qt_meta_tag_ZN9ImageViewE_t {};
} // unnamed namespace

template <> constexpr inline auto ImageView::qt_create_metaobjectdata<qt_meta_tag_ZN9ImageViewE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ImageView",
        "hoverPixel",
        "",
        "x",
        "y",
        "QRgb",
        "rgba",
        "clickPixel",
        "hoverOutside"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'hoverPixel'
        QtMocHelpers::SignalData<void(int, int, QRgb)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::Int, 4 }, { 0x80000000 | 5, 6 },
        }}),
        // Signal 'clickPixel'
        QtMocHelpers::SignalData<void(int, int, QRgb)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::Int, 4 }, { 0x80000000 | 5, 6 },
        }}),
        // Signal 'hoverOutside'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ImageView, qt_meta_tag_ZN9ImageViewE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ImageView::staticMetaObject = { {
    QMetaObject::SuperData::link<QGraphicsView::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9ImageViewE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9ImageViewE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN9ImageViewE_t>.metaTypes,
    nullptr
} };

void ImageView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ImageView *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->hoverPixel((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QRgb>>(_a[3]))); break;
        case 1: _t->clickPixel((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QRgb>>(_a[3]))); break;
        case 2: _t->hoverOutside(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ImageView::*)(int , int , QRgb )>(_a, &ImageView::hoverPixel, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ImageView::*)(int , int , QRgb )>(_a, &ImageView::clickPixel, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ImageView::*)()>(_a, &ImageView::hoverOutside, 2))
            return;
    }
}

const QMetaObject *ImageView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ImageView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9ImageViewE_t>.strings))
        return static_cast<void*>(this);
    return QGraphicsView::qt_metacast(_clname);
}

int ImageView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QGraphicsView::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void ImageView::hoverPixel(int _t1, int _t2, QRgb _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3);
}

// SIGNAL 1
void ImageView::clickPixel(int _t1, int _t2, QRgb _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2, _t3);
}

// SIGNAL 2
void ImageView::hoverOutside()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
