/*
 * Copyright (c) 2023, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define GL_GLEXT_PROTOTYPES

#include "Painter.h"
#include "Canvas.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <LibGfx/Color.h>

namespace AccelGfx {

struct ColorComponents {
    float red;
    float green;
    float blue;
    float alpha;
};

static ColorComponents gfx_color_to_opengl_color(Gfx::Color color)
{
    ColorComponents components;
    components.red = static_cast<float>(color.red()) / 255.0f;
    components.green = static_cast<float>(color.green()) / 255.0f;
    components.blue = static_cast<float>(color.blue()) / 255.0f;
    components.alpha = static_cast<float>(color.alpha()) / 255.0f;
    return components;
}

Gfx::FloatRect Painter::to_clip_space(Gfx::FloatRect const& screen_rect) const
{
    float x = 2.0f * screen_rect.x() / m_canvas.width() - 1.0f;
    float y = -1.0f + 2.0f * screen_rect.y() / m_canvas.height();

    float width = 2.0f * screen_rect.width() / m_canvas.width();
    float height = 2.0f * screen_rect.height() / m_canvas.height();

    return { x, y, width, height };
}

Painter::Painter(Canvas& canvas)
    : m_canvas(canvas)
{
    m_state_stack.empend(State());
}

Painter::~Painter()
{
    flush();
}

void Painter::clear(Gfx::Color color)
{
    auto [red, green, blue, alpha] = gfx_color_to_opengl_color(color);
    glClearColor(red, green, blue, alpha);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Painter::fill_rect(Gfx::IntRect rect, Gfx::Color color)
{
    fill_rect(rect.to_type<float>(), color);
}

static GLuint create_shader(GLenum type, char const* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char buffer[512];
        glGetShaderInfoLog(shader, sizeof(buffer), nullptr, buffer);
        dbgln("GLSL shader compilation failed: {}", buffer);
        VERIFY_NOT_REACHED();
    }

    return shader;
}

static Array<GLfloat, 8> rect_to_vertices(Gfx::FloatRect const& rect)
{
    return {
        rect.left(),
        rect.top(),
        rect.left(),
        rect.bottom(),
        rect.right(),
        rect.bottom(),
        rect.right(),
        rect.top(),
    };
}

void Painter::fill_rect(Gfx::FloatRect rect, Gfx::Color color)
{
    // Draw a filled rect (with `color`) using OpenGL after mapping it through the current transform.

    auto vertices = rect_to_vertices(to_clip_space(transform().map(rect)));

    char const* vertex_shader_source = R"(
    attribute vec2 position;
    void main() {
        gl_Position = vec4(position, 0.0, 1.0);
    }
)";

    char const* fragment_shader_source = R"(
    precision mediump float;
    uniform vec4 uColor;
    void main() {
        gl_FragColor = uColor;
    }
)";

    auto [red, green, blue, alpha] = gfx_color_to_opengl_color(color);

    GLuint vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);

    GLuint program = glCreateProgram();

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    int linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char buffer[512];
        glGetProgramInfoLog(program, sizeof(buffer), nullptr, buffer);
        dbgln("GLSL program linking failed: {}", buffer);
        VERIFY_NOT_REACHED();
    }

    glUseProgram(program);

    GLuint position_attribute = glGetAttribLocation(program, "position");
    GLuint color_uniform = glGetUniformLocation(program, "uColor");

    glUniform4f(color_uniform, red, green, blue, alpha);

    glVertexAttribPointer(position_attribute, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), vertices.data());
    glEnableVertexAttribArray(position_attribute);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    glDeleteProgram(program);
}

static Gfx::FloatRect to_texture_space(Gfx::FloatRect rect, Gfx::IntSize image_size) {
    auto x = rect.x() / image_size.width();
    auto y = rect.y() / image_size.height();
    auto width = rect.width() / image_size.width();
    auto height = rect.height() / image_size.height();

    return { x, y, width, height };
}

void Painter::draw_scaled_bitmap(Gfx::IntRect const& dst_rect, Gfx::Bitmap const& bitmap, Gfx::IntRect const& src_rect)
{
    char const* vertex_shader_source = R"(
    attribute vec4 aVertexPosition;
    attribute vec2 aTextureCoord;
    varying vec2 vTextureCoord;

    void main() {
        gl_Position = aVertexPosition;
        vTextureCoord = aTextureCoord;
    }
)";

    char const* fragment_shader_source = R"(
    precision mediump float;

    varying vec2 vTextureCoord;
    uniform sampler2D uSampler;

    void main() {
        gl_FragColor = texture2D(uSampler, vTextureCoord);
    }
)";

    GLuint vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);

    GLuint program = glCreateProgram();

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    int linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char buffer[512];
        glGetProgramInfoLog(program, sizeof(buffer), nullptr, buffer);
        dbgln("GLSL program linking failed: {}", buffer);
        VERIFY_NOT_REACHED();
    }

    glUseProgram(program);

    GLuint texture;

    glGenTextures(1, &texture);
    VERIFY(glGetError() == GL_NO_ERROR);
    glBindTexture(GL_TEXTURE_2D, texture);
    VERIFY(glGetError() == GL_NO_ERROR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.width(), bitmap.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, bitmap.scanline(0));
    VERIFY(glGetError() == GL_NO_ERROR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    VERIFY(glGetError() == GL_NO_ERROR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    VERIFY(glGetError() == GL_NO_ERROR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    VERIFY(glGetError() == GL_NO_ERROR);

    auto vertices = rect_to_vertices(to_clip_space(transform().map(dst_rect.to_type<float>())));
    auto texture_coordinates = rect_to_vertices(to_texture_space(src_rect.to_type<float>(), bitmap.size()));

    GLuint vertex_position_attribute = glGetAttribLocation(program, "aVertexPosition");
    glVertexAttribPointer(vertex_position_attribute, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), vertices.data());
    glEnableVertexAttribArray(vertex_position_attribute);

    GLuint texture_coord_attribute = glGetAttribLocation(program, "aTextureCoord");
    glVertexAttribPointer(texture_coord_attribute, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), texture_coordinates.data());
    glEnableVertexAttribArray(texture_coord_attribute);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    VERIFY(glGetError() == GL_NO_ERROR);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    glDeleteProgram(program);
}

void Painter::save()
{
    m_state_stack.append(state());
}

void Painter::restore()
{
    VERIFY(!m_state_stack.is_empty());
    m_state_stack.take_last();
}

void Painter::flush()
{
    m_canvas.flush();
}

}
