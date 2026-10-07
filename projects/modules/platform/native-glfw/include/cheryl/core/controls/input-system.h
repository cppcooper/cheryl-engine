#pragma once

#include <core/controls/input-interface.h>
#include <core/diagnostics.h>

#if CHERYL_NATIVE_INPUT
#include "input-mapper.h"

#include <exception>
#include <memory>
#include <templates/singleton.h>
#include <utility>

class GLFWwindow;

namespace CE {
    class iWindow;
    class Window;
}

namespace CE::Input {
    class GlfwInputDevice;
    class GainputLifetime;

    // Routes GLFW window input into Gainput; Gainput polls gamepads directly.
    // The engine attaches the window before AbstractGame::init, where games can bind device IDs.
    // GLFW event processing and Gainput Update stay on the platform thread. Completed action snapshots
    // can be handed to simulation without reading live Gainput state from another thread.
    // One adapter owns Gainput from its first attachment through destruction.
    // Windows reattachment requires the same live native notification window.
    // Detachment retains devices; Update receives elapsed seconds between attached polls.
    // Ordered GLFW records are captured before Gainput mapping. The character
    // callback provides OS text independently of physical keyboard State.
    class InputSystem final : public iInputSystem,
                              public Singleton_CTS<InputSystem> {
        std::unique_ptr<GainputLifetime> gainput_lifetime_;
        gainput::InputManager manager_;
        InputMapper bindings_;
        bool manager_initialized_ = false;
        InputClock::time_point last_update_{};
        GlfwInputDevice* keyboard_ = nullptr;
        GlfwInputDevice* mouse_ = nullptr;
        gainput::DeviceId keyboard_id_ = gainput::InvalidDeviceId;
        gainput::DeviceId mouse_id_ = gainput::InvalidDeviceId;
        gainput::DeviceId gamepad_id_ = gainput::InvalidDeviceId;
        Window* window_ = nullptr;
        double cursor_x_ = 0.0;
        double cursor_y_ = 0.0;
        std::vector<float> pad_axes_;
        std::vector<bool> pad_buttons_;
        std::exception_ptr callback_failure_;
        const Diagnostics::DomainId domain_ = Diagnostics::next_domain_id();
        bool observed_gamepad_available_ = false;
        bool observed_window_focus_ = false;

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

        static InputSystem* attached(GLFWwindow* window) noexcept;
        static void on_key(GLFWwindow* window, int key, int scancode, int action, int modifiers) noexcept;
        static void on_mouse_button(GLFWwindow* window, int button, int action, int modifiers) noexcept;
        static void on_scroll(GLFWwindow* window, double x, double y) noexcept;
        static void on_character(GLFWwindow* window, unsigned int codepoint) noexcept;
        static void on_cursor(GLFWwindow* window, double x, double y) noexcept;

    public:
        InputSystem();
        ~InputSystem() override;

        void initialize(iWindow& window) override;
        void poll() override;
        // Host event-pump integration: begin_poll(), pump GLFW, then update().
        void begin_poll();
        void update();
        void deinitialize() override;

        [[nodiscard]] InputBindings& bindings() override { return bindings_; }
        // Access devices/configuration after initialize(); Init/Exit belong to this adapter.
        [[nodiscard]] gainput::InputManager& manager() { return manager_; }
        [[nodiscard]] DeviceId keyboard_id() const override { return keyboard_id_; }
        [[nodiscard]] DeviceId mouse_id() const override { return mouse_id_; }
        [[nodiscard]] DeviceId gamepad_id() const override { return gamepad_id_; }
        [[nodiscard]] bool supports_focus() const override { return true; }
        [[nodiscard]] bool supports(InputMode mode) const override {
            return mode == InputMode::State || mode == InputMode::Events || mode == InputMode::Text;
        }
    };
}
#endif
