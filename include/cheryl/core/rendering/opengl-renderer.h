#pragma once
#include "renderer.h"

namespace CE::RenderAPIs {
    struct OpenGLRenderer : iRenderer {
        // todo: needs abstraction
        DisplaySystem display;
        void initialize_libraries() override;
        void initialize_rendering_context() override;
        void clear() override;
        void swap_buffer() override;
        void draw() override;
        program_id compile_shader(fs::path file) override;
    };
}
