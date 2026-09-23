#pragma once
#include "renderer.h"
#include <core/resources/opengl-resource-provider.h>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <vector>

namespace CE {
    class Window;
}

namespace CE::RenderAPIs {
    namespace fs = std::filesystem;
    using program_id = std::uint64_t;

    struct OpenGLRenderer : iRenderer {
        OpenGLRenderer();
        ~OpenGLRenderer() override = default;
        void initialize_libraries() override;
        void initialize_rendering_context() override;
        void deinitialize() override;
        void clear() override;
        void set_viewport(FramebufferSize size) override;
        void set_depth_test(bool enabled) override;
        void set_clear_colour(float r, float g, float b, float a) override;
        void set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) override;
        void swap_buffer() override;
        [[nodiscard]] Assets::ResourceProvider& resources() override { return resource_provider_; }
        program_id compile_shader(fs::path file);
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
