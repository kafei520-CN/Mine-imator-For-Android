// Minimal Qt platform for the existing GLSurfaceView.
// The Android qtbase plugin expects QtActivity. This one only supplies a
// screen, a dummy window, and the EGL context GLSurfaceView already made.

#include <QtGui/5.15.2/QtGui/qpa/qplatformintegrationplugin.h>
#include <QtGui/5.15.2/QtGui/qpa/qplatformintegration.h>
#include <QtGui/5.15.2/QtGui/qpa/qplatformwindow.h>
#include <QtGui/5.15.2/QtGui/qpa/qplatformbackingstore.h>
#include <QtGui/5.15.2/QtGui/qpa/qplatformoffscreensurface.h>
#include <QtGui/5.15.2/QtGui/qpa/qplatformopenglcontext.h>
#include <QtGui/5.15.2/QtGui/qpa/qwindowsysteminterface.h>
#include <QtGui/QImage>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOffscreenSurface>

#include <EGL/egl.h>
#include <QtCore/5.15.2/QtCore/private/qeventdispatcher_unix_p.h>
#include <QtPlatformHeaders/QEGLNativeContext>

#include <cstring>

#define QT_STATICPLUGIN
#include <QtCore/qplugin.h>

static QSize g_ui_size(1280, 720);
static QPlatformScreen* g_ui_screen = nullptr;

namespace {

QSurfaceFormat GlesFormat()
{
	QSurfaceFormat format;
	format.setRenderableType(QSurfaceFormat::OpenGLES);
	format.setVersion(3, 1);
	format.setDepthBufferSize(24);
	format.setStencilBufferSize(8);
	return format;
}

class MiScreen : public QPlatformScreen
{
public:
	QRect geometry() const override { return QRect(QPoint(0, 0), g_ui_size); }
	int depth() const override { return 32; }
	QImage::Format format() const override { return QImage::Format_RGBA8888; }
};

class MiOffscreen : public QPlatformOffscreenSurface
{
public:
	explicit MiOffscreen(QOffscreenSurface* surface) : QPlatformOffscreenSurface(surface) {}
	QSurfaceFormat format() const override { return GlesFormat(); }
	bool isValid() const override { return true; }
};

class MiGlContext : public QPlatformOpenGLContext
{
public:
	explicit MiGlContext(QOpenGLContext* context)
	{
		const QVariant handle = context->nativeHandle();
		if (handle.canConvert<QEGLNativeContext>()) {
			const QEGLNativeContext native = handle.value<QEGLNativeContext>();
			m_context = native.context();
			m_display = native.display();
		}
		if (m_context == EGL_NO_CONTEXT)
			m_context = eglGetCurrentContext();
		if (m_display == EGL_NO_DISPLAY)
			m_display = eglGetCurrentDisplay();
	}

	QSurfaceFormat format() const override { return GlesFormat(); }
	void swapBuffers(QPlatformSurface*) override {}
	bool makeCurrent(QPlatformSurface*) override
	{
		return m_context != EGL_NO_CONTEXT && eglGetCurrentContext() == m_context;
	}
	void doneCurrent() override {}
	bool isValid() const override { return m_context != EGL_NO_CONTEXT; }
	QFunctionPointer getProcAddress(const char* procName) override
	{
		return static_cast<QFunctionPointer>(eglGetProcAddress(procName));
	}

private:
	EGLContext m_context = EGL_NO_CONTEXT;
	EGLDisplay m_display = EGL_NO_DISPLAY;
};

class MiBackingStore : public QPlatformBackingStore
{
public:
	explicit MiBackingStore(QWindow* window) : QPlatformBackingStore(window) {}
	QPaintDevice* paintDevice() override { return &m_image; }
	void flush(QWindow*, const QRegion&, const QPoint&) override {}
	void resize(const QSize& size, const QRegion&) override
	{
		m_image = QImage(size, QImage::Format_RGBA8888);
	}

private:
	QImage m_image;
};

class MiIntegration : public QPlatformIntegration
{
public:
	bool hasCapability(Capability cap) const override
	{
		if (cap == OpenGL)
			return true;
		return QPlatformIntegration::hasCapability(cap);
	}

	QPlatformWindow* createPlatformWindow(QWindow* window) const override
	{
		return new QPlatformWindow(window);
	}

	QPlatformBackingStore* createPlatformBackingStore(QWindow* window) const override
	{
		return new MiBackingStore(window);
	}

	QPlatformOpenGLContext* createPlatformOpenGLContext(QOpenGLContext* context) const override
	{
		return new MiGlContext(context);
	}

	QPlatformOffscreenSurface* createPlatformOffscreenSurface(QOffscreenSurface* surface) const override
	{
		return new MiOffscreen(surface);
	}

	QAbstractEventDispatcher* createEventDispatcher() const override
	{
		return new QEventDispatcherUNIX;
	}

	void initialize() override
	{
		m_screen = new MiScreen;
		g_ui_screen = m_screen;
		QWindowSystemInterface::handleScreenAdded(m_screen, true);
	}

private:
	MiScreen* m_screen = nullptr;
};

}  // namespace

