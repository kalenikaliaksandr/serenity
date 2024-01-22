#pragma once

#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/Forward.h>
#include <LibWeb/WebGL/Types.h>
#include <LibWeb/WebGL/WebGLObject.h>

namespace Web::WebGL {

class WebGLShader : public WebGLObject {
    WEB_PLATFORM_OBJECT(WebGLShader, WebGLObject);

public:
    WebGLShader(JS::Realm& realm, WeakPtr<OpenGLContext> context, GLenum type, GLuint gl_shader)
        : WebGLObject(realm, move(context))
        , m_type(type)
        , m_gl_shader(gl_shader) {};

    void set_source(String const& source)
    {
        m_source = source;
        GLchar const* source_ptr = reinterpret_cast<GLchar const*>(source.bytes().data());
        GLint const source_length = source.bytes_as_string_view().length();
        dbgln(">>>>>WebGLShader::set_source: ({}) length=({})", source, source_length);
        context()->gl_shader_source(m_gl_shader, 1, &source_ptr, &source_length);
    }

    void compile() const
    {
        context()->gl_compile_shader(m_gl_shader);
    }

    bool is_fragment_shader() const
    {
        return m_type == FRAGMENT_SHADER;
    }

    bool is_vertex_shader() const
    {
        return m_type == VERTEX_SHADER;
    }

    GLuint gl_shader() const
    {
        return m_gl_shader;
    }

private:
    GLenum m_type { 0 };
    GLuint m_gl_shader;
    Optional<String> m_source;
};

}
