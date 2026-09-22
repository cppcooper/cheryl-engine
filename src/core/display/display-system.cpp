#include <core/display/display-system.h>

#include <internals/exceptions.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <atomic>
#include <memory>

namespace CE {
    namespace {
        std::atomic<std::uint64_t> next_monitor_id{1};
    }

    Monitor DisplaySystem::create_primary_monitor() {
        auto* handle = glfwGetPrimaryMonitor();
        const auto* video_mode = handle ? glfwGetVideoMode(handle) : nullptr;
        if (!video_mode)
            throw Exceptions::runtime_exception(CE_HERE, "No primary monitor or video mode is available");
        return {next_monitor_id++, video_mode->width, video_mode->height};
    }

    DisplaySystem::DisplaySystem() : primary_monitor_(create_primary_monitor()) {
        int count = 0;
        GLFWmonitor** handles = glfwGetMonitors(&count);
        if (!handles || count <= 0)
            throw Exceptions::runtime_exception(CE_HERE, "No monitors are available");

        monitors_.reserve(static_cast<std::size_t>(count));
        native_monitors_.reserve(static_cast<std::size_t>(count));
        monitors_.push_back(primary_monitor_);
        auto* primary_handle = glfwGetPrimaryMonitor();
        native_monitors_.push_back(primary_handle);
        for (int index = 0; index < count; ++index) {
            if (handles[index] == primary_handle)
                continue;
            const auto* video_mode = glfwGetVideoMode(handles[index]);
            if (video_mode) {
                monitors_.push_back(Monitor(next_monitor_id++, video_mode->width, video_mode->height));
                native_monitors_.push_back(handles[index]);
            }
        }
    }

    GLFWmonitor* DisplaySystem::native_monitor(const Monitor& monitor) const {
        for (std::size_t index = 0; index < monitors_.size(); ++index) {
            if (monitors_[index].id_ == monitor.id_)
                return native_monitors_[index];
        }
        throw Exceptions::invalid_args(CE_HERE, "Monitor is not owned by this display");
    }

    std::pair<float, float> DisplaySystem::content_scale(const Monitor& monitor) const {
        float x = 1.0f;
        float y = 1.0f;
        glfwGetMonitorContentScale(native_monitor(monitor), &x, &y);
        return {x, y};
    }

    Window* DisplaySystem::create_window(const Monitor& monitor, const Enum::window_mode mode, const int width,
                                         const int height) {
        auto window = std::unique_ptr<Window>(new Window(monitor, native_monitor(monitor), mode, width, height));
        auto* result = window.get();
        windows_.push_back(std::move(window));
        return result;
    }

    Window* DisplaySystem::create_window(const Monitor& monitor, const Enum::window_mode mode,
                                         const Resolution resolution) {
        return create_window(monitor, mode, resolution.width, resolution.height);
    }

    Window* DisplaySystem::create_window(const Monitor& monitor, const Enum::window_mode mode) {
        return create_window(monitor, mode, monitor.width, monitor.height);
    }

    void DisplaySystem::activate_window(Window& window) {
        const auto owned =
            std::ranges::any_of(windows_, [&window](const auto& candidate) { return candidate.get() == &window; });
        if (!owned)
            throw Exceptions::invalid_args(CE_HERE, "Active window must be owned by DisplaySystem");
        if (active_ && active_ != &window)
            throw Exceptions::failed_operation(CE_HERE, "Switching active rendering windows is not supported");
        active_ = &window;
    }
}
