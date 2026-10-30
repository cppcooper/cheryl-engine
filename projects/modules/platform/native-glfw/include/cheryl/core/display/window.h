#pragma once

#include <cheryl/export.h>
#include <core/display/monitor.h>
#include <core/display/window-interface.h>

#include <string>
#include <exception>
#include <core/diagnostics.h>
#include <chrono>

struct GLFWwindow;
struct GLFWmonitor;

namespace CE::Platform::GLFW {
    class DisplaySystem;

    /** Owns a GLFW window and its logical/framebuffer dimensions on the platform
     * thread. Native callbacks retain their first failure; normal polling and
     * explicit resize/mode boundaries consume it through check_native_failure().
     * Size getters return cached observations updated by callbacks and explicit
     * resize/mode queries; they neither pump events nor synchronize cross-thread reads.
     * Input and graphics users release this borrowed native window before destruction.
     */
    class CE_TYPE_API Window final : public ::CE::Platform::Window {
        ::CE::Platform::Dimensions<int> logical_size_;
        ::CE::Platform::FramebufferSize framebuffer_size_{};
        ::CE::Platform::WindowMode window_mode_;
        ::CE::Platform::Monitor monitor_;
        GLFWmonitor* glfw_monitor_;
        GLFWwindow* glfw_window_;
        int windowed_x_ = 0;
        int windowed_y_ = 0;
        int windowed_width_;
        int windowed_height_;
        mutable std::exception_ptr native_failure_;
        const ::CE::Diagnostics::DomainId domain_ = ::CE::Diagnostics::next_domain_id();
        mutable std::uint64_t resize_observations_ = 0;
        mutable std::chrono::steady_clock::time_point next_diagnostic_;

    public:
        /** Native graphics procedure pointer; lifetime follows the native context/library. */
        using ProcAddress = void (*)();

        CE_NATIVE_GLFW_API ~Window() override;
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(Window&&) = delete;

        // Borrowed until destruction. Preserve Window's GLFW user pointer and callbacks.
        [[nodiscard]] GLFWwindow* native_handle() const { return glfw_window_; }
        [[nodiscard]] ::CE::Diagnostics::DomainId diagnostic_id() const noexcept { return domain_; }
        [[nodiscard]] ::CE::Platform::Dimensions<int> logical_size() const override { return logical_size_; }
        [[nodiscard]] ::CE::Platform::FramebufferSize framebuffer_size() const override { return framebuffer_size_; }
        // Selected mode; a later callback failure does not roll this value back.
        [[nodiscard]] ::CE::Platform::WindowMode mode() const override { return window_mode_; }
        [[nodiscard]] CE_NATIVE_GLFW_API bool should_close() const override;
        [[nodiscard]] bool supports_close_intent() const noexcept override { return true; }
        [[nodiscard]] CE_NATIVE_GLFW_API bool consume_close_intent() override;
        [[nodiscard]] CE_NATIVE_GLFW_API std::optional<bool> focused() const override;
        CE_NATIVE_GLFW_API void check_native_failure() const override;

        CE_NATIVE_GLFW_API void resize(int width, int height) override;
        CE_NATIVE_GLFW_API void set_mode(::CE::Platform::WindowMode mode) override;
        CE_NATIVE_GLFW_API void hide_cursor(bool hide) const override;

        // Context operations may run on a rendering owner after platform creation.
        // Release a context before transferring it to another thread.
        CE_NATIVE_GLFW_API void make_context_current(int swap_interval = 1);
        CE_NATIVE_GLFW_API void release_context();
        [[nodiscard]] CE_NATIVE_GLFW_API bool context_current() const;
        [[nodiscard]] CE_NATIVE_GLFW_API ProcAddress context_proc_address(const char* name) const;
        CE_NATIVE_GLFW_API void swap_buffers();

    private:
        friend class ::CE::Platform::GLFW::DisplaySystem;
        CE_NATIVE_GLFW_API Window(
            const ::CE::Platform::Monitor& monitor,
            GLFWmonitor* native_monitor,
            ::CE::Platform::WindowMode mode,
            int width,
            int height,
            const std::string& title
        );

