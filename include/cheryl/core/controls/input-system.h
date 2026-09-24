#pragma once

#include "input-interface.h"

#ifndef CHERYL_SANDBOX_BUILD
#include "input-mapper.h"

#include <templates/singleton.h>

class GLFWwindow;

namespace CE {
    class iWindow;
    class Window;
}

namespace CE::Input {
    class GlfwInputDevice;

    // Routes GLFW window input into Gainput; Gainput polls gamepads directly.
    // The engine attaches the window before AbstractGame::init, where games can bind device IDs.
    // TODO: Do not move poll() wholesale to a worker thread. GLFW event processing/window callbacks belong
    // to the platform/main thread; if input processing becomes concurrent, hand gathered state to simulation
    // and keep callback queues plus Gainput Update on one owner thread or add explicit synchronization.
    class InputSystem final : public iInputSystem, public Singleton_CTS<InputSystem> {
    public:
        InputSystem();
        ~InputSystem() override;

        void initialize(iWindow& window) override;
        void poll() override;
        void update();
        void deinitialize() override;

        [[nodiscard]] InputMapper& bindings() override { return bindings_; }
        [[nodiscard]] gainput::InputManager& manager() { return manager_; }
        [[nodiscard]] DeviceId keyboard_id() const override { return keyboard_id_; }
        [[nodiscard]] DeviceId mouse_id() const override { return mouse_id_; }
        [[nodiscard]] DeviceId gamepad_id() const override { return gamepad_id_; }

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
#endif
