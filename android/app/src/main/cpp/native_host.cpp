// Android GLES host for the Mine-imator C++ runtime.
//
// The desktop executable enters through AppHandler::Run. This library owns
// the phone window, the GLES 3 context created by GLSurfaceView, and the
// touch queue. Each GL frame calls mi_step_frame, the same entry the
// desktop timer uses. Until AppHandler is constructed, the host still draws.

#include "gles_shader.hpp"
#include "GlesDevice.hpp"
#include "Platform/FrameBridge.hpp"
#include "Platform/Storage.hpp"

#define AL_LIBTYPE_STATIC
#include <AL/al.h>
#include <zip.h>

extern "C" unsigned avcodec_version(void);

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <GLES3/gl3.h>
#include <jni.h>

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

// This file is built together with AppHandler.cpp. That file owns mi_step_frame.
// A host-only library must define mi_step_frame again and call HostDrawFrame from it.
extern "C" int mi_app_running(void);
extern "C" int mi_editor_frame(int width, int height, float x, float y, int down, int right, int shift, int wheel);
extern "C" void mi_take_text(char* buf, int cap, int* backspace, int* enter);
extern "C" void mi_ime_set(int show);

namespace {

constexpr int kMaxTouchEvents = 64;
constexpr char kLogTag[] = "MineImator";

enum TouchAction {
	kTouchDown = 0,
	kTouchMove = 1,
	kTouchUp = 2,
	kTouchCancel = 3,
};

struct TouchEvent {
	int action = 0;
	int pointer = 0;
	float x = 0.f;
	float y = 0.f;
};

struct Host {
	std::mutex mutex;
	std::string files_dir;
	std::string cache_dir;
	std::vector<TouchEvent> pending;
	int width = 0;
	int height = 0;
	float density = 1.f;
	bool pointer_down = false;
	float pointer_x = 0.f;
	float pointer_y = 0.f;
	bool tapped = false;
	float tap_x = 0.f;
	float tap_y = 0.f;
	int right = 0;
	int shift = 0;
	int wheel = 0;
	int warp = 0;
	GLuint program = 0;
	GLuint vbo = 0;
	GLuint vignette_program = 0;
	GLuint vignette_vbo = 0;
	GLuint vignette_tex = 0;
	GLint vignette_mvp = -1;
	GLint vignette_screen = -1;
	GLint vignette_radius = -1;
	GLint vignette_softness = -1;
	GLint vignette_strength = -1;
	GLint vignette_color = -1;
	GLint vignette_uv = -1;
	GLint vignette_repeat = -1;
	GLint vignette_sampler = -1;
	std::string vignette_vs_src;
	std::string vignette_fs_src;
	bool started = false;
};

Host g_host;
CppProject::GlesDevice g_device;
AAssetManager* g_assets = nullptr;
jobject g_asset_ref = nullptr;
JavaVM* g_vm = nullptr;
jobject g_activity = nullptr;
std::mutex g_pick_mutex;
std::condition_variable g_pick_cv;
bool g_pick_done = false;
int g_pick_mode = 0;
std::string g_pick_path;
std::mutex g_text_mutex;
std::string g_text;
int g_backspace = 0;
int g_enter = 0;

struct Finger {
	int id = 0;
	float x = 0.f;
	float y = 0.f;
};
std::vector<Finger> g_fingers;
float g_pinch_span = -1.f;

void UpsertFinger(int id, float x, float y) {
	for (Finger& finger : g_fingers) {
		if (finger.id == id) {
			finger.x = x;
			finger.y = y;
			return;
		}
	}
	g_fingers.push_back(Finger{id, x, y});
}

void EraseFinger(int id) {
	g_fingers.erase(
		std::remove_if(g_fingers.begin(), g_fingers.end(), [id](const Finger& finger) {
			return finger.id == id;
		}),
		g_fingers.end());
}

void ResolveFingers() {
	g_host.wheel = 0;
	g_host.right = 0;
	static int previousCount = 0;
	const int count = static_cast<int>(g_fingers.size());
	// A new or lifted finger jumps the pointer. That one frame must not move the camera.
	g_host.warp = count != previousCount ? 1 : 0;
	previousCount = count;
	if (g_fingers.empty()) {
		g_host.pointer_down = g_host.tapped;
		g_host.shift = 0;
		g_pinch_span = -1.f;
		if (g_host.tapped) {
			g_host.pointer_x = g_host.tap_x;
			g_host.pointer_y = g_host.tap_y;
		}
		return;
	}
	if (count >= 3) {
		// Three fingers stand in for the right mouse button (walk).
		g_host.pointer_down = false;
		g_host.right = 1;
		g_host.shift = 0;
		g_host.pointer_x = g_fingers[0].x;
		g_host.pointer_y = g_fingers[0].y;
		g_pinch_span = -1.f;
		return;
	}
	if (count == 1) {
		g_host.pointer_down = true;
		g_host.pointer_x = g_fingers[0].x;
		g_host.pointer_y = g_fingers[0].y;
		g_host.shift = 0;
		g_pinch_span = -1.f;
		return;
	}
	const Finger& a = g_fingers[0];
	const Finger& b = g_fingers[1];
	g_host.pointer_down = true;
	g_host.right = 0;
	const float dx = b.x - a.x;
	const float dy = b.y - a.y;
	const float span = std::sqrt(dx * dx + dy * dy);
	float pinch = 0.f;
	if (g_pinch_span > 0.f)
		pinch = span - g_pinch_span;
	else
		g_pinch_span = span;
	if (std::fabs(pinch) > 80.f) {
		// Pinch zooms and does not also pan.
		g_host.wheel = pinch > 0.f ? -1 : 1;
		g_pinch_span = span;
		g_host.shift = 0;
		g_host.warp = 1;
		return;
	}
	g_host.pointer_x = (a.x + b.x) * 0.5f;
	g_host.pointer_y = (a.y + b.y) * 0.5f;
	// Two fingers hold Shift, so a drag pans the camera instead of orbiting.
	g_host.shift = 1;
	g_host.wheel = 0;
}

const char kVertexShader[] = R"glsl(#version 300 es
layout(location = 0) in vec2 a_position;
void main() {
	gl_Position = vec4(a_position, 0.0, 1.0);
}
)glsl";

