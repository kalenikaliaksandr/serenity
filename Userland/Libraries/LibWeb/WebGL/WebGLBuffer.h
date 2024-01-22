#pragma once

#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/Forward.h>
#include <LibWeb/WebGL/WebGLObject.h>

namespace Web::WebGL {

class WebGLBuffer : public WebGLObject {
    WEB_PLATFORM_OBJECT(WebGLBuffer, WebGLObject);

public:
    WebGLBuffer(JS::Realm& realm, WeakPtr<OpenGLContext> context)
        : WebGLObject(realm, move(context)) {};
};

}
