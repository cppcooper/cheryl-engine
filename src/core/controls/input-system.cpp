#include <core/controls/input-system.h>

#include <core/controls/glfw-bindings.h>
#include <core/display/window.h>
#include <internals/exceptions.h>

#include <gainput/GainputInputDeltaState.h>
#include <gainput/GainputHelpers.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace CE::Input {
    class GlfwInputDevice : public gainput::InputDevice {
    public:
        GlfwInputDevice(gainput::InputManager& manager, const gainput::DeviceId id, const unsigned index,
                        const DeviceVariant variant, const bool mouse) :
            InputDevice(manager, id,
                        index == AutoIndex ? manager.GetDeviceCountByType(mouse ? DT_MOUSE : DT_KEYBOARD) : index),
            mouse_(mouse) {
            // Keep current and previous Gainput state for delta generation;
            // GLFW callbacks enqueue changes instead of mutating either here.
            (void)variant;
            const unsigned count =
                mouse_ ? static_cast<unsigned>(gainput::MouseButtonCount_) : static_cast<unsigned>(gainput::KeyCount_);
            state_ = manager.GetAllocator().New<gainput::InputState>(manager.GetAllocator(), count);
            previousState_ = manager.GetAllocator().New<gainput::InputState>(manager.GetAllocator(), count);
        }

        ~GlfwInputDevice() override {
            manager_.GetAllocator().Delete(state_);
            manager_.GetAllocator().Delete(previousState_);
        }

        [[nodiscard]] DeviceType GetType() const override { return mouse_ ? DT_MOUSE : DT_KEYBOARD; }
        // Avoid Gainput's native-event casts; GLFW delivers the events to this device instead.
        [[nodiscard]] DeviceVariant GetVariant() const override { return DV_NULL; }
        [[nodiscard]] const char* GetTypeName() const override { return mouse_ ? "mouse" : "keyboard"; }
        [[nodiscard]] bool IsValidButtonId(const gainput::DeviceButtonId button) const override {
            return button < (mouse_ ? static_cast<unsigned>(gainput::MouseButtonCount_)
                                    : static_cast<unsigned>(gainput::KeyCount_));
        }
        [[nodiscard]] gainput::ButtonType GetButtonType(const gainput::DeviceButtonId button) const override {
            return mouse_ && button >= gainput::MouseAxisX ? gainput::BT_FLOAT : gainput::BT_BOOL;
        }
        [[nodiscard]] size_t GetAnyButtonDown(gainput::DeviceButtonSpec* buttons, size_t max_count) const override {
            const unsigned end =
                mouse_ ? static_cast<unsigned>(gainput::MouseAxisX) : static_cast<unsigned>(gainput::KeyCount_);
            return CheckAllButtonsDown(buttons, max_count, 0, end);
        }

        void queue_button(const gainput::DeviceButtonId button, const bool pressed) {
            if (IsValidButtonId(button) && GetButtonType(button) == gainput::BT_BOOL)
                pending_.push_back({button, gainput::BT_BOOL, pressed, 0.0f});
        }

        void queue_axis(const gainput::DeviceButtonId axis, const float value) {
            if (IsValidButtonId(axis) && GetButtonType(axis) == gainput::BT_FLOAT)
                pending_.push_back({axis, gainput::BT_FLOAT, false, value});
        }

        void queue_pulse(const gainput::DeviceButtonId button) {
            // Wheel motion is a button transition lasting one Update, so queue
            // its matching release separately for the following frame.
            queue_button(button, true);
            pending_pulses_.push_back(button);
        }

        void reset() {
            // Detachment discards queued transitions and clears both snapshots;
            // reattaching must not replay a held key or pending wheel pulse.
            pending_.clear();
            release_next_frame_.clear();
            pending_pulses_.clear();
            const unsigned count = state_->GetButtonCount();
            for (unsigned button = 0; button < count; ++button) {
                if (GetButtonType(button) == gainput::BT_BOOL) {
                    state_->Set(button, false);
                    previousState_->Set(button, false);
                }
                else {
                    state_->Set(button, 0.0f);
                    previousState_->Set(button, 0.0f);
                }
            }
        }

    protected:
        void InternalUpdate(gainput::InputDeltaState* delta) override {
            // Release last frame's wheel pulses, then apply GLFW events queued since the previous poll.
            // New pulses stay pressed for this update and are released on the following update.
            auto releases = std::move(release_next_frame_);
            release_next_frame_ = std::move(pending_pulses_);
            pending_pulses_.clear();
            for (const auto button : releases)
                gainput::HandleButton(*this, *state_, delta, button, false);
            for (const auto& change : pending_) {
                if (change.type == gainput::BT_BOOL)
                    gainput::HandleButton(*this, *state_, delta, change.button, change.pressed);
                else
                    gainput::HandleAxis(*this, *state_, delta, change.button, change.value);
            }
            pending_.clear();
        }

        [[nodiscard]] DeviceState InternalGetState() const override { return DS_OK; }

    private:
        bool mouse_;
        struct Change {
            gainput::DeviceButtonId button;
            gainput::ButtonType type;
            bool pressed;
            float value;
        };
        std::vector<Change> pending_;
        std::vector<gainput::DeviceButtonId> release_next_frame_;
        std::vector<gainput::DeviceButtonId> pending_pulses_;
    };

    class GlfwKeyboardDevice final : public GlfwInputDevice {
    public:
        GlfwKeyboardDevice(gainput::InputManager& manager, gainput::DeviceId id, unsigned index,
                           DeviceVariant variant) : GlfwInputDevice(manager, id, index, variant, false) {}
    };

    class GlfwMouseDevice final : public GlfwInputDevice {
    public:
        GlfwMouseDevice(gainput::InputManager& manager, gainput::DeviceId id, unsigned index, DeviceVariant variant) :
            GlfwInputDevice(manager, id, index, variant, true) {}
    };

    InputSystem::InputSystem() : bindings_(manager_) {
    }

    InputSystem::~InputSystem() {
        deinitialize();
    }

    void InputSystem::initialize(iWindow& window) {
        // This adapter needs the GLFW-backed window; a different backend supplies its own input adapter.
        auto* glfw_window = dynamic_cast<Window*>(&window);
        if (!glfw_window)
            throw Exceptions::invalid_args(CE_HERE, "GLFW input requires a GLFW window");
        if (window_ && window_ != glfw_window)
            throw Exceptions::failed_operation(CE_HERE, "Input is already attached to another window");
        if (window_)
            return;

        // Create Gainput devices once; reinitialization attaches them to the current GLFW window.
        if (!keyboard_) {
            keyboard_ = manager_.CreateAndGetDevice<GlfwKeyboardDevice>();
            keyboard_id_ = keyboard_->GetDeviceId();
        }
        if (!mouse_) {
            mouse_ = manager_.CreateAndGetDevice<GlfwMouseDevice>();
            mouse_id_ = mouse_->GetDeviceId();
        }
        if (gamepad_id_ == gainput::InvalidDeviceId)
            gamepad_id_ = manager_.CreateDevice<gainput::InputDevicePad>();

        window_ = glfw_window;
        const auto size = window.logical_size();
        manager_.SetDisplaySize(std::max(size.width, 1), std::max(size.height, 1));
        // GLFW callbacks only queue transitions. Gainput processes them together in update().
        auto* handle = glfw_window->native_handle();
        glfwSetKeyCallback(handle, on_key);
        glfwSetMouseButtonCallback(handle, on_mouse_button);
        glfwSetCursorPosCallback(handle, on_cursor);
        glfwSetScrollCallback(handle, on_scroll);
    }

    void InputSystem::update() {
        if (!window_)
            throw Exceptions::failed_operation(CE_HERE, "Input must be initialized before updating");
        const auto size = window_->logical_size();
        // Refresh normalized pointer dimensions before Gainput consumes this
        // frame's queued GLFW changes and invokes any registered listeners.
        manager_.SetDisplaySize(std::max(size.width, 1), std::max(size.height, 1));
        manager_.Update();
    }

    void InputSystem::poll() {
        if (!window_)
            throw Exceptions::failed_operation(CE_HERE, "Input must be initialized before polling");
        // Pump GLFW on the platform thread before Gainput converts queued changes to listener calls.
        glfwPollEvents();
        update();
    }

    void InputSystem::deinitialize() {
        if (!window_)
            return;
        auto* handle = window_->native_handle();
        // Detach callbacks while the native window is still alive, then clear per-window input state.
        glfwSetKeyCallback(handle, nullptr);
        glfwSetMouseButtonCallback(handle, nullptr);
        glfwSetCursorPosCallback(handle, nullptr);
        glfwSetScrollCallback(handle, nullptr);
        window_ = nullptr;
        keyboard_->reset();
        mouse_->reset();
        bindings_.clear();
    }

    void InputSystem::on_key(GLFWwindow* handle, const int key, int, const int action, int) {
        // The singleton receives GLFW's global callback; ignore stale windows
        // and repeat events so button transitions match press/release edges.
        auto& input = get();
        if (!input.window_ || input.window_->native_handle() != handle || action == GLFW_REPEAT)
            return;
        input.keyboard_->queue_button(gainput_key(key), action == GLFW_PRESS);
    }

    void InputSystem::on_mouse_button(GLFWwindow* handle, const int button, const int action, int) {
        auto& input = get();
        if (!input.window_ || input.window_->native_handle() != handle)
            return;
        input.mouse_->queue_button(gainput_mouse_button(button), action == GLFW_PRESS);
    }

    void InputSystem::on_cursor(GLFWwindow* handle, const double x, const double y) {
        auto& input = get();
        if (!input.window_ || input.window_->native_handle() != handle)
            return;
        // Convert GLFW logical coordinates into normalized mouse axes, using
        // a nonzero divisor while the window is minimized.
        const auto size = input.window_->logical_size();
        input.mouse_->queue_axis(gainput::MouseAxisX, static_cast<float>(x / std::max(size.width, 1)));
        input.mouse_->queue_axis(gainput::MouseAxisY, static_cast<float>(y / std::max(size.height, 1)));
    }

    void InputSystem::on_scroll(GLFWwindow* handle, double, const double y) {
        auto& input = get();
        if (!input.window_ || input.window_->native_handle() != handle || y == 0.0)
            return;
        const auto button = y > 0.0 ? gainput::MouseButtonWheelUp : gainput::MouseButtonWheelDown;
        input.mouse_->queue_pulse(button);
    }
}