        CE_NATIVE_GLFW_API static void on_window_size(GLFWwindow* window, int width, int height) noexcept;
        CE_NATIVE_GLFW_API static void on_framebuffer_size(GLFWwindow* window, int width, int height) noexcept;
        CE_NATIVE_GLFW_API void update_framebuffer_size(int width, int height);
    };
}

/** @fn CE::Platform::GLFW::Window::~Window()
 * Destroy the native window on platform after input and graphics owners release their borrowed handle/context use. No external native reference may survive.
 */

/** @fn GLFWwindow* CE::Platform::GLFW::Window::native_handle() const
 * Borrow the native window until destruction. Preserve its user pointer and installed callbacks; application replacement can invalidate Engine integration.
 */

/** @fn CE::Diagnostics::DomainId CE::Platform::GLFW::Window::diagnostic_id() const noexcept
 * Return this window's diagnostic lifetime identity, not a stable monitor/window configuration ID.
 */

/** @fn CE::Platform::Dimensions<int> CE::Platform::GLFW::Window::logical_size() const
 * Copy cached logical window dimensions on platform; no event pump/native query or cross-thread synchronization occurs.
 */

/** @fn CE::Platform::FramebufferSize CE::Platform::GLFW::Window::framebuffer_size() const
 * Copy cached drawable pixel dimensions, distinct from logical size. Callbacks/explicit boundaries refresh them; minimized windows may have zero extent.
 */

/** @fn CE::Platform::WindowMode CE::Platform::GLFW::Window::mode() const
 * Return selected mode; a subsequent callback failure does not roll it back.
 */

/** @fn bool CE::Platform::GLFW::Window::should_close() const
 * Query native close intent on platform; does not pump events or perform teardown.
 */

/** @fn void CE::Platform::GLFW::Window::check_native_failure() const
 * Consume/rethrow the first retained callback failure at a normal engine boundary. Callback failure capture avoids throwing through the native callback ABI; a second check does not rethrow the consumed exception.
 */

/** @fn void CE::Platform::GLFW::Window::resize(int width, int height)
 * Request positive logical dimensions and refresh cached logical/framebuffer observations. Invalid sizes reject; listeners/native callbacks may fail after dimensions have changed. This mutation is not atomic.
 */

/** @fn void CE::Platform::GLFW::Window::set_mode(CE::Platform::WindowMode mode)
 * NORMAL restores saved windowed placement; BORDERLESS uses it undecorated; FULLSCREEN attaches the selected monitor at captured dimensions. Refresh observed sizes. Unknown modes/native errors fail; callback failure does not roll back mode/dimensions.
 */

/** @fn void CE::Platform::GLFW::Window::hide_cursor(bool hide) const
 * Set native cursor hidden/normal mode on platform; this does not supply toolkit OS cursor services or relative pointer capture.
 */

/** @fn void CE::Platform::GLFW::Window::make_context_current(int swap_interval)
 * Bind this OpenGL context to the rendering owner and set nonnegative swap interval. Negative interval or unsuccessful binding throws. Release from the prior thread before transfer; initial window creation remains platform-owned.
 */

/** @fn void CE::Platform::GLFW::Window::release_context()
 * Unbind this context from the calling rendering owner; throws if it is not current on that thread.
 */

/** @fn bool CE::Platform::GLFW::Window::context_current() const
 * Return whether this native context is current on the calling thread; no transfer or synchronization is performed.
 */

/** @fn CE::Platform::GLFW::Window::ProcAddress CE::Platform::GLFW::Window::context_proc_address(const char* name) const
 * Look up a native graphics procedure with this context current. Null name or absent current context rejects; an unknown procedure can return null. The name is borrowed only during lookup.
 */

/** @fn void CE::Platform::GLFW::Window::swap_buffers()
 * Present this window on its rendering owner with its context current; absent current context throws. Framebuffer clearing/drawing is owned separately.
 * @see [OpenGL context integration](cheryl-guide:modules/opengl)
 */
