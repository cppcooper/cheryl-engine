#pragma once

#include <cheryl/export.h>

#include <core/controls/input-interface.h>
#include <core/diagnostics.h>

#if CHERYL_NATIVE_INPUT
#include <core/controls/input-bindings.h>

#include <cstdint>
#include <memory>
#include <templates/singleton.h>
#include <utility>

struct GLFWwindow;

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
        struct InputTestAccess;
    }

    struct DeviceSnapshot {
        std::uint64_t identity = 0;
        bool available = false;
        [[nodiscard]] bool known() const noexcept { return identity != 0; }
    };

    // Normalizes GLFW window input and Gainput controller reports for Engine bindings.
    // The engine attaches the window before AbstractGame::init, where games can bind device IDs.
    // GLFW event processing and Gainput Update stay on the platform thread. Completed action snapshots
    // can be handed to simulation without reading live Gainput state from another thread.
    // One adapter owns Gainput from its first attachment through destruction.
    // Windows reattachment requires the same live native notification window.
    // Detachment retains devices; Update receives elapsed seconds between attached polls.
    // Ordered GLFW records are captured before binding evaluation. The character
    // callback provides OS text independently of physical keyboard State.
    class CE_TYPE_API InputSystem final : public CE::Input::System,
                              public CE::State::Singleton<InputSystem> {
        struct State;
        std::unique_ptr<State> state_;
        friend struct Detail::InputTestAccess;

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

        [[nodiscard]] CE_NATIVE_GLFW_API CE::Input::Actions::Bindings& bindings() override;
        [[nodiscard]] CE_NATIVE_GLFW_API CE::Input::DeviceId keyboard_id() const override;
        [[nodiscard]] CE_NATIVE_GLFW_API CE::Input::DeviceId mouse_id() const override;
        [[nodiscard]] CE_NATIVE_GLFW_API CE::Input::DeviceId gamepad_id() const override;
        // Observe on the platform/input owner. An unknown device has identity zero.
        // Identity is opaque and stable while this adapter retains the device.
        [[nodiscard]] CE_NATIVE_GLFW_API DeviceSnapshot device(CE::Input::DeviceId id) const;
        [[nodiscard]] CE_NATIVE_GLFW_API std::uint64_t backend_time_ms() const;
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
