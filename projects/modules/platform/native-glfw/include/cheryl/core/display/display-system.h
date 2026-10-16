#pragma once
#include <core/display/display-system-interface.h>
#include "window.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

class GLFWmonitor;

namespace CE::Platform::GLFW {
    struct Resolution {
        int width{};
        int height{};
    };

    /** Owns a share of GLFW lifetime and all created windows on the platform thread.
     * Monitor modes/IDs are captured at construction with the primary entry first;
     * there is no hotplug refresh or native-handle rebinding. Keep monitor topology
     * stable while using this inventory. Borrowed windows and monitor references
     * expire at display destruction, after dependent input/context owners retire.
     */
    class DisplaySystem final : public ::CE::Platform::DisplaySystem {
    public:
        DisplaySystem();

        [[nodiscard]] const std::vector<::CE::Platform::Monitor>& monitors() const override { return monitors_; }
        [[nodiscard]] int monitor_count() const override { return static_cast<int>(monitors_.size()); }
        [[nodiscard]] const ::CE::Platform::Monitor& primary_monitor() const override { return primary_monitor_; }
        // nullptr until activate_window(); successful creation does not select a window.
        [[nodiscard]] ::CE::Platform::GLFW::Window* active_window() const override { return active_; }
        /** Query current GLFW monitor scale by an owned snapshot's ID; foreign IDs
         * throw invalid_args. This does not refresh dimensions or report window scale.
         */
        [[nodiscard]] std::pair<float, float> content_scale(const ::CE::Platform::Monitor& monitor) const override;

        /** Borrow a new display-owned window. Positive logical dimensions and a known
         * mode are required; foreign monitor IDs, allocation/native creation failures
         * propagate. An omitted/empty title selects a generated title. The overload
         * without dimensions uses the captured monitor dimensions.
         */
        ::CE::Platform::GLFW::Window*
        create_window(const ::CE::Platform::Monitor& monitor, ::CE::Platform::WindowMode mode, int width, int height) override;
        ::CE::Platform::GLFW::Window* create_window(
            const ::CE::Platform::Monitor& monitor,
            ::CE::Platform::WindowMode mode,
            int width,
            int height,
            const std::string& title
        );
        ::CE::Platform::GLFW::Window*
        create_window(const ::CE::Platform::Monitor& monitor, ::CE::Platform::WindowMode mode, ::CE::Platform::GLFW::Resolution resolution);
        ::CE::Platform::GLFW::Window* create_window(const ::CE::Platform::Monitor& monitor, ::CE::Platform::WindowMode mode);
        /** Select the initial owned window, accepting repeat selection of that window.
         * Foreign windows or replacing the active window fail explicitly. This does
         * not make a graphics context current or switch renderer/resource domains.
         */
        void activate_window(::CE::Platform::Window& window) override;

    private:
        struct GlfwLibrary {
            GlfwLibrary();
            ~GlfwLibrary();
            GlfwLibrary(const GlfwLibrary&) = delete;
            GlfwLibrary& operator=(const GlfwLibrary&) = delete;
        } glfw_;

        [[nodiscard]] static ::CE::Platform::Monitor create_primary_monitor();
        [[nodiscard]] GLFWmonitor* native_monitor(const ::CE::Platform::Monitor& monitor) const;

        std::vector<::CE::Platform::Monitor> monitors_;
        std::vector<GLFWmonitor*> native_monitors_;
        std::vector<std::unique_ptr<::CE::Platform::GLFW::Window>> windows_;
        ::CE::Platform::Monitor primary_monitor_;
        ::CE::Platform::GLFW::Window* active_ = nullptr;
    };
}
