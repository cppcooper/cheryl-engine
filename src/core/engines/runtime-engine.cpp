#include <core/engines/runtime-engine.h>

#include <core/subsystems/event-system.h>
#include <internals/exceptions.h>

#include <utility>

namespace CE::Engine {
    RuntimeEngine::RuntimeEngine(RenderAPIs::iRenderer& selected_renderer,
                                 std::function<Input::iInputSystem&()> input_factory) :
        input_factory_(std::move(input_factory)), camera_2d_(std::make_shared<Camera2D>()),
        camera_3d_(std::make_shared<Camera3D>()), active_camera_(camera_2d_) {
        if (!input_factory_)
            throw Exceptions::invalid_args(CE_HERE, "An input adapter is required");
        renderer = &selected_renderer;
    }

    void RuntimeEngine::synchronize_camera() {
        // Use framebuffer pixels for projection/viewport; window logical dimensions are for input/UI.
        auto* window = renderer->display ? renderer->display->active_window() : nullptr;
        if (!window)
            throw Exceptions::failed_operation(CE_HERE, "No active window is available for the camera");

        const auto size = window->framebuffer_size();
        if (viewport_size_ != size) {
            renderer->set_viewport(size);
            viewport_size_ = size;
        }
        // Resize the active camera before comparing revisions: a new pixel
        // size may change its projection even when the view matrix is stable.
        active_camera_->set_framebuffer_size(size);
        // A stable camera revision means neither its view nor its projection needs republishing.
        if (published_camera_ == active_camera_ && published_revision_ == active_camera_->revision())
            return;

        renderer->set_depth_test(active_camera_->mode() == Enum::gfx_mode::R3D);

        const auto& projection = active_camera_->projection_matrix();
        // Notify projection consumers only when the projection actually changes; view changes still
        // update the renderer through set_camera_matrices below.
        const bool projection_changed = !published_camera_ || published_projection_ != projection;
        renderer->set_camera_matrices(projection, active_camera_->view_matrix());
        if (projection_changed)
            SubSystems::EventSystem::get().dispatch("projection-matrix-changed", projection);

        published_projection_ = projection;
        published_revision_ = active_camera_->revision();
        published_camera_ = active_camera_;
    }

    void RuntimeEngine::set_camera(std::shared_ptr<CameraBase> camera) {
        if (!camera)
            throw Exceptions::invalid_args(CE_HERE, "Cannot activate a null camera");
        active_camera_ = std::move(camera);
        if (initialized_)
            synchronize_camera();
    }

    void RuntimeEngine::init() {
        if (initialized_)
            return;
        // Establish the display and rendering context before publishing camera state or attaching input.
        renderer->initialize_libraries();
        renderer->initialize_rendering_context();
        synchronize_camera();
        input().initialize(*renderer->display->active_window());
        initialized_ = true;
    }

    void RuntimeEngine::deinit() {
        // Stop GLFW callbacks before forgetting published camera state;
        // the next init will resend viewport and camera matrices.
        if (initialized_)
            input().deinitialize();
        initialized_ = false;
        published_camera_.reset();
        viewport_size_ = {-1, -1};
    }

    Input::iInputSystem& RuntimeEngine::input() {
        if (!input_)
            input_ = &input_factory_();
        return *input_;
    }

    Assets::ResourceProvider& RuntimeEngine::resources() {
        return renderer->resources();
    }

    void RuntimeEngine::poll_input() {
        if (!initialized_)
            throw Exceptions::failed_operation(CE_HERE, "Input cannot be polled before engine initialization");
        input().poll();
    }

    void RuntimeEngine::pre_draw() {
        synchronize_camera();
        renderer->clear();
    }

    void RuntimeEngine::post_draw() {
        renderer->swap_buffer();
    }

    bool RuntimeEngine::should_close() const {
        auto* window = renderer->display ? renderer->display->active_window() : nullptr;
        return !window || window->should_close();
    }

    void RuntimeEngine::set_mode(const Enum::gfx_mode mode) {
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

    void RuntimeEngine::set_mode(const Enum::window_mode mode) {
        auto* window = renderer->display ? renderer->display->active_window() : nullptr;
        if (!window)
            throw Exceptions::failed_operation(CE_HERE, "Cannot change window mode before display initialization");
        window->set_mode(mode);
        synchronize_camera();
    }

    void RuntimeEngine::set_clear_colour(const float r, const float g, const float b, const float a) {
        renderer->set_clear_colour(r, g, b, a);
    }

    void RuntimeEngine::hide_cursor(const bool hide) {
        auto* window = renderer->display ? renderer->display->active_window() : nullptr;
        if (!window)
            throw Exceptions::failed_operation(CE_HERE, "Cannot set cursor mode before display initialization");
        window->hide_cursor(hide);
    }
}
