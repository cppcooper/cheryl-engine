#pragma once
#include "renderer.h"
#include <core/resources/opengl-resource-provider.h>
#include <mutex>
#include <vector>

namespace CE {
    class Window;
}

namespace CE::RenderAPIs {
    struct OpenGLRenderer : iRenderer {
        OpenGLRenderer();
        ~OpenGLRenderer() override = default;
        void initialize_libraries() override;
        void initialize_rendering_context() override;
        void deinitialize() override;
        void clear() override;
        void set_viewport(FramebufferSize size) override;
        void swap_buffer() override;
        [[nodiscard]] Assets::ResourceProvider& resources() override { return resource_provider_; }
        void draw() override;
        program_id compile_shader(fs::path file) override;
        program_id compile_program(const std::vector<fs::path>& stages);
        void initialize_glfw();
        void initialize_glad();
    private:
        Assets::OpenGLResourceProvider resource_provider_;
        Window* render_window_ = nullptr; // Owned by display.
        std::once_flag glfw_flag;
        std::once_flag glad_flag;
        bool lib_init = false;
    };
}
