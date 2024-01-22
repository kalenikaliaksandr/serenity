#pragma once

#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/Forward.h>
#include <LibWeb/WebGL/WebGLObject.h>
#include <LibWeb/WebGL/WebGLShader.h>

namespace Web::WebGL {

class WebGLProgram : public WebGLObject {
    WEB_PLATFORM_OBJECT(WebGLProgram, WebGLObject);

public:
    WebGLProgram(JS::Realm& realm, WeakPtr<OpenGLContext> context, GLuint program)
        : WebGLObject(realm, move(context))
        , m_gl_program(program) {};

    void attach_shader(JS::NonnullGCPtr<WebGLShader> shader)
    {
        if (shader->is_vertex_shader())
            m_vertex_shader = shader;
        else if (shader->is_fragment_shader())
            m_fragment_shader = shader;

        context()->gl_attach_shader(m_gl_program, shader->gl_shader());
    }

    GLint get_attrib_location(String const& name) const
    {
        return context()->gl_get_attrib_location(m_gl_program, reinterpret_cast<GLchar const*>(name.bytes().data()));
    }

    void link() const
    {
        context()->gl_link_program(m_gl_program);
    }

    void use() const
    {
        context()->gl_use_program(m_gl_program);
    }

    void visit_edges(Visitor& visitor) override
    {
        Base::visit_edges(visitor);
        visitor.visit(m_vertex_shader);
        visitor.visit(m_fragment_shader);
    }

private:
    GLuint m_gl_program;
    JS::GCPtr<WebGLShader> m_vertex_shader;
    JS::GCPtr<WebGLShader> m_fragment_shader;
};

}
