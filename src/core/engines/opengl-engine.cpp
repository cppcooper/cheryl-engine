#include <core/engines/opengl-engine.h>

#include <cgl.h>
#include <core/controls/input-system.h>
#include <core/rendering/opengl-renderer.h>
#include <core/resources/asset-management/shader-mgr.h>
#include <core/subsystems/event-system.h>
#include <internals/exceptions.h>
#include <templates/singleton.h>

#include <utility>

namespace CE::Engine {
    glEngine::glEngine() :
        camera_2d_(std::make_shared<Camera2D>()), camera_3d_(std::make_shared<Camera3D>()), active_camera_(camera_2d_) {
        renderer = &Singleton_CTS<RenderAPIs::OpenGLRenderer>::get();
    }

    void glEngine::synchronize_camera() {
        auto* window = renderer->display ? renderer->display->active_window() : nullptr;
        if (!window)
            throw Exceptions::failed_operation(CE_HERE, "No active window is available for the camera");

        const auto size = window->framebuffer_size();
        if (viewport_size_ != size) {
            renderer->set_viewport(size);
            viewport_size_ = size;
        }
        active_camera_->set_framebuffer_size(size);
        if (published_camera_ == active_camera_ && published_revision_ == active_camera_->revision())
            return;

        if (active_camera_->mode() == Enum::gfx_mode::R3D)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);

        const auto& projection = active_camera_->projection_matrix();
        const bool projection_changed = !published_camera_ || published_projection_ != projection;
        Assets::ShaderMgr::get().set_camera_matrices(projection, active_camera_->view_matrix());
        if (projection_changed)
            SubSystems::EventSystem::get().dispatch("projection-matrix-changed", projection);

        published_projection_ = projection;
        published_revision_ = active_camera_->revision();
        published_camera_ = active_camera_;
    }

    void glEngine::set_camera(std::shared_ptr<CameraBase> camera) {
        if (!camera)
            throw Exceptions::invalid_args(CE_HERE, "Cannot activate a null camera");
        active_camera_ = std::move(camera);
        if (initialized_)
            synchronize_camera();
    }

    void glEngine::init() {
        if (initialized_)
            return;
        renderer->initialize_libraries();
        renderer->initialize_rendering_context();
        synchronize_camera();
        input().initialize(*renderer->display->active_window());
        initialized_ = true;
    }

    void glEngine::deinit() {
        if (initialized_)
            input().deinitialize();
        initialized_ = false;
        published_camera_.reset();
        viewport_size_ = {-1, -1};
    }

    Input::iInputSystem& glEngine::input() {
        return Input::InputSystem::get();
    }

    Assets::ResourceProvider& glEngine::resources() {
        return renderer->resources();
    }

    void glEngine::poll_input() {
        if (!initialized_)
            throw Exceptions::failed_operation(CE_HERE, "Input cannot be polled before engine initialization");
        input().poll();
    }

    void glEngine::pre_draw() {
        synchronize_camera();
        renderer->clear();
    }

    void glEngine::post_draw() {
        renderer->swap_buffer();
    }

    bool glEngine::should_close() const {
        auto* window = renderer->display ? renderer->display->active_window() : nullptr;
        return !window || window->should_close();
    }

    void glEngine::set_mode(const Enum::gfx_mode mode) {
        switch (mode) {
        case Enum::gfx_mode::R2D:
            set_camera(camera_2d_);
            break;
        case Enum::gfx_mode::R3D:
            set_camera(camera_3d_);
            break;
        default:
            throw Exceptions::invalid_args(CE_HERE, "Unknown graphics mode");
        }
    }

    void glEngine::set_mode(const Enum::window_mode mode) {
        auto* window = renderer->display ? renderer->display->active_window() : nullptr;
        if (!window)
            throw Exceptions::failed_operation(CE_HERE, "Cannot change window mode before display initialization");
        window->set_mode(mode);
        synchronize_camera();
    }

    void glEngine::set_clear_colour(const float r, const float g, const float b, const float a) {
        glClearColor(r, g, b, a);
    }

    void glEngine::hide_cursor(const bool hide) {
        auto* window = renderer->display ? renderer->display->active_window() : nullptr;
        if (!window)
            throw Exceptions::failed_operation(CE_HERE, "Cannot set cursor mode before display initialization");
        window->hide_cursor(hide);
    }
}
