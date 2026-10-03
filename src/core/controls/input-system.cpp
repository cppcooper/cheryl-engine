#include <core/controls/input-system.h>

#include <core/controls/glfw-bindings.h>
#include <core/display/window.h>
#include <internals/exceptions.h>
#include <internals/failure-reporting.h>

#include <gainput/GainputInputDeltaState.h>

// Gainput's inline helpers call methods on the complete delta-state type.
#include <gainput/GainputHelpers.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
    // GLFW's window user pointer belongs to Window. Keep adapter association
    // separately so callbacks also work for explicitly constructed adapters.
    std::unordered_map<GLFWwindow*, CE::Input::InputSystem*> attached_inputs;

    CE::Input::Modifiers input_modifiers(const int modifiers) {
        auto result = CE::Input::Modifiers::None;
        if (modifiers & GLFW_MOD_SHIFT)
            result = result | CE::Input::Modifiers::Shift;
        if (modifiers & GLFW_MOD_CONTROL)
            result = result | CE::Input::Modifiers::Control;
        if (modifiers & GLFW_MOD_ALT)
            result = result | CE::Input::Modifiers::Alt;
        if (modifiers & GLFW_MOD_SUPER)
            result = result | CE::Input::Modifiers::Super;
        if (modifiers & GLFW_MOD_CAPS_LOCK)
            result = result | CE::Input::Modifiers::CapsLock;
        if (modifiers & GLFW_MOD_NUM_LOCK)
            result = result | CE::Input::Modifiers::NumLock;
        return result;
    }
}

namespace CE::Input {
    class GlfwInputDevice : public gainput::InputDevice {
        struct Change {
            gainput::DeviceButtonId button;
            gainput::ButtonType type;
            bool pressed;
            float value;
        };

        bool mouse_;
        std::vector<Change> pending_;
        std::vector<gainput::DeviceButtonId> release_next_frame_;
        std::vector<gainput::DeviceButtonId> pending_pulses_;

    public:
        GlfwInputDevice(
            gainput::InputManager& manager,
            const gainput::DeviceId id,
            const unsigned index,
            const DeviceVariant,
            const bool mouse
        )
        : InputDevice(manager, id, index == AutoIndex ? manager.GetDeviceCountByType(mouse ? DT_MOUSE : DT_KEYBOARD) : index),
          mouse_(mouse) {
            // Keep current and previous Gainput state for delta generation;
            // GLFW callbacks enqueue changes instead of mutating either here.
            const unsigned count = mouse_ ? static_cast<unsigned>(gainput::MouseButtonCount_) : static_cast<unsigned>(gainput::KeyCount_);
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
            return button < (mouse_ ? static_cast<unsigned>(gainput::MouseButtonCount_) : static_cast<unsigned>(gainput::KeyCount_));
        }

        [[nodiscard]] gainput::ButtonType GetButtonType(const gainput::DeviceButtonId button) const override {
            return mouse_ && button >= gainput::MouseAxisX ? gainput::BT_FLOAT : gainput::BT_BOOL;
        }

        [[nodiscard]] size_t GetAnyButtonDown(gainput::DeviceButtonSpec* buttons, size_t max_count) const override {
            const unsigned end = mouse_ ? static_cast<unsigned>(gainput::MouseAxisX) : static_cast<unsigned>(gainput::KeyCount_);
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
            if (!IsValidButtonId(button) || GetButtonType(button) != gainput::BT_BOOL)
                return;
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
                } else {
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
    };

    class GlfwKeyboardDevice final : public GlfwInputDevice {
    public:
        GlfwKeyboardDevice(gainput::InputManager& manager, gainput::DeviceId id, unsigned index, DeviceVariant variant)
        : GlfwInputDevice(manager, id, index, variant, false) {}
    };

    class GlfwMouseDevice final : public GlfwInputDevice {
    public:
        GlfwMouseDevice(gainput::InputManager& manager, gainput::DeviceId id, unsigned index, DeviceVariant variant)
        : GlfwInputDevice(manager, id, index, variant, true) {}
    };

    InputSystem::InputSystem()
    : bindings_(manager_) {}

