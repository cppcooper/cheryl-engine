#pragma once

#include "abstract-engine.h"

#include <core/camera.h>
#include <glm.hpp>

#include <cstdint>
#include <memory>

namespace CE::Engine {
    struct glEngine final : iEngine {
    private:
        void synchronize_camera();

        std::shared_ptr<Camera2D> camera_2d_;
        std::shared_ptr<Camera3D> camera_3d_;
        std::shared_ptr<CameraBase> active_camera_;
        std::shared_ptr<CameraBase> published_camera_;
        std::uint64_t published_revision_ = 0;
        glm::mat4 published_projection_{0.0f};
        FramebufferSize viewport_size_{-1, -1};
        bool initialized_ = false;

    public:
        glEngine();
        ~glEngine() override = default;
        void set_camera(std::shared_ptr<CameraBase> camera);
        [[nodiscard]] std::shared_ptr<CameraBase> active_camera() const { return active_camera_; }
        void init() override;
        void deinit() override;
        [[nodiscard]] Input::iInputSystem& input() override;
        void poll_input() override;
        void pre_draw() override;
        void post_draw() override;
        [[nodiscard]] bool should_close() const override;
        void set_mode(Enum::gfx_mode) override;
        void set_mode(Enum::window_mode) override;
        void set_clear_colour(float, float, float, float) override;
        void hide_cursor(bool) override;
    };
}
