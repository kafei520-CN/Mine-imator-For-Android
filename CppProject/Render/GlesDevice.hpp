#pragma once

#include <string>

#ifndef API_OPENGLES
#define API_OPENGLES 0
#endif

#if API_OPENGLES

#include <GLES3/gl3.h>

namespace CppProject
{
	// OpenGL ES device used by the Android host. Desktop GL stays on
	// QOpenGLFunctions_3_1. This owns programs, textures, and the offscreen
	// framebuffer the editor will later bind as a Surface.
	struct GlesDevice
	{
		GLuint Link(const std::string& vertex_source, const std::string& fragment_source);
		GLuint CreateRgbaTexture(int width, int height, const void* pixels);
		bool EnsureOffscreen(int width, int height);
		void BindOffscreen();
		void BlitToScreen(int width, int height);
		GLuint OffscreenColor() const { return color_; }

	private:
		void DestroyOffscreen();
		void EnsureBlit();

		GLuint fbo_ = 0;
		GLuint color_ = 0;
		GLuint depth_ = 0;
		GLuint blit_program_ = 0;
		GLuint blit_vbo_ = 0;
		int width_ = 0;
		int height_ = 0;
	};
}

#endif
