#include "GlesDevice.hpp"

#if API_OPENGLES

#include <android/log.h>

namespace {

constexpr char kLogTag[] = "MineImator";

GLuint Compile(GLenum type, const char* source) {
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
	__android_log_print(ANDROID_LOG_ERROR, kLogTag, "gles shader: %s", log);
	glDeleteShader(shader);
	return 0;
}

}  // namespace

namespace CppProject {

GLuint GlesDevice::Link(const std::string& vertex_source, const std::string& fragment_source) {
	const GLuint vertex = Compile(GL_VERTEX_SHADER, vertex_source.c_str());
	const GLuint fragment = Compile(GL_FRAGMENT_SHADER, fragment_source.c_str());
	if (vertex == 0 || fragment == 0) {
		if (vertex != 0) {
			glDeleteShader(vertex);
		}
		if (fragment != 0) {
			glDeleteShader(fragment);
		}
		return 0;
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
		char log[1024];
		glGetProgramInfoLog(program, sizeof(log), nullptr, log);
		__android_log_print(ANDROID_LOG_ERROR, kLogTag, "gles link: %s", log);
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

GLuint GlesDevice::CreateRgbaTexture(int width, int height, const void* pixels) {
	GLuint texture = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	return texture;
}

void GlesDevice::DestroyOffscreen() {
	if (fbo_ != 0) {
		glDeleteFramebuffers(1, &fbo_);
		fbo_ = 0;
	}
	if (color_ != 0) {
		glDeleteTextures(1, &color_);
		color_ = 0;
	}
	if (depth_ != 0) {
		glDeleteRenderbuffers(1, &depth_);
		depth_ = 0;
	}
	width_ = 0;
	height_ = 0;
}

bool GlesDevice::EnsureOffscreen(int width, int height) {
	if (width <= 0 || height <= 0) {
		return false;
	}
	if (fbo_ != 0 && width_ == width && height_ == height) {
		return true;
	}
	DestroyOffscreen();
	glGenFramebuffers(1, &fbo_);
	glGenTextures(1, &color_);
	glBindTexture(GL_TEXTURE_2D, color_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenRenderbuffers(1, &depth_);
	glBindRenderbuffer(GL_RENDERBUFFER, depth_);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth_);
	const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		__android_log_print(ANDROID_LOG_ERROR, kLogTag, "gles fbo status 0x%x", status);
		DestroyOffscreen();
		return false;
	}
	width_ = width;
	height_ = height;
	__android_log_print(ANDROID_LOG_INFO, kLogTag, "gles fbo %dx%d", width, height);
	return true;
}

void GlesDevice::BindOffscreen() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
}

void GlesDevice::EnsureBlit() {
	if (blit_program_ != 0) {
		return;
	}
	const char* vertex =
		"#version 300 es\n"
		"layout(location = 0) in vec2 a_position;\n"
		"layout(location = 1) in vec2 a_uv;\n"
		"out vec2 v_uv;\n"
		"void main() {\n"
		"\tv_uv = a_uv;\n"
		"\tgl_Position = vec4(a_position, 0.0, 1.0);\n"
		"}\n";
	const char* fragment =
		"#version 300 es\n"
		"precision mediump float;\n"
		"in vec2 v_uv;\n"
		"uniform sampler2D u_tex;\n"
		"out vec4 frag_color;\n"
		"void main() {\n"
		"\tfrag_color = texture(u_tex, v_uv);\n"
		"}\n";
	blit_program_ = Link(vertex, fragment);
	const float vertices[] = {
		-1.f, -1.f, 0.f, 0.f,
		1.f, -1.f, 1.f, 0.f,
		-1.f, 1.f, 0.f, 1.f,
		1.f, 1.f, 1.f, 1.f,
	};
	glGenBuffers(1, &blit_vbo_);
	glBindBuffer(GL_ARRAY_BUFFER, blit_vbo_);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GlesDevice::BlitToScreen(int width, int height) {
	EnsureBlit();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, width, height);
	if (blit_program_ == 0) {
		return;
	}
	for (int attrib = 0; attrib < 8; ++attrib) {
		glDisableVertexAttribArray(attrib);
	}
	glUseProgram(blit_program_);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, color_);
	glUniform1i(glGetUniformLocation(blit_program_, "u_tex"), 0);
	glBindBuffer(GL_ARRAY_BUFFER, blit_vbo_);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
		reinterpret_cast<void*>(sizeof(float) * 2));
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

}  // namespace CppProject

#endif
