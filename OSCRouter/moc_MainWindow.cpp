/****************************************************************************
** Meta object code from reading C++ file 'MainWindow.h'
**
** Created: Wed Jul 29 20:57:00 2015
**      by: The Qt Meta Object Compiler version 63 (Qt 4.8.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "MainWindow.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainWindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 63
#error "This file was generated using the moc from 4.8.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_TableScrollArea[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: signature, parameters, type, tag, flags
      21,   17,   16,   16, 0x05,

       0        // eod
};

static const char qt_meta_stringdata_TableScrollArea[] = {
    "TableScrollArea\0\0w,h\0resized(int,int)\0"
};

void TableScrollArea::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        TableScrollArea *_t = static_cast<TableScrollArea *>(_o);
        switch (_id) {
        case 0: _t->resized((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData TableScrollArea::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject TableScrollArea::staticMetaObject = {
    { &QScrollArea::staticMetaObject, qt_meta_stringdata_TableScrollArea,
      qt_meta_data_TableScrollArea, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &TableScrollArea::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *TableScrollArea::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *TableScrollArea::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_TableScrollArea))
        return static_cast<void*>(const_cast< TableScrollArea*>(this));
    return QScrollArea::qt_metacast(_clname);
}

int TableScrollArea::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QScrollArea::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    return _id;
}

// SIGNAL 0
void TableScrollArea::resized(int _t1, int _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
static const uint qt_meta_data_Indicator[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: signature, parameters, type, tag, flags
      11,   10,   10,   10, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_Indicator[] = {
    "Indicator\0\0onUpdate()\0"
};

void Indicator::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        Indicator *_t = static_cast<Indicator *>(_o);
        switch (_id) {
        case 0: _t->onUpdate(); break;
        default: ;
        }
    }
    Q_UNUSED(_a);
}

const QMetaObjectExtraData Indicator::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject Indicator::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_Indicator,
      qt_meta_data_Indicator, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &Indicator::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *Indicator::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *Indicator::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_Indicator))
        return static_cast<void*>(const_cast< Indicator*>(this));
    return QWidget::qt_metacast(_clname);
}

int Indicator::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    return _id;
}
static const uint qt_meta_data_TcpTableRow[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: signature, parameters, type, tag, flags
      16,   13,   12,   12, 0x05,

 // slots: signature, parameters, type, tag, flags
      49,   41,   12,   12, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_TcpTableRow[] = {
    "TcpTableRow\0\0id\0addRemoveClicked(size_t)\0"
    "checked\0onAddRemoveClicked(bool)\0"
};

void TcpTableRow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        TcpTableRow *_t = static_cast<TcpTableRow *>(_o);
        switch (_id) {
        case 0: _t->addRemoveClicked((*reinterpret_cast< size_t(*)>(_a[1]))); break;
        case 1: _t->onAddRemoveClicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData TcpTableRow::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject TcpTableRow::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_TcpTableRow,
      qt_meta_data_TcpTableRow, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &TcpTableRow::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *TcpTableRow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *TcpTableRow::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_TcpTableRow))
        return static_cast<void*>(const_cast< TcpTableRow*>(this));
    return QWidget::qt_metacast(_clname);
}

int TcpTableRow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void TcpTableRow::addRemoveClicked(size_t _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
static const uint qt_meta_data_TcpTable[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: signature, parameters, type, tag, flags
      13,   10,    9,    9, 0x0a,
      34,   31,    9,    9, 0x0a,

       0        // eod
};

static const char qt_meta_stringdata_TcpTable[] = {
    "TcpTable\0\0w,\0autoSize(int,int)\0id\0"
    "onAddRemoveClicked(size_t)\0"
};

void TcpTable::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        TcpTable *_t = static_cast<TcpTable *>(_o);
        switch (_id) {
        case 0: _t->autoSize((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 1: _t->onAddRemoveClicked((*reinterpret_cast< size_t(*)>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData TcpTable::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject TcpTable::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_TcpTable,
      qt_meta_data_TcpTable, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &TcpTable::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *TcpTable::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *TcpTable::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_TcpTable))
        return static_cast<void*>(const_cast< TcpTable*>(this));
    return QWidget::qt_metacast(_clname);
}

int TcpTable::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    return _id;
}
static const uint qt_meta_data_RoutingTableRow[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: signature, parameters, type, tag, flags
      20,   17,   16,   16, 0x05,

 // slots: signature, parameters, type, tag, flags
      53,   45,   16,   16, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_RoutingTableRow[] = {
    "RoutingTableRow\0\0id\0addRemoveClicked(size_t)\0"
    "checked\0onAddRemoveClicked(bool)\0"
};

void RoutingTableRow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        RoutingTableRow *_t = static_cast<RoutingTableRow *>(_o);
        switch (_id) {
        case 0: _t->addRemoveClicked((*reinterpret_cast< size_t(*)>(_a[1]))); break;
        case 1: _t->onAddRemoveClicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData RoutingTableRow::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject RoutingTableRow::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_RoutingTableRow,
      qt_meta_data_RoutingTableRow, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &RoutingTableRow::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *RoutingTableRow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *RoutingTableRow::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_RoutingTableRow))
        return static_cast<void*>(const_cast< RoutingTableRow*>(this));
    return QWidget::qt_metacast(_clname);
}

int RoutingTableRow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void RoutingTableRow::addRemoveClicked(size_t _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
static const uint qt_meta_data_RoutingTable[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: signature, parameters, type, tag, flags
      14,   13,   13,   13, 0x05,

 // slots: signature, parameters, type, tag, flags
      27,   24,   13,   13, 0x0a,
      48,   45,   13,   13, 0x0a,
      83,   75,   13,   13, 0x0a,
     102,   75,   13,   13, 0x0a,

       0        // eod
};

static const char qt_meta_stringdata_RoutingTable[] = {
    "RoutingTable\0\0changed()\0w,\0autoSize(int,int)\0"
    "id\0onAddRemoveClicked(size_t)\0checked\0"
    "onTcpClicked(bool)\0onApplyClicked(bool)\0"
};

void RoutingTable::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        RoutingTable *_t = static_cast<RoutingTable *>(_o);
        switch (_id) {
        case 0: _t->changed(); break;
        case 1: _t->autoSize((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 2: _t->onAddRemoveClicked((*reinterpret_cast< size_t(*)>(_a[1]))); break;
        case 3: _t->onTcpClicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 4: _t->onApplyClicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData RoutingTable::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject RoutingTable::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_RoutingTable,
      qt_meta_data_RoutingTable, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &RoutingTable::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *RoutingTable::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *RoutingTable::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_RoutingTable))
        return static_cast<void*>(const_cast< RoutingTable*>(this));
    return QWidget::qt_metacast(_clname);
}

int RoutingTable::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void RoutingTable::changed()
{
    QMetaObject::activate(this, &staticMetaObject, 0, 0);
}
static const uint qt_meta_data_MainWindow[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: signature, parameters, type, tag, flags
      12,   11,   11,   11, 0x08,
      21,   11,   11,   11, 0x08,
      53,   45,   11,   11, 0x08,
      76,   45,   11,   11, 0x08,
     100,   45,   11,   11, 0x08,
     124,   45,   11,   11, 0x08,
     150,   45,   11,   11, 0x08,
     174,   45,   11,   11, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_MainWindow[] = {
    "MainWindow\0\0onTick()\0onRoutingTableChanged()\0"
    "checked\0onNewFileClicked(bool)\0"
    "onOpenFileClicked(bool)\0onSaveFileClicked(bool)\0"
    "onSaveAsFileClicked(bool)\0"
    "onClearLogClicked(bool)\0onOpenLogClicked(bool)\0"
};

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        MainWindow *_t = static_cast<MainWindow *>(_o);
        switch (_id) {
        case 0: _t->onTick(); break;
        case 1: _t->onRoutingTableChanged(); break;
        case 2: _t->onNewFileClicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 3: _t->onOpenFileClicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 4: _t->onSaveFileClicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 5: _t->onSaveAsFileClicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 6: _t->onClearLogClicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 7: _t->onOpenLogClicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData MainWindow::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject MainWindow::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_MainWindow,
      qt_meta_data_MainWindow, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &MainWindow::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_MainWindow))
        return static_cast<void*>(const_cast< MainWindow*>(this));
    return QWidget::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
