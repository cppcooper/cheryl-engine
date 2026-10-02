#pragma once

#include <backends/opengl/renderer.h>

#include <functional>

namespace CE::RenderAPIs::RendererDetail {
    struct RendererAccess {
        // Configure before initial startup. Recording fixtures already install
        // native entries; production loads and validates GLAD directly.
        static void set_native_loader(
            OpenGLRenderer& renderer,
            std::function<void(iOpenGLContext&)> loader
        );
    };
}
