#pragma once

#include <cheryl/export.h>

#include <core/controls/input-interface.h>
#include <core/diagnostics.h>

#if CHERYL_NATIVE_INPUT
#include "input-mapper.h"

#include <exception>
#include <memory>
#include <templates/singleton.h>
#include <utility>

class GLFWwindow;

namespace CE::Platform {
    class Window;
}

namespace CE::Platform::GLFW {
    class Window;
}

namespace CE::Platform::GLFW {
    namespace Detail {
        class InputDevice;
        class GainputLifetime;
    }

    // Routes GLFW window input into Gainput; Gainput polls gamepads directly.
    // The engine attaches the window before AbstractGame::init, where games can bind device IDs.
    // GLFW event processing and Gainput Update stay on the platform thread. Completed action snapshots
    // can be handed to simulation without reading live Gainput state from another thread.
    // One adapter owns Gainput from its first attachment through destruction.
    // Windows reattachment requires the same live native notification window.
    // Detachment retains devices; Update receives elapsed seconds between attached polls.
    // Ordered GLFW records are captured before Gainput mapping. The character
    // callback provides OS text independently of physical keyboard State.
    class CE_TYPE_API InputSystem final : public CE::Input::System,
                              public CE::State::Singleton<InputSystem> {
        std::unique_ptr<Detail::GainputLifetime> gainput_lifetime_;
        gainput::InputManager manager_;
        GainputMapper bindings_;
        bool manager_initialized_ = false;
        CE::Input::ObservationClock::time_point last_update_{};
        Detail::InputDevice* keyboard_ = nullptr;
        Detail::InputDevice* mouse_ = nullptr;
        gainput::DeviceId keyboard_id_ = gainput::InvalidDeviceId;
        gainput::DeviceId mouse_id_ = gainput::InvalidDeviceId;
        gainput::DeviceId gamepad_id_ = gainput::InvalidDeviceId;
        Window* window_ = nullptr;
        double cursor_x_ = 0.0;
        double cursor_y_ = 0.0;
        std::vector<float> pad_axes_;
        std::vector<bool> pad_buttons_;
        std::exception_ptr callback_failure_;
        const CE::Diagnostics::DomainId domain_ = CE::Diagnostics::next_domain_id();
        bool observed_gamepad_available_ = false;
        bool observed_window_focus_ = false;
        bool gamepad_diagnostics_ = false;
        CE::Input::ObservationClock::time_point next_gamepad_diagnostics_{};

        template <typename Work> void receive(Work&& work) noexcept {
            if (callback_failure_)
                return;
            try {
                std::forward<Work>(work)();
            } catch (...) {
                // Do not unwind through GLFW's C callback stack. update() reports
                // the first failure on the normal runtime/teardown path instead.
                callback_failure_ = std::current_exception();
            }
        }

        CE_NATIVE_GLFW_API static InputSystem* attached(GLFWwindow* window) noexcept;
        CE_NATIVE_GLFW_API static void on_key(GLFWwindow* window, int key, int scancode, int action, int modifiers) noexcept;
        CE_NATIVE_GLFW_API static void on_mouse_button(GLFWwindow* window, int button, int action, int modifiers) noexcept;
        CE_NATIVE_GLFW_API static void on_scroll(GLFWwindow* window, double x, double y) noexcept;
        CE_NATIVE_GLFW_API static void on_character(GLFWwindow* window, unsigned int codepoint) noexcept;
        CE_NATIVE_GLFW_API static void on_cursor(GLFWwindow* window, double x, double y) noexcept;

    public:
        CE_NATIVE_GLFW_API InputSystem();
        CE_NATIVE_GLFW_API ~InputSystem() override;

        CE_NATIVE_GLFW_API void initialize(CE::Platform::Window& window) override;
        CE_NATIVE_GLFW_API void poll() override;
        // Host event-pump integration: begin_poll(), pump GLFW, then update().
        CE_NATIVE_GLFW_API void begin_poll();
        CE_NATIVE_GLFW_API void update();
        CE_NATIVE_GLFW_API void deinitialize() override;
        // Opt-in gamepad values/IDs at TRACE; call on platform before startup or between polls.
        CE_NATIVE_GLFW_API void set_gamepad_diagnostics(bool enabled) noexcept;

        [[nodiscard]] CE::Input::Actions::Bindings& bindings() override { return bindings_; }
        // Access devices/configuration after initialize(); Init/Exit belong to this adapter.
        [[nodiscard]] gainput::InputManager& manager() { return manager_; }
        [[nodiscard]] CE::Input::DeviceId keyboard_id() const override { return keyboard_id_; }
        [[nodiscard]] CE::Input::DeviceId mouse_id() const override { return mouse_id_; }
        [[nodiscard]] CE::Input::DeviceId gamepad_id() const override { return gamepad_id_; }
        [[nodiscard]] bool supports_focus() const override { return true; }
        [[nodiscard]] bool supports(CE::Input::Capture::Channel mode) const override {
            return mode == CE::Input::Capture::Channel::State || mode == CE::Input::Capture::Channel::Events ||
                   mode == CE::Input::Capture::Channel::Text;
        }
    };
}

namespace CE::State {
    template <> CE_NATIVE_GLFW_API auto Singleton<CE::Platform::GLFW::InputSystem>::storage() -> Storage&;
}
#endif
