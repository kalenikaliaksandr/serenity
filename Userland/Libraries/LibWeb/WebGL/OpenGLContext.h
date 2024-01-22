/*
 * Copyright (c) 2024, Aliaksandr Kalenik <kalenik.aliaksandr@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Weakable.h>
#include <LibGfx/Bitmap.h>
#include <LibWeb/WebGL/Types.h>

namespace Web::WebGL {

class OpenGLContext : public Weakable<OpenGLContext> {
public:
    static OwnPtr<OpenGLContext> create(Gfx::Bitmap&);

    virtual void present(Gfx::Bitmap&) = 0;
    void clear_buffer_to_default_values();

    virtual GLenum gl_get_error() = 0;
    virtual void gl_get_doublev(GLenum, GLdouble*) = 0;
    virtual void gl_get_integerv(GLenum, GLint*) = 0;
    virtual void gl_clear(GLbitfield) = 0;
    virtual void gl_clear_color(GLfloat, GLfloat, GLfloat, GLfloat) = 0;
    virtual void gl_clear_depth(GLdouble) = 0;
    virtual void gl_clear_stencil(GLint) = 0;
    virtual void gl_active_texture(GLenum) = 0;
    virtual void gl_viewport(GLint, GLint, GLsizei, GLsizei) = 0;
    virtual void gl_line_width(GLfloat) = 0;
    virtual void gl_polygon_offset(GLfloat, GLfloat) = 0;
    virtual void gl_scissor(GLint, GLint, GLsizei, GLsizei) = 0;
    virtual void gl_depth_mask(GLboolean) = 0;
    virtual void gl_depth_func(GLenum) = 0;
    virtual void gl_depth_range(GLdouble, GLdouble) = 0;
    virtual void gl_cull_face(GLenum) = 0;
    virtual void gl_color_mask(GLboolean, GLboolean, GLboolean, GLboolean) = 0;
    virtual void gl_front_face(GLenum) = 0;
    virtual void gl_finish() = 0;
    virtual void gl_flush() = 0;
    virtual void gl_stencil_op_separate(GLenum, GLenum, GLenum, GLenum) = 0;
    virtual GLuint gl_create_shader(GLenum shader_type) = 0;
    virtual void gl_shader_source(GLuint shader, GLsizei count, GLchar const** string, GLint const* length) = 0;
    virtual void gl_compile_shader(GLuint shader) = 0;
    virtual GLuint gl_create_program() = 0;
    virtual void gl_attach_shader(GLuint program, GLuint shader) = 0;
    virtual void gl_link_program(GLuint program) = 0;
    virtual void gl_use_program(GLuint program) = 0;
    virtual GLuint gl_get_attrib_location(GLuint program, GLchar const* name) = 0;
    virtual void gl_vertex_attrib_3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) = 0;
    virtual void gl_draw_arrays(GLenum mode, GLint first, GLsizei count) = 0;

    virtual ~OpenGLContext() { }
};

}