const char kFragmentShader[] = R"glsl(#version 300 es
precision mediump float;
out vec4 frag_color;
void main() {
	frag_color = vec4(0.42, 0.74, 0.27, 1.0);
}
)glsl";

GLuint CompileShader(GLenum type, const char* source) {
	const GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);
	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled) {
		return shader;
	}
	char log[512];
	glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
	__android_log_print(ANDROID_LOG_ERROR, kLogTag, "shader: %s", log);
	glDeleteShader(shader);
	return 0;
}

void CreateProgram() {
	if (g_host.program != 0) {
		glDeleteProgram(g_host.program);
		g_host.program = 0;
	}
	const GLuint vertex = CompileShader(GL_VERTEX_SHADER, kVertexShader);
	const GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
	if (vertex == 0 || fragment == 0) {
		if (vertex != 0) {
			glDeleteShader(vertex);
		}
		if (fragment != 0) {
			glDeleteShader(fragment);
		}
		return;
	}
	const GLuint program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	GLint linked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked) {
		char log[512];
		glGetProgramInfoLog(program, sizeof(log), nullptr, log);
		__android_log_print(ANDROID_LOG_ERROR, kLogTag, "link: %s", log);
		glDeleteProgram(program);
		return;
	}
	g_host.program = program;
	if (g_host.vbo != 0) {
		glDeleteBuffers(1, &g_host.vbo);
		g_host.vbo = 0;
	}
	glGenBuffers(1, &g_host.vbo);
}

std::string JStringToStd(JNIEnv* env, jstring value) {
	if (value == nullptr) {
		return {};
	}
	const char* chars = env->GetStringUTFChars(value, nullptr);
	std::string copy = chars == nullptr ? std::string() : std::string(chars);
	if (chars != nullptr) {
		env->ReleaseStringUTFChars(value, chars);
	}
	return copy;
}

void DrainTouch() {
	std::vector<TouchEvent> events;
	{
		std::lock_guard<std::mutex> lock(g_host.mutex);
		events.swap(g_host.pending);
	}
	g_host.tapped = false;
	for (const TouchEvent& event : events) {
		if (event.action == kTouchDown) {
			g_host.tapped = true;
			g_host.tap_x = event.x;
			g_host.tap_y = event.y;
			UpsertFinger(event.pointer, event.x, event.y);
		} else if (event.action == kTouchMove) {
			UpsertFinger(event.pointer, event.x, event.y);
		} else {
			EraseFinger(event.pointer);
		}
	}
	ResolveFingers();
}

