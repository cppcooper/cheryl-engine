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
    // GLFW event processing and Gainput Update stay on the platform thread. Completed action snapshots
    // can be handed to simulation without reading live Gainput state from another thread.
    // TODO: For InputMode::Events, retain ordered, timestamped physical transitions before
    // Gainput condenses them. For InputMode::Text, capture OS text (including repeats)
    // through character input and deliver editing controls separately. Textbox focus
    // routes keyboard input; it must not be implemented by converting action keys to characters.
    class InputSystem final : public iInputSystem, public Singleton_CTS<InputSystem> {
        gainput::InputManager manager_;
        InputMapper bindings_;
        GlfwInputDevice* keyboard_ = nullptr;
        GlfwInputDevice* mouse_ = nullptr;
        gainput::DeviceId keyboard_id_ = gainput::InvalidDeviceId;
        gainput::DeviceId mouse_id_ = gainput::InvalidDeviceId;
        gainput::DeviceId gamepad_id_ = gainput::InvalidDeviceId;
        Window* window_ = nullptr;

        static void on_key(GLFWwindow* window, int key, int scancode, int action, int modifiers);
        static void on_mouse_button(GLFWwindow* window, int button, int action, int modifiers);
        static void on_scroll(GLFWwindow* window, double x, double y);

    public:
        InputSystem();
        ~InputSystem() override;

        void initialize(iWindow& window) override;
        void poll() override;
        // Advance Gainput after a host application pumps GLFW events itself.
        void update();
        void deinitialize() override;

        [[nodiscard]] InputBindings& bindings() override { return bindings_; }
        // Access the backend for extra Gainput devices and configuration.
        [[nodiscard]] gainput::InputManager& manager() { return manager_; }
        [[nodiscard]] DeviceId keyboard_id() const override { return keyboard_id_; }
        [[nodiscard]] DeviceId mouse_id() const override { return mouse_id_; }
        [[nodiscard]] DeviceId gamepad_id() const override { return gamepad_id_; }
    };
}
#endif
