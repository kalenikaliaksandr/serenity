/*
 * Copyright (c) 2023, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Canvas.h"
#include <EGL/egl.h>
#include <GL/gl.h>
#include <LibGfx/Bitmap.h>

namespace AccelGfx {

ErrorOr<Canvas> Canvas::create(Gfx::IntSize size)
{
    auto bitmap = TRY(Gfx::Bitmap::create(Gfx::BitmapFormat::BGRA8888, size));
    Canvas canvas { move(bitmap) };
    canvas.initialize();
    return canvas;
}

Canvas::Canvas(NonnullRefPtr<Gfx::Bitmap> bitmap)
    : m_bitmap(move(bitmap))
{
}

void Canvas::initialize()
{
    EGLDisplay egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    EGLint major;
    EGLint minor;
    eglInitialize(egl_display, &major, &minor);

    const GLubyte* glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);
    printf("GLSL Version: %s\n", glslVersion);

    static const EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };

    EGLint num_configs;
    EGLConfig egl_config;
    eglChooseConfig(egl_display, config_attributes, &egl_config, 1, &num_configs);

    const EGLint pbufferAttribs[] = {
        EGL_WIDTH,
        width(),
        EGL_HEIGHT,
        height(),
        EGL_NONE,
    };
    EGLSurface eglSurf = eglCreatePbufferSurface(egl_display, egl_config, pbufferAttribs);

    static const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLContext eglCtx = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, contextAttribs);

    eglMakeCurrent(egl_display, eglSurf, eglSurf, eglCtx);
}

void Canvas::flush()
{
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width(), height(), GL_BGRA, GL_UNSIGNED_BYTE, m_bitmap->scanline(0));
}

}
