#pragma once

#include "input-interface.h"

#ifndef CHERYL_SANDBOX_BUILD
#include "input-mapper.h"

#include <exception>
#include <templates/singleton.h>
#include <utility>

class GLFWwindow;

namespace CE {
    class iWindow;
    class Window;
}

namespace CE::Input {
    class GlfwInputDevice;

    // Routes GLFW window input into Gainput; Gainput polls gamepads directly.
    // The engine attaches the window before AbstractGame::init, where games can bind device IDs.
    // GLFW event processing and Gainput Update stay on the platform thread. Completed action snapshots
    // can be handed to simulation without reading live Gainput state from another thread.
    // Ordered GLFW records are captured before Gainput mapping. The character
    // callback provides OS text independently of physical keyboard State.
    class InputSystem final : public iInputSystem, public Singleton_CTS<InputSystem> {
        gainput::InputManager manager_;
        InputMapper bindings_;
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

        template <typename Work>
        void receive(Work&& work) noexcept {
            if (callback_failure_)
                return;
            try {
                std::forward<Work>(work)();
            }
            catch (...) {
                // Do not unwind through GLFW's C callback stack. update() reports
                // the first failure on the normal runtime/teardown path instead.
                callback_failure_ = std::current_exception();
            }
        }

        static InputSystem* attached(GLFWwindow* window);
        static void on_key(GLFWwindow* window, int key, int scancode, int action, int modifiers);
        static void on_mouse_button(GLFWwindow* window, int button, int action, int modifiers);
        static void on_scroll(GLFWwindow* window, double x, double y);
        static void on_character(GLFWwindow* window, unsigned int codepoint);
        static void on_cursor(GLFWwindow* window, double x, double y);

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
        // Access the backend for extra Gainput devices and configuration.
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
