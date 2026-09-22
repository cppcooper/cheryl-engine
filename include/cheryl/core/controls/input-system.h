#pragma once

#include "input-mapper.h"

#include <templates/singleton.h>

class GLFWwindow;

namespace CE {
    class Window;
}

namespace CE::Input {
    class GlfwInputDevice;

    // Routes GLFW window input into Gainput; Gainput polls gamepads directly.
    // The engine attaches the window before AbstractGame::init, where games can bind device IDs.
    class InputSystem final : public Singleton_CTS<InputSystem> {
    public:
        InputSystem();
        ~InputSystem();

        void initialize(Window& window);
        void update();
        void deinitialize();

        [[nodiscard]] InputMapper& bindings() { return bindings_; }
        [[nodiscard]] gainput::InputManager& manager() { return manager_; }
        [[nodiscard]] gainput::DeviceId keyboard_id() const { return keyboard_id_; }
        [[nodiscard]] gainput::DeviceId mouse_id() const { return mouse_id_; }
        [[nodiscard]] gainput::DeviceId gamepad_id() const { return gamepad_id_; }

    private:
        static void on_key(GLFWwindow* window, int key, int scancode, int action, int modifiers);
        static void on_mouse_button(GLFWwindow* window, int button, int action, int modifiers);
        static void on_cursor(GLFWwindow* window, double x, double y);
        static void on_scroll(GLFWwindow* window, double x, double y);

        gainput::InputManager manager_;
        InputMapper bindings_;
        GlfwInputDevice* keyboard_ = nullptr;
        GlfwInputDevice* mouse_ = nullptr;
        gainput::DeviceId keyboard_id_ = gainput::InvalidDeviceId;
        gainput::DeviceId mouse_id_ = gainput::InvalidDeviceId;
        gainput::DeviceId gamepad_id_ = gainput::InvalidDeviceId;
        Window* window_ = nullptr;
    };
}
