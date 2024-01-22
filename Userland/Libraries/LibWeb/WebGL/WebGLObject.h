#pragma once

#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/Forward.h>
#include <LibWeb/WebGL/OpenGLContext.h>

namespace Web::WebGL {

class WebGLObject : public Bindings::PlatformObject {
    WEB_PLATFORM_OBJECT(WebGLBuffer, Bindings::PlatformObject);

public:
    WebGLObject(JS::Realm& realm, WeakPtr<OpenGLContext> context)
        : PlatformObject(realm)
        , m_context(move(context)) {};

    OpenGLContext* context() const { return m_context.ptr(); }

private:
    WeakPtr<OpenGLContext> m_context;
};

}
