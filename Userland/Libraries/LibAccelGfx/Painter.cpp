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
#include <LibGfx/Painter.h>
#include <LibGfx/ImageFormats/PNGWriter.h>
#include <LibCore/File.h>

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

void Painter::prepare_glyph_atlas(HashMap<NonnullRefPtr<Gfx::Font>, HashTable<u32>> unique_glyphs_per_font)
{
    HashMap<AtlasKey, NonnullRefPtr<Gfx::Bitmap>> glyph_bitmaps;
    for (auto const& [font, code_points] : unique_glyphs_per_font) {
        StringBuilder builder;
        for (auto const& code_point : code_points) {
            builder.append_code_point(code_point);

            auto glyph = font->glyph(code_point);
            if (!glyph.bitmap()) {
                continue;
            }
            auto atlas_key = AtlasKey { font, code_point };
            glyph_bitmaps.set(atlas_key, *glyph.bitmap());
        }
    }

    int atlas_height = 0;
    int atlas_width = 0;
    for (auto const& [atlas_key, bitmap] : glyph_bitmaps) {
        atlas_width += bitmap->width();
        atlas_height = max(atlas_height, bitmap->height());
    }

    if (atlas_width == 0 || atlas_height == 0)
        return;

    auto atlas_bitmap = Gfx::Bitmap::create(Gfx::BitmapFormat::BGRA8888, { atlas_width, atlas_height }).release_value_but_fixme_should_propagate_errors();
    auto atlas_painter = Gfx::Painter(*atlas_bitmap);
    auto current_width = 0;
    for (auto const& [atlas_key, bitmap] : glyph_bitmaps) {
        atlas_painter.blit({ current_width, 0 }, bitmap, bitmap->rect());
        m_glyph_atlas_rects.set(atlas_key, { current_width, 0, bitmap->width(), bitmap->height() });
        current_width += bitmap->width();
    }

    auto encoded = MUST(Gfx::PNGWriter::encode(atlas_bitmap));

    auto screenshot_file = MUST(Core::File::open("/home/alex/atlas.png"sv, Core::File::OpenMode::Write));
    MUST(screenshot_file->write_until_depleted(encoded));

    m_glyph_atlas = atlas_bitmap;

    return;
}

static bool should_paint_as_space(u32 code_point)
{
    return is_ascii_space(code_point) || code_point == 0xa0;
}

void Painter::draw_text_run(Gfx::FloatPoint baseline_start, Utf8View const& string, Gfx::Font const& font, Color color)
{
    float space_width = font.glyph_width(' ') + font.glyph_spacing();

    u32 last_code_point = 0;

    auto point = baseline_start;
    point.translate_by(0, -font.pixel_metrics().ascent);

    Vector<GLfloat> vertices;

    for (auto code_point_iterator = string.begin(); code_point_iterator != string.end(); ++code_point_iterator) {
        auto code_point = *code_point_iterator;
        if (should_paint_as_space(code_point)) {
            point.translate_by(space_width, 0);
            last_code_point = code_point;
            continue;
        }

        auto kerning = font.glyphs_horizontal_kerning(last_code_point, code_point);
        if (kerning != 0.0f)
            point.translate_by(kerning, 0);

        auto it = code_point_iterator; // The callback function will advance the iterator, so create a copy for this lookup.
        auto glyph_width = font.glyph_or_emoji_width(it) + font.glyph_spacing();

        auto glyph = font.glyph(code_point);
        if (glyph.bitmap()) {
            auto maybe_texture_rect = m_glyph_atlas_rects.get(AtlasKey { font, code_point });
            if (!maybe_texture_rect.has_value()) {
                dbgln("Failed to find texture rect for code point {}", code_point);
                continue;
            }

            VERIFY(m_glyph_atlas);

            auto texture_rect = to_texture_space(maybe_texture_rect.value().to_type<float>(), m_glyph_atlas->size());

            auto rect = Gfx::FloatRect { point + Gfx::FloatPoint(font.glyph_left_bearing(code_point), 0), glyph.bitmap()->rect().size().to_type<float>() };
            auto rect_in_clip_space = to_clip_space(rect);

            // p0 --- p1
            // | \     |
            // |   \   |
            // |     \ |
            // p2 --- p3

            auto p0 = rect_in_clip_space.top_left();
            auto p1 = rect_in_clip_space.top_right();
            auto p2 = rect_in_clip_space.bottom_left();
            auto p3 = rect_in_clip_space.bottom_right();

            auto s0 = texture_rect.top_left();
            auto s1 = texture_rect.top_right();
            auto s2 = texture_rect.bottom_left();
            auto s3 = texture_rect.bottom_right();

            vertices.append(p0.x());
            vertices.append(p0.y());
            vertices.append(s0.x());
            vertices.append(s0.y());
            vertices.append(p1.x());
            vertices.append(p1.y());
            vertices.append(s1.x());
            vertices.append(s1.y());
            vertices.append(p3.x());
            vertices.append(p3.y());
            vertices.append(s3.x());
            vertices.append(s3.y());


            vertices.append(p0.x());
            vertices.append(p0.y());
            vertices.append(s0.x());
            vertices.append(s0.y());
            vertices.append(p3.x());
            vertices.append(p3.y());
            vertices.append(s3.x());
            vertices.append(s3.y());
            vertices.append(p2.x());
            vertices.append(p2.y());
            vertices.append(s2.x());
            vertices.append(s2.y());
        }

        point.translate_by(glyph_width, 0);
        last_code_point = code_point;
    }

    char const* vertex_shader_source = R"(
    attribute vec4 position;
    varying vec2 coord;
    void main() {
        coord = position.zw;
        gl_Position = vec4(position.xy, 0.0, 1.0);
    }
)";

    char const* fragment_shader_source = R"(
    precision mediump float;
    uniform vec4 uColor;
    varying vec2 coord;
    uniform sampler2D uSampler;
    void main() {
        gl_FragColor = texture2D(uSampler, coord) * uColor;
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

    GLuint texture;

    glGenTextures(1, &texture);
    VERIFY(glGetError() == GL_NO_ERROR);
    glBindTexture(GL_TEXTURE_2D, texture);
    VERIFY(glGetError() == GL_NO_ERROR);
    VERIFY(m_glyph_atlas);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_glyph_atlas->width(), m_glyph_atlas->height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, m_glyph_atlas->scanline(0));
    VERIFY(glGetError() == GL_NO_ERROR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    GLuint position_attribute = glGetAttribLocation(program, "position");
    GLuint color_uniform = glGetUniformLocation(program, "uColor");

    glUniform4f(color_uniform, red, green, blue, alpha);

    glVertexAttribPointer(position_attribute, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vertices.data());
    glEnableVertexAttribArray(position_attribute);

    glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 4);

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
