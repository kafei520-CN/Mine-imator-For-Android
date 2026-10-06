// Hand-written stand-in for Qt moc. The Android kit's moc.exe needs MinGW
// runtime DLLs that are not in the qtbase package. Preview and RegionLoader
// are the only Q_OBJECT types in the engine.

#include "World/World.hpp"

#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>

#include <cstring>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "World.hpp did not include QObject."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This meta-object code matches Qt 5.15 moc revision 67."
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED

struct qt_meta_stringdata_CppProject__Preview_t {
	QByteArrayData data[5];
	char stringdata0[42];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
	Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
	qptrdiff(offsetof(qt_meta_stringdata_CppProject__Preview_t, stringdata0) + ofs \
		- idx * sizeof(QByteArrayData)) \
	)
static const qt_meta_stringdata_CppProject__Preview_t qt_meta_stringdata_CppProject__Preview = {
	{
		QT_MOC_LITERAL(0, 0, 19),
		QT_MOC_LITERAL(1, 20, 5),
		QT_MOC_LITERAL(2, 26, 7),
		QT_MOC_LITERAL(3, 34, 6),
		QT_MOC_LITERAL(4, 41, 0)
	},
	"CppProject::Preview\0Reset\0Confirm\0Cancel\0"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_CppProject__Preview[] = {
	8,
	0,
	0, 0,
	3, 14,
	0, 0,
	0, 0,
	0, 0,
	0,
	0,

	1, 0, 29, 4, 0x0a,
	2, 0, 30, 4, 0x0a,
	3, 0, 31, 4, 0x0a,

	QMetaType::Void,
	QMetaType::Void,
	QMetaType::Void,

	0
};

void CppProject::Preview::qt_static_metacall(QObject* object, QMetaObject::Call call, int id, void** args)
{
	if (call == QMetaObject::InvokeMetaMethod) {
		auto* self = static_cast<Preview*>(object);
		switch (id) {
		case 0: self->Reset(); break;
		case 1: self->Confirm(); break;
		case 2: self->Cancel(); break;
		default: break;
		}
	} else if (call == QMetaObject::RegisterMethodArgumentMetaType) {
		switch (id) {
		default: *reinterpret_cast<int*>(args[0]) = -1; break;
		}
	}
	Q_UNUSED(args);
}

QT_INIT_METAOBJECT const QMetaObject CppProject::Preview::staticMetaObject = { {
	QMetaObject::SuperData::link<QObject::staticMetaObject>(),
	qt_meta_stringdata_CppProject__Preview.data,
	qt_meta_data_CppProject__Preview,
	qt_static_metacall,
	nullptr,
	nullptr
} };

const QMetaObject* CppProject::Preview::metaObject() const
{
	return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void* CppProject::Preview::qt_metacast(const char* name)
{
	if (!name)
		return nullptr;
	if (!strcmp(name, qt_meta_stringdata_CppProject__Preview.stringdata0))
		return static_cast<void*>(this);
	return QObject::qt_metacast(name);
}

int CppProject::Preview::qt_metacall(QMetaObject::Call call, int id, void** args)
{
	id = QObject::qt_metacall(call, id, args);
	if (id < 0)
		return id;
	if (call == QMetaObject::InvokeMetaMethod) {
		if (id < 3)
			qt_static_metacall(this, call, id, args);
		id -= 3;
	} else if (call == QMetaObject::RegisterMethodArgumentMetaType) {
		if (id < 3)
			qt_static_metacall(this, call, id, args);
		id -= 3;
	}
	return id;
}

struct qt_meta_stringdata_CppProject__RegionLoader_t {
	QByteArrayData data[4];
	char stringdata0[55];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
	Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
	qptrdiff(offsetof(qt_meta_stringdata_CppProject__RegionLoader_t, stringdata0) + ofs \
		- idx * sizeof(QByteArrayData)) \
	)
static const qt_meta_stringdata_CppProject__RegionLoader_t qt_meta_stringdata_CppProject__RegionLoader = {
	{
		QT_MOC_LITERAL(0, 0, 24),
		QT_MOC_LITERAL(1, 25, 10),
		QT_MOC_LITERAL(2, 36, 17),
		QT_MOC_LITERAL(3, 54, 0)
	},
	"CppProject::RegionLoader\0UpdateDone\0UpdateLoadRegions\0"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_CppProject__RegionLoader[] = {
	8,
	0,
	0, 0,
	2, 14,
	0, 0,
	0, 0,
	0, 0,
	0,
	1,

	1, 0, 24, 3, 0x06,
	2, 0, 25, 3, 0x0a,

	QMetaType::Void,
	QMetaType::Void,

	0
};

void CppProject::RegionLoader::qt_static_metacall(QObject* object, QMetaObject::Call call, int id, void** args)
{
	if (call == QMetaObject::InvokeMetaMethod) {
		auto* self = static_cast<RegionLoader*>(object);
		switch (id) {
		case 0: self->UpdateDone(); break;
		case 1: self->UpdateLoadRegions(); break;
		default: break;
		}
	} else if (call == QMetaObject::IndexOfMethod) {
		int* result = reinterpret_cast<int*>(args[0]);
		using Method = void (RegionLoader::*)();
		if (*reinterpret_cast<Method*>(args[1]) == static_cast<Method>(&RegionLoader::UpdateDone)) {
			*result = 0;
			return;
		}
	} else if (call == QMetaObject::RegisterMethodArgumentMetaType) {
		switch (id) {
		default: *reinterpret_cast<int*>(args[0]) = -1; break;
		}
	}
}

QT_INIT_METAOBJECT const QMetaObject CppProject::RegionLoader::staticMetaObject = { {
	QMetaObject::SuperData::link<QObject::staticMetaObject>(),
	qt_meta_stringdata_CppProject__RegionLoader.data,
	qt_meta_data_CppProject__RegionLoader,
	qt_static_metacall,
	nullptr,
	nullptr
} };

const QMetaObject* CppProject::RegionLoader::metaObject() const
{
	return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void* CppProject::RegionLoader::qt_metacast(const char* name)
{
	if (!name)
		return nullptr;
	if (!strcmp(name, qt_meta_stringdata_CppProject__RegionLoader.stringdata0))
		return static_cast<void*>(this);
	return QObject::qt_metacast(name);
}

int CppProject::RegionLoader::qt_metacall(QMetaObject::Call call, int id, void** args)
{
	id = QObject::qt_metacall(call, id, args);
	if (id < 0)
		return id;
	if (call == QMetaObject::InvokeMetaMethod) {
		if (id < 2)
			qt_static_metacall(this, call, id, args);
		id -= 2;
	} else if (call == QMetaObject::RegisterMethodArgumentMetaType) {
		if (id < 2)
			qt_static_metacall(this, call, id, args);
		id -= 2;
	}
	return id;
}

void CppProject::RegionLoader::UpdateDone()
{
	QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

QT_WARNING_POP
QT_END_MOC_NAMESPACE