extern "C" void mi_set_ui_size(int width, int height)
{
	if (width <= 0 || height <= 0)
		return;
	g_ui_size = QSize(width, height);
	if (g_ui_screen == nullptr)
		return;
	if (QScreen* screen = g_ui_screen->screen())
		QWindowSystemInterface::handleScreenGeometryChange(screen, g_ui_screen->geometry(), g_ui_screen->geometry());
}

extern "C" void mi_ui_size(int* width, int* height)
{
	if (width)
		*width = g_ui_size.width();
	if (height)
		*height = g_ui_size.height();
}

class MiPlatformIntegrationPlugin : public QPlatformIntegrationPlugin
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID QPlatformIntegrationFactoryInterface_iid FILE "mi.json")

public:
	QPlatformIntegration* create(const QString& key, const QStringList&) override
	{
		if (key.compare(QLatin1String("mi"), Qt::CaseInsensitive) != 0)
			return nullptr;
		return new MiIntegration;
	}
};

static const char kPluginIid[] = "org.qt-project.Qt.QPA.QPlatformIntegrationFactoryInterface.5.3";
static const char kPluginClass[] = "MiPlatformIntegrationPlugin";
static_assert(sizeof(kPluginIid) - 1 == 62, "plugin iid length");
static_assert(sizeof(kPluginClass) - 1 == 27, "plugin class length");

QT_PLUGIN_METADATA_SECTION
static const unsigned char qt_pluginMetaData[] = {
	'Q', 'T', 'M', 'E', 'T', 'A', 'D', 'A', 'T', 'A', ' ', '!',
	0x00, 0x05, 0x0F, 0x00,
	0xBF,
	0x02,
	0x78, 0x3E,
	'o','r','g','.','q','t','-','p','r','o','j','e','c','t','.','Q','t','.','Q','P','A','.',
	'Q','P','l','a','t','f','o','r','m','I','n','t','e','g','r','a','t','i','o','n','F','a','c','t','o','r','y',
	'I','n','t','e','r','f','a','c','e','.','5','.','3',
	0x03,
	0x78, 0x1B,
	'M','i','P','l','a','t','f','o','r','m','I','n','t','e','g','r','a','t','i','o','n','P','l','u','g','i','n',
	0x04,
	0xA1,
	0x64, 'K', 'e', 'y', 's',
	0x81,
	0x62, 'm', 'i',
	0xFF
};

struct qt_meta_stringdata_MiPlatformIntegrationPlugin_t {
	QByteArrayData data[1];
	char stringdata0[28];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
	Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
	qptrdiff(offsetof(qt_meta_stringdata_MiPlatformIntegrationPlugin_t, stringdata0) + ofs \
		- idx * sizeof(QByteArrayData)) \
	)
static const qt_meta_stringdata_MiPlatformIntegrationPlugin_t qt_meta_stringdata_MiPlatformIntegrationPlugin = {
	{
		QT_MOC_LITERAL(0, 0, 27)
	},
	"MiPlatformIntegrationPlugin"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MiPlatformIntegrationPlugin[] = {
	8,
	0,
	0, 0,
	0, 14,
	0, 0,
	0, 0,
	0, 0,
	0,
	0,
	0
};

void MiPlatformIntegrationPlugin::qt_static_metacall(QObject*, QMetaObject::Call, int, void**)
{
}

QT_INIT_METAOBJECT const QMetaObject MiPlatformIntegrationPlugin::staticMetaObject = { {
	QMetaObject::SuperData::link<QPlatformIntegrationPlugin::staticMetaObject>(),
	qt_meta_stringdata_MiPlatformIntegrationPlugin.data,
	qt_meta_data_MiPlatformIntegrationPlugin,
	qt_static_metacall,
	nullptr,
	nullptr
} };

const QMetaObject* MiPlatformIntegrationPlugin::metaObject() const
{
	return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void* MiPlatformIntegrationPlugin::qt_metacast(const char* name)
{
	if (!name)
		return nullptr;
	if (!strcmp(name, qt_meta_stringdata_MiPlatformIntegrationPlugin.stringdata0))
		return static_cast<void*>(this);
	return QPlatformIntegrationPlugin::qt_metacast(name);
}

int MiPlatformIntegrationPlugin::qt_metacall(QMetaObject::Call call, int id, void** args)
{
	return QPlatformIntegrationPlugin::qt_metacall(call, id, args);
}

static QObject* qt_plugin_instance_MiPlatformIntegrationPlugin()
{
	static QPointer<QObject> instance;
	if (!instance)
		instance = new MiPlatformIntegrationPlugin;
	return instance;
}

static const char* qt_plugin_query_metadata_MiPlatformIntegrationPlugin()
{
	return reinterpret_cast<const char*>(qt_pluginMetaData);
}

const QStaticPlugin qt_static_plugin_MiPlatformIntegrationPlugin()
{
	QStaticPlugin plugin = {
		qt_plugin_instance_MiPlatformIntegrationPlugin,
		qt_plugin_query_metadata_MiPlatformIntegrationPlugin
	};
	return plugin;
}