void DrawMarker() {
	if (g_host.program == 0 || g_host.vbo == 0 || g_host.width <= 0 || g_host.height <= 0) {
		return;
	}
	float center_x = 0.f;
	float center_y = 0.f;
	if (g_host.pointer_down) {
		center_x = (g_host.pointer_x / static_cast<float>(g_host.width)) * 2.f - 1.f;
		center_y = 1.f - (g_host.pointer_y / static_cast<float>(g_host.height)) * 2.f;
	}
	const float half_x = 80.f / static_cast<float>(g_host.width);
	const float half_y = 80.f / static_cast<float>(g_host.height);
	const float vertices[] = {
		center_x - half_x, center_y - half_y,
		center_x + half_x, center_y - half_y,
		center_x - half_x, center_y + half_y,
		center_x + half_x, center_y + half_y,
	};
	glUseProgram(g_host.program);
	glBindBuffer(GL_ARRAY_BUFFER, g_host.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void EnsureParent(const std::string& file)
{
	for (size_t i = 1; i < file.size(); ++i) {
		if (file[i] != '/')
			continue;
		const std::string dir = file.substr(0, i);
		mkdir(dir.c_str(), 0700);
	}
}

int CopyAssetList(const char* const* paths, const std::string& dest_root)
{
	int copied = 0;
	for (int i = 0; paths[i] != nullptr; ++i) {
		const std::string rel = paths[i];
		const std::string dest = dest_root + "/" + rel;
		if (std::ifstream(dest, std::ios::binary).good())
			continue;
		AAsset* asset = AAssetManager_open(g_assets, rel.c_str(), AASSET_MODE_BUFFER);
		if (asset == nullptr) {
			__android_log_print(ANDROID_LOG_ERROR, kLogTag, "missing asset %s", rel.c_str());
			continue;
		}
		const off_t length = AAsset_getLength(asset);
		std::string bytes(static_cast<size_t>(length > 0 ? length : 0), '\0');
		if (length > 0)
			AAsset_read(asset, bytes.data(), bytes.size());
		AAsset_close(asset);
		EnsureParent(dest);
		std::ofstream out(dest, std::ios::binary);
		if (!out)
			continue;
		out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		++copied;
	}
	return copied;
}

extern "C" void mi_prepare_files()
{
	static bool done = false;
	if (done || g_assets == nullptr || g_host.files_dir.empty())
		return;
	static const char* kGmShaders[] = {
#include "shader_gm.inc"
		nullptr
	};
	static const char* kAssetShaders[] = {
#include "shader_asset.inc"
		nullptr
	};
	static const char* kDataFiles[] = {
#include "datafiles.inc"
		nullptr
	};
	static const char* kSprites[] = {
#include "sprites.inc"
		nullptr
	};
	const int gm = CopyAssetList(kGmShaders, g_host.files_dir + "/shaders");
	const int asset = CopyAssetList(kAssetShaders, g_host.files_dir + "/assets/Shaders");
	const int data = CopyAssetList(kDataFiles, g_host.files_dir);
	const int sprites = CopyAssetList(kSprites, g_host.files_dir + "/assets");
	QDir::setCurrent(QString::fromStdString(g_host.files_dir));
	__android_log_print(ANDROID_LOG_INFO, kLogTag, "copied gm=%d asset=%d data=%d sprites=%d cwd=%s",
		gm, asset, data, sprites, QDir::currentPath().toUtf8().constData());
	done = true;
}

std::string LoadAsset(const char* path) {
	if (g_assets == nullptr) {
		return {};
	}
	AAsset* asset = AAssetManager_open(g_assets, path, AASSET_MODE_BUFFER);
	if (asset == nullptr) {
		__android_log_print(ANDROID_LOG_ERROR, kLogTag, "missing asset %s", path);
		return {};
	}
	const off_t length = AAsset_getLength(asset);
	std::string text(static_cast<size_t>(length), '\0');
	if (length > 0) {
		AAsset_read(asset, text.data(), text.size());
	}
	AAsset_close(asset);
	return text;
}

void CreateVignette() {
	if (g_host.vignette_vs_src.empty() || g_host.vignette_fs_src.empty()) {
		return;
	}
	const std::string vertex_source = TranslateMineimatorShader(g_host.vignette_vs_src, true);
	const std::string fragment_source = TranslateMineimatorShader(g_host.vignette_fs_src, false);
	g_host.vignette_program = g_device.Link(vertex_source, fragment_source);
	if (g_host.vignette_program == 0) {
		return;
	}
	const GLuint program = g_host.vignette_program;
	g_host.vignette_mvp = glGetUniformLocation(program, "_uMatrixMVP");
	g_host.vignette_screen = glGetUniformLocation(program, "uScreenSize");
	g_host.vignette_radius = glGetUniformLocation(program, "uRadius");
	g_host.vignette_softness = glGetUniformLocation(program, "uSoftness");
	g_host.vignette_strength = glGetUniformLocation(program, "uStrength");
	g_host.vignette_color = glGetUniformLocation(program, "uColor");
	g_host.vignette_uv = glGetUniformLocation(program, "_uUvRect");
	g_host.vignette_repeat = glGetUniformLocation(program, "_uTexRepeat");
	g_host.vignette_sampler = glGetUniformLocation(program, "_uBaseTexture");

	const int position = glGetAttribLocation(program, "in_Position");
	const int color = glGetAttribLocation(program, "in_Colour");
	const int uv = glGetAttribLocation(program, "in_TextureCoord");
	const float vertices[] = {
		-1.f, -1.f, 0.f, 1.f, 1.f, 1.f, 1.f, 0.f, 0.f,
		1.f, -1.f, 0.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0.f,
		-1.f, 1.f, 0.f, 1.f, 1.f, 1.f, 1.f, 0.f, 1.f,
		1.f, 1.f, 0.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f,
	};
	glGenBuffers(1, &g_host.vignette_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_host.vignette_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	const GLsizei stride = sizeof(float) * 9;
	if (position >= 0) {
		glEnableVertexAttribArray(position);
		glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
	}
	if (color >= 0) {
		glEnableVertexAttribArray(color);
		glVertexAttribPointer(color, 4, GL_FLOAT, GL_FALSE, stride,
			reinterpret_cast<void*>(sizeof(float) * 3));
	}
	if (uv >= 0) {
		glEnableVertexAttribArray(uv);
		glVertexAttribPointer(uv, 2, GL_FLOAT, GL_FALSE, stride,
			reinterpret_cast<void*>(sizeof(float) * 7));
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	unsigned char pixels[64 * 64 * 4];
	for (int y = 0; y < 64; ++y) {
		for (int x = 0; x < 64; ++x) {
			const bool light = ((x / 8) + (y / 8)) % 2 == 0;
			unsigned char* pixel = pixels + (y * 64 + x) * 4;
			pixel[0] = light ? 186 : 42;
			pixel[1] = light ? 214 : 72;
			pixel[2] = light ? 96 : 36;
			pixel[3] = 255;
		}
	}
	g_host.vignette_tex = g_device.CreateRgbaTexture(64, 64, pixels);
	__android_log_print(ANDROID_LOG_INFO, kLogTag, "shader_vignette linked");
}

void DrawVignette() {
	if (g_host.vignette_program == 0 || g_host.width <= 0 || g_host.height <= 0) {
		return;
	}
	const bool offscreen = g_device.EnsureOffscreen(g_host.width, g_host.height);
	if (offscreen) {
		g_device.BindOffscreen();
		glViewport(0, 0, g_host.width, g_host.height);
		glClearColor(0.102f, 0.102f, 0.102f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	const float identity[16] = {
		1.f, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f,
	};
	const float uv_rect[4] = {0.f, 0.f, 1.f, 1.f};
	const int repeat = 0;
	glUseProgram(g_host.vignette_program);
	glUniformMatrix4fv(g_host.vignette_mvp, 1, GL_FALSE, identity);
	glUniform2f(g_host.vignette_screen,
		static_cast<float>(g_host.width), static_cast<float>(g_host.height));
	glUniform1f(g_host.vignette_radius, 0.48f);
	glUniform1f(g_host.vignette_softness, 0.28f);
	glUniform1f(g_host.vignette_strength, g_host.pointer_down ? 1.f : 0.55f);
	glUniform4f(g_host.vignette_color, 0.02f, 0.04f, 0.02f, 1.f);
	glUniform4fv(g_host.vignette_uv, 1, uv_rect);
	glUniform1iv(g_host.vignette_repeat, 1, &repeat);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_host.vignette_tex);
	glUniform1i(g_host.vignette_sampler, 0);
	glBindBuffer(GL_ARRAY_BUFFER, g_host.vignette_vbo);
	const GLsizei stride = sizeof(float) * 9;
	const int position = glGetAttribLocation(g_host.vignette_program, "in_Position");
	const int color = glGetAttribLocation(g_host.vignette_program, "in_Colour");
	const int uv = glGetAttribLocation(g_host.vignette_program, "in_TextureCoord");
	if (position >= 0) {
		glEnableVertexAttribArray(position);
		glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
	}
	if (color >= 0) {
		glEnableVertexAttribArray(color);
		glVertexAttribPointer(color, 4, GL_FLOAT, GL_FALSE, stride,
			reinterpret_cast<void*>(sizeof(float) * 3));
	}
	if (uv >= 0) {
		glEnableVertexAttribArray(uv);
		glVertexAttribPointer(uv, 2, GL_FLOAT, GL_FALSE, stride,
			reinterpret_cast<void*>(sizeof(float) * 7));
	}
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if (offscreen) {
		g_device.BlitToScreen(g_host.width, g_host.height);
	}
}

}  // namespace

static void HostDrawFrame(const MiFrameInput* input) {
	if (input != nullptr) {
		if (input->width > 0 && input->height > 0) {
			g_host.width = input->width;
			g_host.height = input->height;
		}
		g_host.pointer_x = input->x;
		g_host.pointer_y = input->y;
		g_host.pointer_down = input->pointer_down != 0;
	}
	static bool logged = false;
	if (!logged) {
		__android_log_print(ANDROID_LOG_INFO, kLogTag, "mi_step_frame %dx%d",
			g_host.width, g_host.height);
		logged = true;
	}
	if (g_host.tapped) {
		__android_log_print(ANDROID_LOG_INFO, kLogTag, "touch %.0f,%.0f",
			g_host.tap_x, g_host.tap_y);
	}
	glViewport(0, 0, g_host.width, g_host.height);
	glClearColor(0.102f, 0.102f, 0.102f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	DrawVignette();
	DrawMarker();
}

extern "C" void mi_take_text(char* buf, int cap, int* backspace, int* enter) {
	std::lock_guard<std::mutex> lock(g_text_mutex);
	if (buf != nullptr && cap > 0) {
		const int count = std::min(cap - 1, static_cast<int>(g_text.size()));
		if (count > 0)
			std::memcpy(buf, g_text.data(), static_cast<size_t>(count));
		buf[count] = '\0';
	}
	g_text.clear();
	if (backspace != nullptr)
		*backspace = g_backspace > 0 ? 1 : 0;
	if (g_backspace > 0)
		--g_backspace;
	if (enter != nullptr)
		*enter = g_enter > 0 ? 1 : 0;
	if (g_enter > 0)
		--g_enter;
}

extern "C" void mi_ime_set(int show) {
	if (g_vm == nullptr || g_activity == nullptr)
		return;
	JNIEnv* env = nullptr;
	bool attached = false;
	const jint got = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
	if (got == JNI_EDETACHED) {
		if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
			return;
		attached = true;
	} else if (got != JNI_OK || env == nullptr) {
		return;
	}
	jclass keyboard = env->FindClass("com/mineimator/app/Keyboard");
	jmethodID set_visible = keyboard == nullptr ? nullptr : env->GetStaticMethodID(
		keyboard, "setVisible", "(Landroid/app/Activity;Z)V");
	if (set_visible != nullptr)
		env->CallStaticVoidMethod(keyboard, set_visible, g_activity, show ? JNI_TRUE : JNI_FALSE);
	if (env->ExceptionCheck())
		env->ExceptionClear();
	if (attached)
		g_vm->DetachCurrentThread();
}

extern "C" int mi_consume_warp() {
	const int warp = g_host.warp;
	g_host.warp = 0;
	return warp;
}

extern "C" void mi_pick_begin() {
	if (g_vm == nullptr || g_activity == nullptr)
		return;
	if (g_pick_mode == 1)
		return;
	JNIEnv* env = nullptr;
	if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK || env == nullptr)
		return;
	{
		std::lock_guard<std::mutex> lock(g_pick_mutex);
		g_pick_done = false;
		g_pick_path.clear();
		g_pick_mode = 1;
	}
	jclass picker = env->FindClass("com/mineimator/app/FilePicker");
	jmethodID open = picker == nullptr ? nullptr : env->GetStaticMethodID(picker, "open", "(Landroid/app/Activity;)V");
	if (open == nullptr) {
		std::lock_guard<std::mutex> lock(g_pick_mutex);
		g_pick_mode = 0;
		g_pick_done = true;
		return;
	}
	env->CallStaticVoidMethod(picker, open, g_activity);
	if (env->ExceptionCheck()) {
		env->ExceptionClear();
		std::lock_guard<std::mutex> lock(g_pick_mutex);
		g_pick_mode = 0;
		g_pick_done = true;
	}
}

extern "C" int mi_pick_poll(char* buf, int cap) {
	std::lock_guard<std::mutex> lock(g_pick_mutex);
	if (!g_pick_done)
		return 0;
	if (buf != nullptr && cap > 0) {
		const int count = std::min(cap - 1, static_cast<int>(g_pick_path.size()));
		if (count > 0)
			std::memcpy(buf, g_pick_path.data(), static_cast<size_t>(count));
		buf[count] = '\0';
	}
	g_pick_path.clear();
	g_pick_done = false;
	g_pick_mode = 0;
	return 1;
}

const char* mi_storage_pick_open() {
	if (g_vm == nullptr || g_activity == nullptr) {
		return "";
	}
	JNIEnv* env = nullptr;
	if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK || env == nullptr) {
		return "";
	}
	{
		std::lock_guard<std::mutex> lock(g_pick_mutex);
		g_pick_done = false;
		g_pick_path.clear();
	}
	jclass picker = env->FindClass("com/mineimator/app/FilePicker");
	jmethodID open = env->GetStaticMethodID(picker, "open", "(Landroid/app/Activity;)V");
	if (picker == nullptr || open == nullptr) {
		return "";
	}
	env->CallStaticVoidMethod(picker, open, g_activity);
	if (env->ExceptionCheck()) {
		env->ExceptionClear();
		return "";
	}
	std::unique_lock<std::mutex> lock(g_pick_mutex);
	g_pick_cv.wait(lock, [] { return g_pick_done; });
	return g_pick_path.c_str();
}

extern "C" JNIEXPORT void JNICALL
Java_com_mineimator_app_NativeHost_nativeCommitText(JNIEnv* env, jclass, jstring text) {
	const std::string chars = JStringToStd(env, text);
	if (chars.empty())
		return;
	std::lock_guard<std::mutex> lock(g_text_mutex);
	g_text += chars;
}

extern "C" JNIEXPORT void JNICALL
Java_com_mineimator_app_NativeHost_nativeKey(JNIEnv*, jclass, jint code) {
	std::lock_guard<std::mutex> lock(g_text_mutex);
	if (code == 8)
		++g_backspace;
	else if (code == 13)
		++g_enter;
}

extern "C" JNIEXPORT void JNICALL
Java_com_mineimator_app_NativeHost_nativePickResult(JNIEnv* env, jclass, jstring path) {
	std::lock_guard<std::mutex> lock(g_pick_mutex);
	g_pick_path = JStringToStd(env, path);
	g_pick_done = true;
	g_pick_cv.notify_one();
}

extern "C" JNIEXPORT void JNICALL
Java_com_mineimator_app_NativeHost_nativeStart(
	JNIEnv* env,
	jclass,
	jobject activity,
	jobject assets,
	jstring files_dir,
	jstring cache_dir) {
	env->GetJavaVM(&g_vm);
	if (g_activity != nullptr) {
		env->DeleteGlobalRef(g_activity);
		g_activity = nullptr;
	}
	if (activity != nullptr) {
		g_activity = env->NewGlobalRef(activity);
	}
	if (g_asset_ref != nullptr) {
		env->DeleteGlobalRef(g_asset_ref);
		g_asset_ref = nullptr;
		g_assets = nullptr;
	}
	if (assets != nullptr) {
		g_asset_ref = env->NewGlobalRef(assets);
		g_assets = AAssetManager_fromJava(env, g_asset_ref);
	}
	std::string files = JStringToStd(env, files_dir);
	std::string cache = JStringToStd(env, cache_dir);
	std::string vertex = LoadAsset("shader_vignette/shader_vignette.vsh");
	std::string fragment = LoadAsset("shader_vignette/shader_vignette.fsh");
	{
		std::lock_guard<std::mutex> lock(g_host.mutex);
		g_host.files_dir = std::move(files);
		g_host.cache_dir = std::move(cache);
		g_host.vignette_vs_src = std::move(vertex);
		g_host.vignette_fs_src = std::move(fragment);
		g_host.started = true;
		__android_log_print(
			ANDROID_LOG_INFO,
			kLogTag,
			"files=%s cache=%s openal_err=%d libzip=%s avcodec=0x%x",
			g_host.files_dir.c_str(),
			g_host.cache_dir.c_str(),
			static_cast<int>(alGetError()),
			zip_libzip_version(),
			avcodec_version());
		mi_storage_set_root(g_host.files_dir.c_str());
		mi_storage_set_cache(g_host.cache_dir.c_str());
		mi_storage_ensure_dirs();
		__android_log_print(
			ANDROID_LOG_INFO,
			kLogTag,
			"saves=%s projects=%s imports=%s",
			mi_storage_saves_dir(),
			mi_storage_projects_dir(),
			mi_storage_imports_dir());
	}
}

extern "C" JNIEXPORT void JNICALL
Java_com_mineimator_app_NativeHost_nativeSurfaceCreated(JNIEnv*, jclass) {
	const GLubyte* version = glGetString(GL_VERSION);
	const GLubyte* renderer = glGetString(GL_RENDERER);
	__android_log_print(
		ANDROID_LOG_INFO,
		kLogTag,
		"GLES %s / %s",
		version == nullptr ? "?" : reinterpret_cast<const char*>(version),
		renderer == nullptr ? "?" : reinterpret_cast<const char*>(renderer));
	CreateProgram();
	CreateVignette();
}

extern "C" JNIEXPORT void JNICALL
Java_com_mineimator_app_NativeHost_nativeResize(
	JNIEnv*,
	jclass,
	jint width,
	jint height,
	jfloat density) {
	g_host.width = width;
	g_host.height = height;
	g_host.density = density;
	glViewport(0, 0, width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_mineimator_app_NativeHost_nativeFrame(JNIEnv*, jclass) {
	DrainTouch();
	MiFrameInput input;
	input.width = g_host.width;
	input.height = g_host.height;
	input.x = g_host.pointer_x;
	input.y = g_host.pointer_y;
	const int down = g_host.pointer_down ? 1 : 0;
	input.pointer_down = down;
	if (mi_editor_frame(
			g_host.width, g_host.height, g_host.pointer_x, g_host.pointer_y,
			down, g_host.right, g_host.shift, g_host.wheel))
		return;
	mi_step_frame(&input);
	if (!mi_app_running())
		HostDrawFrame(&input);
}

extern "C" JNIEXPORT void JNICALL
Java_com_mineimator_app_NativeHost_nativeTouch(
	JNIEnv*,
	jclass,
	jint action,
	jint pointer,
	jfloat x,
	jfloat y) {
	std::lock_guard<std::mutex> lock(g_host.mutex);
	if (static_cast<int>(g_host.pending.size()) >= kMaxTouchEvents) {
		g_host.pending.erase(g_host.pending.begin());
	}
	g_host.pending.push_back(TouchEvent{action, pointer, x, y});
}

extern "C" JNIEXPORT void JNICALL
Java_com_mineimator_app_NativeHost_nativeStop(JNIEnv* env, jclass) {
	std::lock_guard<std::mutex> lock(g_host.mutex);
	g_host.started = false;
	g_host.pending.clear();
	if (g_asset_ref != nullptr) {
		env->DeleteGlobalRef(g_asset_ref);
		g_asset_ref = nullptr;
		g_assets = nullptr;
	}
	if (g_activity != nullptr) {
		env->DeleteGlobalRef(g_activity);
		g_activity = nullptr;
	}
}
