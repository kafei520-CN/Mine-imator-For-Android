#include "AppHandler.hpp"
#include "AppWindow.hpp"
#include "Generated/GmlFunc.hpp"
#include "Generated/Scripts.hpp"
#include "Asset/Shader.hpp"
#include "Render/GLWidget.hpp"
#include "Render/PrimitiveRenderer.hpp"

#include <QtCore/qplugin.h>
#include <android/log.h>

extern "C" void mi_take_text(char* buf, int cap, int* backspace, int* enter);
extern "C" void mi_ime_set(int show);
extern "C" int mi_consume_warp();
extern "C" int mi_android_import_pump();

Q_IMPORT_PLUGIN(MiPlatformIntegrationPlugin)

extern "C" void mi_prepare_files();

static int g_editor_ready = 0;

extern "C" int mi_editor_ready(void)
{
	return g_editor_ready;
}

extern "C" int mi_editor_frame(int width, int height, float x, float y, int down, int right, int shift, int wheel)
{
	using namespace CppProject;
	static int failed = 0;
	if (failed || width <= 0 || height <= 0)
		return 0;
	try
	{
		if (!g_editor_ready)
		{
			mi_prepare_files();
			qputenv("QT_QPA_PLATFORM", QByteArray("mi"));
			if (!App)
			{
				static int argc = 1;
				static char name[] = "mineimator";
				static char* argv[] = { name, nullptr };
				new AppHandler(argc, argv);
			}
			App->StartOnGlSurface(width, height);
			Shader* shader = PrimitiveRenderer::GetShader();
			if (!shader || !shader->IsLoaded())
			{
				__android_log_print(ANDROID_LOG_ERROR, "MineImator", "primitive shader did not load");
				failed = 1;
				return 0;
			}
			g_editor_ready = 1;
			__android_log_print(ANDROID_LOG_INFO, "MineImator", "editor ready %dx%d", width, height);
		}
		else if (App->mainWindow
			&& (App->mainWindow->width() != width || App->mainWindow->height() != height))
		{
			App->StartOnGlSurface(width, height);
		}
		if (App->mainWindow)
		{
			// mousePos is in widget pixels. StepFrame divides by App->scale.
			const int widgetW = std::max(1, App->mainWindow->width());
			const int widgetH = std::max(1, App->mainWindow->height());
			App->mouseWindow = App->mainWindow;
			App->mainWindow->mousePos = QPoint(
				static_cast<int>(x) * widgetW / std::max(1, width),
				static_cast<int>(y) * widgetH / std::max(1, height));
			// Qt never receives the touch. Buttons fire on mouse_left_released.
			App->mainWindow->mouseDown[mb_left] = down != 0;
			App->mainWindow->mouseDown[mb_right] = right != 0;
			App->mainWindow->mouseWheel = wheel;
			if (mi_consume_warp() && global::_app)
			{
				// Drop the pointer jump when a finger is added or lifted.
				App->mainWindow->mouseLastPos = App->mainWindow->mousePos;
				const RealType uiScale = App->scale > 0 ? App->scale : 1;
				global::_app->mouse_current_x = IntType(App->mainWindow->mousePos.x() / uiScale);
				global::_app->mouse_current_y = IntType(App->mainWindow->mousePos.y() / uiScale);
			}
			static int shiftHeld = 0;
			static int backspaceHeld = 0;
			static int enterHeld = 0;
			if (shift)
				App->keyStateMap[vk_shift].SetDown(true);
			else if (shiftHeld)
				App->keyStateMap[vk_shift].SetDown(false);
			shiftHeld = shift;
			char text[1024];
			int backspace = 0;
			int enter = 0;
			mi_take_text(text, static_cast<int>(sizeof(text)), &backspace, &enter);
			if (text[0] != '\0')
				gmlGlobal::keyboard_string += QString::fromUtf8(text);
			if (backspace)
				App->keyStateMap[vk_backspace].SetDown(true);
			else if (backspaceHeld)
				App->keyStateMap[vk_backspace].SetDown(false);
			backspaceHeld = backspace;
			if (enter)
				App->keyStateMap[vk_enter].SetDown(true);
			else if (enterHeld)
				App->keyStateMap[vk_enter].SetDown(false);
			enterHeld = enter;
		}
		if (global::_app)
		{
			switch (mi_android_import_pump())
			{
				case 1: action_bench_item_tex(global::_app->id, e_option_BROWSE); break;
				case 2: action_bench_item_tex_material(global::_app->id, e_option_BROWSE); break;
				case 3: action_bench_item_tex_normal(global::_app->id, e_option_BROWSE); break;
				case 4: action_bench_model_tex(global::_app->id, e_option_BROWSE); break;
				case 5: action_bench_model_tex_material(global::_app->id, e_option_BROWSE); break;
				case 6: action_bench_model_tex_normal(global::_app->id, e_option_BROWSE); break;
				case 7: action_lib_model_tex(global::_app->id, e_option_BROWSE); break;
				case 8: action_lib_model_tex_material(global::_app->id, e_option_BROWSE); break;
				case 9: action_lib_model_tex_normal(global::_app->id, e_option_BROWSE); break;
				default: break;
			}
		}
		const int stepped = App->StepFrame() ? 1 : 0;
		if (stepped && global::_app)
		{
			const int editing = global::_app->textbox_isediting ? 1 : 0;
			static int imeShown = 0;
			if (editing != imeShown)
			{
				imeShown = editing;
				mi_ime_set(editing);
			}
		}
		return stepped;
	}
	catch (const QString& ex)
	{
		__android_log_print(ANDROID_LOG_ERROR, "MineImator", "editor: %s", ex.toUtf8().constData());
		failed = 1;
		g_editor_ready = 0;
		return 0;
	}
	catch (...)
	{
		__android_log_print(ANDROID_LOG_ERROR, "MineImator", "editor: unknown exception");
		failed = 1;
		g_editor_ready = 0;
		return 0;
	}
}
