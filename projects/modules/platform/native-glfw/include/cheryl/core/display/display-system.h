#pragma once

#include <cheryl/export.h>
#include <core/display/display-system-interface.h>
#include "window.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

struct GLFWmonitor;

namespace CE::Platform::GLFW {
    /** Requested GLFW OpenGL context hints applied to a newly created window, not a runtime backend switch. */
    struct OpenGLContextOptions {
        int major = 3; ///< Positive requested OpenGL major version.
        int minor = 3; ///< Nonnegative requested minor version.
        bool core_profile = true; ///< Request the OpenGL core profile.
#ifdef __APPLE__
        bool forward_compatible = true; ///< Request forward compatibility; defaults true on Apple.
#else
        bool forward_compatible = false; ///< Forward-compatibility request; defaults false outside Apple.
#endif
        bool visible = true; ///< Whether the new native window starts visible.
    };
    /** Requested logical window dimensions; creation requires positive values. */
    struct Resolution {
        int width{}; ///< Logical window width.
        int height{}; ///< Logical window height.
    };

    /** Owns a share of GLFW lifetime and all created windows on the platform thread.
     * Monitor modes/IDs are captured at construction with the primary entry first;
     * there is no hotplug refresh or native-handle rebinding. Keep monitor topology
     * stable while using this inventory. Borrowed windows and monitor references
     * expire at display destruction, after dependent input/context owners retire.
     */
    class CE_TYPE_API DisplaySystem final : public ::CE::Platform::DisplaySystem {
    public:
        [[nodiscard]] CE_NATIVE_GLFW_API Window* create_opengl_window(
            const ::CE::Platform::Monitor& monitor,
            ::CE::Platform::WindowMode mode,
            int width,
            int height,
            const std::string& title,
            const OpenGLContextOptions& context = {}
        );
        CE_NATIVE_GLFW_API DisplaySystem();

        [[nodiscard]] const std::vector<::CE::Platform::Monitor>& monitors() const override { return monitors_; }
        [[nodiscard]] int monitor_count() const override { return static_cast<int>(monitors_.size()); }
        [[nodiscard]] const ::CE::Platform::Monitor& primary_monitor() const override { return primary_monitor_; }
        // nullptr until activate_window(); successful creation does not select a window.
        [[nodiscard]] ::CE::Platform::GLFW::Window* active_window() const override { return active_; }
        [[nodiscard]] CE_NATIVE_GLFW_API std::pair<float, float> content_scale(const ::CE::Platform::Monitor& monitor) const override;

        CE_NATIVE_GLFW_API ::CE::Platform::GLFW::Window*
        create_window(const ::CE::Platform::Monitor& monitor, ::CE::Platform::WindowMode mode, int width, int height) override;
        CE_NATIVE_GLFW_API ::CE::Platform::GLFW::Window* create_window(
            const ::CE::Platform::Monitor& monitor,
            ::CE::Platform::WindowMode mode,
            int width,
            int height,
            const std::string& title
        );
        CE_NATIVE_GLFW_API ::CE::Platform::GLFW::Window*
        create_window(const ::CE::Platform::Monitor& monitor, ::CE::Platform::WindowMode mode, ::CE::Platform::GLFW::Resolution resolution);
        CE_NATIVE_GLFW_API ::CE::Platform::GLFW::Window* create_window(const ::CE::Platform::Monitor& monitor, ::CE::Platform::WindowMode mode);
        CE_NATIVE_GLFW_API void activate_window(::CE::Platform::Window& window) override;

    private:
        struct GlfwLibrary {
            CE_NATIVE_GLFW_API GlfwLibrary();
            CE_NATIVE_GLFW_API ~GlfwLibrary();
            GlfwLibrary(const GlfwLibrary&) = delete;
            GlfwLibrary& operator=(const GlfwLibrary&) = delete;
        } glfw_;

        [[nodiscard]] CE_NATIVE_GLFW_API static ::CE::Platform::Monitor create_primary_monitor();
        [[nodiscard]] CE_NATIVE_GLFW_API GLFWmonitor* native_monitor(const ::CE::Platform::Monitor& monitor) const;

