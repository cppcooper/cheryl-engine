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

    /** Copied native-device identity/availability observation; known does not imply currently connected. */
    struct DeviceSnapshot {
        std::uint64_t identity = 0; ///< Opaque stable identity while this adapter retains the device;0 means unknown.
        bool available = false; ///< Native device currently reports availability.
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
    /** GLFW/Gainput input adapter on the platform owner, available only with CHERYL_NATIVE_INPUT.
     * Retains device identities across detachment; borrowed windows survive callback removal.
     * Completed snapshots can cross to simulation without querying live native state.
     * @see [Native input](cheryl-guide:modules/native-glfw) */
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

/** @fn bool CE::Platform::GLFW::DeviceSnapshot::known() const noexcept
 * Return whether identity is nonzero, independently of current device availability.
 */

/** @fn CE::Platform::GLFW::InputSystem::InputSystem()
 * Create an unattached adapter and semantic mapper; native device admission occurs during attachment. Allocation/native setup may fail.
 */

/** @fn CE::Platform::GLFW::InputSystem::~InputSystem()
 * Detach callbacks while the borrowed window is alive, then retire retained Gainput devices/manager state. Destructor reports cleanup failures without throwing; input users must have quiesced.
 */

/** @fn void CE::Platform::GLFW::InputSystem::initialize(CE::Platform::Window& window)
 * Attach a compatible GLFW window before game init on platform. Foreign windows, conflicting adapters or another attachment reject. One adapter owns Gainput from first attachment through destruction; Windows reattachment requires the same live notification window. Partial native/setup errors propagate.
 */

/** @fn void CE::Platform::GLFW::InputSystem::poll()
 * On platform, begin_poll(), pump glfwPollEvents(), then update() and publish. Requires initialization; callback/device/evaluation failures propagate at the native-to-engine boundary.
 */

/** @fn void CE::Platform::GLFW::InputSystem::begin_poll()
 * Latch routing/capture/action-context changes before a host-owned GLFW pump and reset wheel pulse buttons. Requires attachment; do not call from simulation or in the middle of another pump.
 */

/** @fn void CE::Platform::GLFW::InputSystem::update()
 * After the host pumps GLFW, process queued native reports and Gainput using elapsed seconds since attached polls, then publish complete State/records. Requires attachment; captured callback failure is rethrown before normal update. Pair with begin_poll() for host event-loop integration.
 */

/** @fn void CE::Platform::GLFW::InputSystem::deinitialize()
 * Detach callbacks before the borrowed window dies and clear per-window staging/focus/mappings. Device objects remain retained for possible reattachment. Final State release publication can allocate/fail after callbacks have already detached; existing snapshots remain valid.
 */

/** @fn void CE::Platform::GLFW::InputSystem::set_gamepad_diagnostics(bool enabled) noexcept
 * Opt into gamepad value/ID diagnostics at TRACE. Call on platform before startup or between polls; no logging-level availability or native-device acceptance is implied.
 */

/** @fn CE::Input::Actions::Bindings& CE::Platform::GLFW::InputSystem::bindings()
 * Borrow the native adapter's mutable mapper on platform only; acquire action_contexts there for producer-side runtime binding.
 */

/** @fn CE::Input::DeviceId CE::Platform::GLFW::InputSystem::keyboard_id() const
 * Return the retained adapter keyboard token; cache on platform during game init.
 */

/** @fn CE::Input::DeviceId CE::Platform::GLFW::InputSystem::mouse_id() const
 * Return the retained adapter mouse token on platform.
 */

/** @fn CE::Input::DeviceId CE::Platform::GLFW::InputSystem::gamepad_id() const
 * Return the adapter's convenience gamepad token; this API is not native pad enumeration.
 */

/** @fn CE::Platform::GLFW::DeviceSnapshot CE::Platform::GLFW::InputSystem::device(CE::Input::DeviceId id) const
 * Copy opaque identity and availability on the platform/input owner. Unknown IDs return identity 0/false; identities remain stable while this adapter retains each device, not across process runs.
 */

/** @fn std::uint64_t CE::Platform::GLFW::InputSystem::backend_time_ms() const
 * Return Gainput manager time in milliseconds on its platform owner, not simulation elapsed time.
 */

/** @fn bool CE::Platform::GLFW::InputSystem::supports_focus() const
 * Return true: this adapter implements poll-latched keyboard focus routing.
 */

/** @fn bool CE::Platform::GLFW::InputSystem::supports(CE::Input::Capture::Channel mode) const
 * Support State, ordered physical Events and OS committed Text; unknown channels return false. Text support does not include IME composition.
 */

/** @fn template <> auto CE::State::Singleton<CE::Platform::GLFW::InputSystem>::storage() -> Storage&
 * Return process-local singleton storage for native input; normal application access uses initialization/get conventions.
 */
