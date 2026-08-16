#pragma once
#include "renderer.h"
#include <core/display.h>

namespace CE::RenderAPIs {
    struct OpenGLRenderer : iRenderer {
        ~OpenGLRenderer() override = default;
        void initialize_libraries() override;
        void initialize_rendering_context() override;
        void deinitialize() override;
        void clear() override;
        void swap_buffer() override;
        void draw() override;
        program_id compile_shader(fs::path file) override;
        void initialize_glfw();
        void initialize_glad();
    private:
        std::once_flag glfw_flag;
        std::once_flag glad_flag;
        bool lib_init = false;
    };
}