    InputSystem::~InputSystem() {
        // Explicit runtime teardown reports failures. Destruction must still
        // finish detachment without allowing a publication failure to escape.
        try {
            deinitialize();
        } catch (...) {
            Diagnostics::report_failure("input destruction", std::current_exception());
        }
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
        // Map GLFW controls in their shared callback order. Gainput's per-device
        // queue order must not erase a keyboard-modified mouse tap in this poll.
        bindings_.use_external_state(keyboard_id_);
        bindings_.use_external_state(mouse_id_);

        auto* handle = glfw_window->native_handle();
        if (const auto found = attached_inputs.find(handle); found != attached_inputs.end() && found->second != this)
            throw Exceptions::failed_operation(CE_HERE, "The window already has an input adapter");
        attached_inputs.emplace(handle, this);
        window_ = glfw_window;
        pad_axes_.assign(gainput::PadButtonMax_, 0.0f);
        pad_buttons_.assign(gainput::PadButtonMax_, false);
        glfwGetCursorPos(handle, &cursor_x_, &cursor_y_);
        const auto size = window.logical_size();
        manager_.SetDisplaySize(std::max(size.width, 1), std::max(size.height, 1));
        // GLFW callbacks only queue transitions. Gainput processes them together in update().
        glfwSetInputMode(handle, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
        glfwSetCharCallback(handle, on_character);
        glfwSetCursorPosCallback(handle, on_cursor);
        glfwSetKeyCallback(handle, on_key);
        glfwSetMouseButtonCallback(handle, on_mouse_button);
        glfwSetScrollCallback(handle, on_scroll);
    }

    void InputSystem::update() {
        if (!window_)
            throw Exceptions::failed_operation(CE_HERE, "Input must be initialized before updating");
        window_->check_native_failure();
        if (callback_failure_)
            std::rethrow_exception(std::exchange(callback_failure_, {}));
        const auto size = window_->logical_size();
        const auto width = std::max(size.width, 1);
        const auto height = std::max(size.height, 1);
        manager_.SetDisplaySize(width, height);
        // An absolute pointer axis needs only the final cursor position. Sampling it here
        // also updates normalized coordinates when the window resized without a move event.
        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(window_->native_handle(), &x, &y);
        mouse_->queue_axis(gainput::MouseAxisX, static_cast<float>(x / width));
        mouse_->queue_axis(gainput::MouseAxisY, static_cast<float>(y / height));
        bindings_.on_axis({mouse_id_, gainput::MouseAxisX}, static_cast<float>(x / width));
        bindings_.on_axis({mouse_id_, gainput::MouseAxisY}, static_cast<float>(y / height));
        manager_.Update();
        // Gainput only notifies changes. Reconcile the pad's full state so a held button
        // survives reattachment and a disconnected pad cannot leave an action stuck.
        const auto* pad = manager_.GetDevice(gamepad_id_);
        const bool available = pad && pad->IsAvailable();
        for (gainput::DeviceButtonId button = 0; button < gainput::PadButtonMax_; ++button) {
            const bool valid = available && pad->IsValidButtonId(button);
            if (button < gainput::PadButtonStart) {
                const float value = valid ? pad->GetFloat(button) : 0.0f;
                if (value != pad_axes_[button])
                    capture_buffer().record(gamepad_id_, DeviceKind::Gamepad, AxisEvent{button, value});
                pad_axes_[button] = value;
                bindings_.on_axis({gamepad_id_, button}, value);
            } else {
                const bool held = valid && pad->GetBool(button);
                if (held != pad_buttons_[button])
                    capture_buffer().record(
                        gamepad_id_, DeviceKind::Gamepad, ButtonEvent{button, held ? ButtonPhase::Press : ButtonPhase::Release}
                    );
                pad_buttons_[button] = held;
                bindings_.on_button({gamepad_id_, button}, held);
            }
        }
        // Listeners have now updated pending physical state. Commit the complete semantic sample.
        (void)publish_input();
    }

    void InputSystem::begin_poll() {
        if (!window_)
            throw Exceptions::failed_operation(CE_HERE, "Input must be initialized before beginning a poll");
        begin_input_poll();
        // Legacy wheel buttons are one-poll State pulses; actual scroll records
        // and relative offsets retain all delivered movement independently.
        bindings_.on_button({mouse_id_, gainput::MouseButtonWheelUp}, false);
        bindings_.on_button({mouse_id_, gainput::MouseButtonWheelDown}, false);
    }

    void InputSystem::poll() {
        if (!window_)
            throw Exceptions::failed_operation(CE_HERE, "Input must be initialized before polling");
        // Pump GLFW on the platform thread before Gainput converts queued changes to listener calls.
        begin_poll();
        glfwPollEvents();
        update();
    }

    void InputSystem::deinitialize() {
        if (!window_) {
            discard_captured_input();
            callback_failure_ = {};
            return;
        }
        auto* handle = window_->native_handle();
        // Detach callbacks while the native window is still alive, then clear per-window input state.
        glfwSetKeyCallback(handle, nullptr);
        glfwSetMouseButtonCallback(handle, nullptr);
        glfwSetScrollCallback(handle, nullptr);
        glfwSetCharCallback(handle, nullptr);
        glfwSetCursorPosCallback(handle, nullptr);
        attached_inputs.erase(handle);
        window_ = nullptr;
        keyboard_->reset();
        mouse_->reset();
        // Release routing/capture first: the final State publication may allocate
        // and fail, but callbacks, focus, and pending records must already be detached.
        discard_captured_input();
        pad_axes_.clear();
        pad_buttons_.clear();
        callback_failure_ = {};
        bindings_.clear();
    }

    InputSystem* InputSystem::attached(GLFWwindow* handle) {
        const auto found = attached_inputs.find(handle);
        return found == attached_inputs.end() ? nullptr : found->second;
    }

    void InputSystem::on_key(GLFWwindow* handle, const int key, const int scancode, const int action, const int modifiers) {
        if (auto* input = attached(handle))
            input->receive([&] {
                const auto phase = action == GLFW_REPEAT  ? ButtonPhase::Repeat
                                   : action == GLFW_PRESS ? ButtonPhase::Press
                                                          : ButtonPhase::Release;
                const auto button = gainput_key(key);
                // Preserve delivered callbacks before State ignores repeats or condenses transitions.
                input->capture_buffer().record(
                    input->keyboard_id_, DeviceKind::Keyboard, ButtonEvent{button, phase, input_modifiers(modifiers), key, scancode}
                );
                if (action != GLFW_REPEAT && button != gainput::InvalidDeviceButtonId) {
                    input->bindings_.on_button({input->keyboard_id_, button}, action == GLFW_PRESS);
                    input->keyboard_->queue_button(button, action == GLFW_PRESS);
                }
            });
    }

    void InputSystem::on_mouse_button(GLFWwindow* handle, const int button, const int action, const int modifiers) {
        if (auto* input = attached(handle))
            input->receive([&] {
                const auto control = gainput_mouse_button(button);
                input->capture_buffer().record(
                    input->mouse_id_, DeviceKind::Mouse,
                    ButtonEvent{
                        control, action == GLFW_PRESS ? ButtonPhase::Press : ButtonPhase::Release, input_modifiers(modifiers), button}
                );
                input->mouse_->queue_button(control, action == GLFW_PRESS);
                input->bindings_.on_button({input->mouse_id_, control}, action == GLFW_PRESS);
            });
    }

    void InputSystem::on_scroll(GLFWwindow* handle, const double x, const double y) {
        if (auto* input = attached(handle))
            input->receive([&] {
                input->capture_buffer().record(input->mouse_id_, DeviceKind::Mouse, ScrollEvent{x, y});
                input->bindings_.on_delta({input->mouse_id_, MouseControl::ScrollX}, static_cast<float>(x));
                input->bindings_.on_delta({input->mouse_id_, MouseControl::ScrollY}, static_cast<float>(y));
                if (y != 0.0) {
                    input->bindings_.on_button(
                        {input->mouse_id_, y > 0.0 ? gainput::MouseButtonWheelUp : gainput::MouseButtonWheelDown}, true
                    );
                    input->mouse_->queue_pulse(y > 0.0 ? gainput::MouseButtonWheelUp : gainput::MouseButtonWheelDown);
                }
            });
    }

    void InputSystem::on_character(GLFWwindow* handle, const unsigned int codepoint) {
        if (auto* input = attached(handle))
            input->receive([&] {
                input->capture_buffer().record(input->keyboard_id_, DeviceKind::Keyboard, TextEvent{static_cast<char32_t>(codepoint)});
            });
    }

    void InputSystem::on_cursor(GLFWwindow* handle, const double x, const double y) {
        if (auto* input = attached(handle))
            input->receive([&] {
                input->capture_buffer().record(input->mouse_id_, DeviceKind::Mouse, PointerEvent{x, y});
                input->bindings_.on_delta({input->mouse_id_, MouseControl::DeltaX}, static_cast<float>(x - input->cursor_x_));
                input->bindings_.on_delta({input->mouse_id_, MouseControl::DeltaY}, static_cast<float>(y - input->cursor_y_));
                input->cursor_x_ = x;
                input->cursor_y_ = y;
            });
    }
}
