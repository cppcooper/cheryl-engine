#pragma once

#include <backends/opengl/renderer.h>

#include <functional>

namespace CE::Rendering::OpenGL::Detail {
    struct RendererAccess {
        // Configure before initial startup. Recording fixtures already install
        // native entries; production loads and validates GLAD directly.
        static void
        set_native_loader(::CE::Rendering::OpenGL::Renderer& renderer, std::function<void(::CE::Rendering::OpenGL::Context&)> loader);
    };
}
