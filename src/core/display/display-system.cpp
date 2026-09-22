#include <core/display/display-system.h>

#include <internals/exceptions.h>

#include <algorithm>
#include <memory>

namespace CE {
    namespace {
        Monitor create_primary_monitor() {
            auto* handle = glfwGetPrimaryMonitor();
            const auto* video_mode = handle ? glfwGetVideoMode(handle) : nullptr;
            if (!video_mode)
                throw Exceptions::runtime_exception(CE_HERE, "No primary monitor or video mode is available");
            return {handle, video_mode};
        }
    }

    DisplaySystem::DisplaySystem() : primary_monitor_(create_primary_monitor()) {
        int count = 0;
        GLFWmonitor** handles = glfwGetMonitors(&count);
        if (!handles || count <= 0)
            throw Exceptions::runtime_exception(CE_HERE, "No monitors are available");

        monitors_.reserve(static_cast<std::size_t>(count));
        monitors_.push_back(primary_monitor_);
        for (int index = 0; index < count; ++index) {
            if (handles[index] == primary_monitor_.glfw_monitor)
                continue;
            const auto* video_mode = glfwGetVideoMode(handles[index]);
            if (video_mode)
                monitors_.emplace_back(handles[index], video_mode);
        }
    }

    Window* DisplaySystem::create_window(const Monitor& monitor, const Enum::window_mode mode, const int width,
                                         const int height) {
        auto window = std::make_unique<Window>(monitor, mode, width, height);
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