        std::vector<::CE::Platform::Monitor> monitors_;
        std::vector<GLFWmonitor*> native_monitors_;
        std::vector<std::unique_ptr<::CE::Platform::GLFW::Window>> windows_;
        ::CE::Platform::Monitor primary_monitor_;
        ::CE::Platform::GLFW::Window* active_ = nullptr;
    };
}

/** @fn CE::Platform::GLFW::DisplaySystem::DisplaySystem()
 * Acquire a shared GLFW library lifetime and capture monitors/modes with primary first. Native initialization or absent monitors/video modes throws. Construct/destroy on the platform owner; this is not hotplug inventory refresh.
 * @see [Display ownership](cheryl-guide:modules/native-glfw)
 */

/** @fn CE::Platform::GLFW::Window* CE::Platform::GLFW::DisplaySystem::create_opengl_window(const CE::Platform::Monitor& monitor, CE::Platform::WindowMode mode, int width, int height, const std::string& title, const OpenGLContextOptions& context)
 * Borrow a new display-owned OpenGL window using validated positive dimensions, known mode and requested version/profile/visibility. Foreign monitor IDs, invalid version or native/allocation failures throw. Creation does not activate a rendering window or make the context current.
 */

/** @fn const std::vector<CE::Platform::Monitor>& CE::Platform::GLFW::DisplaySystem::monitors() const
 * Borrow captured monitor snapshots with primary first; do not retain past DisplaySystem destruction. No live topology refresh occurs.
 */

/** @fn int CE::Platform::GLFW::DisplaySystem::monitor_count() const
 * Return the captured monitor count, not a fresh native enumeration.
 */

/** @fn const CE::Platform::Monitor& CE::Platform::GLFW::DisplaySystem::primary_monitor() const
 * Borrow the primary monitor snapshot retained at construction; display must outlive the reference.
 */

/** @fn CE::Platform::GLFW::Window* CE::Platform::GLFW::DisplaySystem::active_window() const
 * Borrow the initially selected window, null until activate_window(). Successful creation alone does not select it.
 */

/** @fn std::pair<float, float> CE::Platform::GLFW::DisplaySystem::content_scale(const CE::Platform::Monitor& monitor) const
 * Query current native X/Y monitor scale by an owned snapshot ID on platform. Foreign IDs throw invalid_args; captured resolution and window scale are not refreshed.
 */

/** @fn CE::Platform::GLFW::Window* CE::Platform::GLFW::DisplaySystem::create_window(const CE::Platform::Monitor& monitor, CE::Platform::WindowMode mode, int width, int height)
 * Create a display-owned window using current GLFW hints and a generated title; use create_opengl_window() for explicit context hints. Positive logical dimensions/known mode/owned monitor required; native/allocation failures propagate. The returned pointer borrows display lifetime.
 */

/** @fn CE::Platform::GLFW::Window* CE::Platform::GLFW::DisplaySystem::create_window(const CE::Platform::Monitor& monitor, CE::Platform::WindowMode mode, int width, int height, const std::string& title)
 * Create a native window with copied title; empty title selects a generated one. Validation/ownership follows the untitled overload. Creation neither selects an active rendering window nor transfers ownership to the caller.
 */

/** @fn CE::Platform::GLFW::Window* CE::Platform::GLFW::DisplaySystem::create_window(const CE::Platform::Monitor& monitor, CE::Platform::WindowMode mode, Resolution resolution)
 * Create using the supplied logical resolution; delegates to the positive-dimension untitled overload.
 */

/** @fn CE::Platform::GLFW::Window* CE::Platform::GLFW::DisplaySystem::create_window(const CE::Platform::Monitor& monitor, CE::Platform::WindowMode mode)
 * Create using captured monitor dimensions and a generated title; this is not a fresh native-resolution query.
 */

/** @fn void CE::Platform::GLFW::DisplaySystem::activate_window(CE::Platform::Window& window)
 * Select the initial display-owned window. Repeat selection of the same window is accepted; foreign or replacement windows reject. Does not make a context current, switch rendering/resource domains or enable multiple active rendering windows.
 */
