/*
 * Copyright (c) 2024, Aliaksandr Kalenik <kalenik.aliaksandr@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/OwnPtr.h>
#include <LibGfx/Bitmap.h>
#include <LibWeb/WebGL/OpenGLContext.h>

#define GL_GLEXT_PROTOTYPES

#ifdef HAS_ACCELERATED_GRAPHICS
#    include <LibAccelGfx/Canvas.h>
#    include <LibAccelGfx/Context.h>
#elif defined(AK_OS_SERENITY)
#    include <LibGL/GLContext.h>
#endif

namespace Web::WebGL {

#ifdef HAS_ACCELERATED_GRAPHICS
class AccelGfxContext : public OpenGLContext {
public:
    void activate()
    {
        m_context->activate();
    }

    virtual void present(Gfx::Bitmap& bitmap) override
    {
        activate();
        dbgln(">VERSION: {}", (char*)glGetString(GL_VERSION));
        VERIFY(bitmap.format() == Gfx::BitmapFormat::BGRA8888);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, bitmap.width(), bitmap.height(), GL_BGRA, GL_UNSIGNED_BYTE, bitmap.scanline(0));
        VERIFY(glGetError() == GL_NO_ERROR);
    }

    virtual GLenum gl_get_error() override
    {
        activate();
        return glGetError();
    }

    virtual void gl_get_doublev(GLenum pname, GLdouble* params) override
    {
        activate();
        glGetDoublev(pname, params);
    }

    virtual void gl_get_integerv(GLenum pname, GLint* params) override
    {
        activate();
        glGetIntegerv(pname, params);
    }

    virtual void gl_clear(GLbitfield mask) override
    {
        activate();
        glClear(mask);
    }

    virtual void gl_clear_color(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) override
    {
        activate();
        glClearColor(red, green, blue, alpha);
    }

    virtual void gl_clear_depth(GLdouble depth) override
    {
        activate();
        glClearDepth(depth);
    }

    virtual void gl_clear_stencil(GLint s) override
    {
        activate();
        glClearStencil(s);
    }

    virtual void gl_active_texture(GLenum texture) override
    {
        activate();
        glActiveTexture(texture);
    }

    virtual void gl_viewport(GLint x, GLint y, GLsizei width, GLsizei height) override
    {
        activate();
        dbgln(">>>glViewport: x={}, y={}, width={}, height={}", x, y, width, height);
        glViewport(x, y, width, height);

        //        glEnable(GL_PROGRAM_POINT_SIZE);
        //        VERIFY(glGetError() == GL_NO_ERROR);
    }

    virtual void gl_line_width(GLfloat width) override
    {
        activate();
        glLineWidth(width);
    }

    virtual void gl_polygon_offset(GLfloat factor, GLfloat units) override
    {
        activate();
        glPolygonOffset(factor, units);
    }

    virtual void gl_scissor(GLint x, GLint y, GLsizei width, GLsizei height) override
    {
        activate();
        glScissor(x, y, width, height);
    }

    virtual void gl_depth_mask(GLboolean mask) override
    {
        activate();
        glDepthMask(mask);
    }

    virtual void gl_depth_func(GLenum func) override
    {
        activate();
        glDepthFunc(func);
    }

    virtual void gl_depth_range(GLdouble z_near, GLdouble z_far) override
    {
        activate();
        glDepthRange(z_near, z_far);
    }

    virtual void gl_cull_face(GLenum mode) override
    {
        activate();
        glCullFace(mode);
    }

    virtual void gl_color_mask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) override
    {
        activate();
        glColorMask(red, green, blue, alpha);
    }

    virtual void gl_front_face(GLenum mode) override
    {
        activate();
        glFrontFace(mode);
    }

    virtual void gl_finish() override
    {
        activate();
        glFinish();
    }

    virtual void gl_flush() override
    {
        activate();
        glFlush();
    }

    virtual void gl_stencil_op_separate(GLenum, GLenum, GLenum, GLenum) override
    {
        TODO();
    }

    virtual GLuint gl_create_shader(GLenum shader_type) override
    {
        activate();

        GLenum type;
        switch (shader_type) {
        case FRAGMENT_SHADER:
            dbgln(">>>fragment shader");
            type = GL_FRAGMENT_SHADER;
            break;
        case VERTEX_SHADER:
            dbgln(">>>vertex shader");
            type = GL_VERTEX_SHADER;
            break;
        default:
            // FIXME: That is wrong
            VERIFY_NOT_REACHED();
        }
        return glCreateShader(type);
    }

    virtual void gl_shader_source(GLuint shader, GLsizei count, GLchar const** string, GLint const* length) override
    {
        activate();
        dbgln(">>>shader source: ({})", string[0]);
        glShaderSource(shader, count, string, length);

        GLchar str[513];
        GLint len = 0;
        glGetShaderSource(shader, 512, &len, str);
        StringView view(str, len);
        dbgln("!!!!!!!!!!!!!!!!!!!!!11shader source: ({})", view);
        // FIXME:
        VERIFY(glGetError() == GL_NO_ERROR);
    }

    virtual void gl_compile_shader(GLuint shader) override
    {
        activate();
        glCompileShader(shader);
        int success = false;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char buffer[512];
            glGetShaderInfoLog(shader, sizeof(buffer), nullptr, buffer);
            dbgln("GLSL shader compilation failed: {}", buffer);
            VERIFY_NOT_REACHED();
        }
        VERIFY(glGetError() == GL_NO_ERROR);
        dbgln(">>>after glCompileShader");
    }

    virtual GLuint gl_create_program() override
    {
        activate();
        return glCreateProgram();
    }

    virtual void gl_attach_shader(GLuint program, GLuint shader) override
    {
        activate();
        glAttachShader(program, shader);
        VERIFY(glGetError() == GL_NO_ERROR);
        dbgln(">>>after glAttachShader program={}, shader={}", program, shader);
    }

    virtual void gl_link_program(GLuint program) override
    {
        activate();
        glLinkProgram(program);
        int linked;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            char buffer[512];
            glGetProgramInfoLog(program, sizeof(buffer), nullptr, buffer);
            dbgln("GLSL program linking failed: {}", buffer);
            VERIFY_NOT_REACHED();
        }
        VERIFY(glGetError() == GL_NO_ERROR);
        dbgln(">>>after glLinkProgram");
    }

    virtual void gl_use_program(GLuint program) override
    {
        activate();
        (void)program;
        dbgln(">>>use program={}", program);
        glUseProgram(program);
        VERIFY(glGetError() == GL_NO_ERROR);
        dbgln(">>>after glUseProgram");
    }

    virtual GLuint gl_get_attrib_location(GLuint program, GLchar const* name) override
    {
        activate();
        auto result = glGetAttribLocation(program, name);
        VERIFY(glGetError() == GL_NO_ERROR);
        dbgln(">>>after glGetAttribLocation program={}, name={}", program, name);
        return result;
    }

    virtual void gl_vertex_attrib_3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) override
    {
        activate();
        glVertexAttrib3f(0, 0, 0, 0);
        VERIFY(glGetError() == GL_NO_ERROR);
        dbgln(">>>after glVertexAttrib3f index={}, x={}, y={}, z={}", index, x, y, z);
    }

    virtual void gl_draw_arrays(GLenum mode, GLint first, GLsizei count) override
    {
        activate();
        (void)mode;
        (void)first;
        (void)count;
        VERIFY(glGetError() == GL_NO_ERROR);
        //        glViewport(0, 0, 100, 100);
        glEnableVertexAttribArray(0);
        glDisable(GL_SCISSOR_TEST);
        //        glEnable(GL_PROGRAM_POINT_SIZE);
        VERIFY(glGetError() == GL_NO_ERROR);

        GLint currentProgram = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
        dbgln(">>>glGetIntegerv(GL_CURRENT_PROGRAM)={}", currentProgram);
        //        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //        glDrawArrays(GL_POINTS, 0, 1);

        GLfloat vertices[] = {
            0.0f, 0.5f, 0.0f,   // Vertice #1
            -0.5f, -0.5f, 0.0f, // Vertice #2
            0.5f, -0.5f, 0.0f   // Vertice #3
        };

        GLuint VBO, VAO;

        // Generate and bind the Vertex Array Object
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        // Generate and bind the Vertex Buffer Object
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        // Copy vertex data to the VBO
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Define vertex attribute pointers
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
        glEnableVertexAttribArray(0);

        // Unbind the VBO (optional, but recommended)
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Unbind the VAO (optional, but recommended)
        glBindVertexArray(0);

        // When drawing
        // Bind the VAO
        glBindVertexArray(VAO);

        // Draw the vertices
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Unbind the VAO (optional)
        glBindVertexArray(0);

        VERIFY(glGetError() == GL_NO_ERROR);
        dbgln(">>>after glDrawArrays");
    }

    AccelGfxContext(OwnPtr<AccelGfx::Context> context, NonnullRefPtr<AccelGfx::Canvas> canvas)
        : m_context(move(context))
        , m_canvas(move(canvas))
    {
    }

    ~AccelGfxContext()
    {
        activate();
    }

private:
    OwnPtr<AccelGfx::Context> m_context;
    NonnullRefPtr<AccelGfx::Canvas> m_canvas;
};
#endif

#ifdef AK_OS_SERENITY
class LibGLContext : public OpenGLContext {
public:
    virtual void present(Gfx::Bitmap&) override
    {
        m_context->present();
    }

    virtual GLenum gl_get_error() override
    {
        return m_context->gl_get_error();
    }

    virtual void gl_get_doublev(GLenum pname, GLdouble* params) override
    {
        m_context->gl_get_doublev(pname, params);
    }

    virtual void gl_get_integerv(GLenum pname, GLint* params) override
    {
        m_context->gl_get_integerv(pname, params);
    }

    virtual void gl_clear(GLbitfield mask) override
    {
        m_context->gl_clear(mask);
    }

    virtual void gl_clear_color(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) override
    {
        m_context->gl_clear_color(red, green, blue, alpha);
    }

    virtual void gl_clear_depth(GLdouble depth) override
    {
        m_context->gl_clear_depth(depth);
    }

    virtual void gl_clear_stencil(GLint s) override
    {
        m_context->gl_clear_stencil(s);
    }

    virtual void gl_active_texture(GLenum texture) override
    {
        m_context->gl_active_texture(texture);
    }

    virtual void gl_viewport(GLint x, GLint y, GLsizei width, GLsizei height) override
    {
        m_context->gl_viewport(x, y, width, height);
    }

    virtual void gl_line_width(GLfloat width) override
    {
        m_context->gl_line_width(width);
    }

    virtual void gl_polygon_offset(GLfloat factor, GLfloat units) override
    {
        m_context->gl_polygon_offset(factor, units);
    }

    virtual void gl_scissor(GLint x, GLint y, GLsizei width, GLsizei height) override
    {
        m_context->gl_scissor(x, y, width, height);
    }

    virtual void gl_depth_mask(GLboolean flag) override
    {
        m_context->gl_depth_mask(flag);
    }

    virtual void gl_depth_func(GLenum func) override
    {
        m_context->gl_depth_func(func);
    }

    virtual void gl_depth_range(GLdouble z_near, GLdouble z_far) override
    {
        m_context->gl_depth_range(z_near, z_far);
    }

    virtual void gl_cull_face(GLenum mode) override
    {
        m_context->gl_cull_face(mode);
    }

    virtual void gl_color_mask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) override
    {
        m_context->gl_color_mask(red, green, blue, alpha);
    }

    virtual void gl_front_face(GLenum mode) override
    {
        m_context->gl_front_face(mode);
    }

    virtual void gl_finish() override
    {
        m_context->gl_finish();
    }

    virtual void gl_flush() override
    {
        m_context->gl_flush();
    }

    virtual void gl_stencil_op_separate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) override
    {
        m_context->gl_stencil_op_separate(face, sfail, dpfail, dppass);
    }

    LibGLContext(OwnPtr<GL::GLContext> context)
        : m_context(move(context))
    {
    }

private:
    OwnPtr<GL::GLContext> m_context;
};
#endif

#ifdef HAS_ACCELERATED_GRAPHICS
static OwnPtr<AccelGfxContext> make_accelgfx_context(Gfx::Bitmap& bitmap)
{
    auto context = AccelGfx::Context::create();
    auto canvas = AccelGfx::Canvas::create(bitmap.size());
    canvas->bind();
    return make<AccelGfxContext>(move(context), move(canvas));
}
#endif

#ifdef AK_OS_SERENITY
static OwnPtr<LibGLContext> make_libgl_context(Gfx::Bitmap& bitmap)
{
    auto context_or_error = GL::create_context(bitmap);
    return make<LibGLContext>(move(context_or_error.value()));
}
#endif

OwnPtr<OpenGLContext> OpenGLContext::create(Gfx::Bitmap& bitmap)
{
#ifdef HAS_ACCELERATED_GRAPHICS
    return make_accelgfx_context(bitmap);
#elif defined(AK_OS_SERENITY)
    return make_libgl_context(bitmap);
#endif

    (void)bitmap;
    return {};
}

void OpenGLContext::clear_buffer_to_default_values()
{
#if defined(HAS_ACCELERATED_GRAPHICS) || defined(AK_OS_SERENITY)
    Array<GLdouble, 4> current_clear_color;
    gl_get_doublev(GL_COLOR_CLEAR_VALUE, current_clear_color.data());

    GLdouble current_clear_depth;
    gl_get_doublev(GL_DEPTH_CLEAR_VALUE, &current_clear_depth);

    GLint current_clear_stencil;
    gl_get_integerv(GL_STENCIL_CLEAR_VALUE, &current_clear_stencil);

    // The implicit clear value for the color buffer is (0, 0, 0, 0)
    gl_clear_color(0, 0, 0, 0);

    // The implicit clear value for the depth buffer is 1.0.
    gl_clear_depth(1.0);

    // The implicit clear value for the stencil buffer is 0.
    gl_clear_stencil(0);

    gl_clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Restore the clear values.
    gl_clear_color(current_clear_color[0], current_clear_color[1], current_clear_color[2], current_clear_color[3]);
    gl_clear_depth(current_clear_depth);
    gl_clear_stencil(current_clear_stencil);
#endif
}

}
