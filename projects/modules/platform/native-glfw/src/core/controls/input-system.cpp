#if CHERYL_NATIVE_TEST_SUPPORT
#define CHERYL_NATIVE_TEST_IMPLEMENTATION
#include "backends/native-glfw/test-access.h"
#endif
#include <core/controls/input-system.h>

#include "gainput-lifetime.h"
#include "gainput-mapper.h"
#include "native-controls.h"

#include <core/display/window.h>
#include <internals/exceptions.h>
#include <internals/failure-reporting.h>
#include <internals/compile-time-logging.hpp>

#include <gainput/GainputInputDeltaState.h>

// Gainput's inline helpers call methods on the complete delta-state type.
#include <gainput/GainputHelpers.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <utility>
#include <vector>

namespace CE::State {
    template <> auto Singleton<CE::Platform::GLFW::InputSystem>::storage() -> Storage& {
        static Storage value;
        return value;
    }
}

namespace {
    std::atomic_flag gainput_ownership = ATOMIC_FLAG_INIT;
#if defined(CONTROLLER_ID)
    constexpr gainput::DeviceId gainput_hid_report_device = CONTROLLER_ID;
    constexpr bool gainput_hid_report_device_known = true;
#else
    constexpr gainput::DeviceId gainput_hid_report_device = gainput::InvalidDeviceId;
    constexpr bool gainput_hid_report_device_known = false;
#endif

    // GLFW's window user pointer belongs to CE::Platform::GLFW::Window. Keep adapter association
    // separately so callbacks also work for explicitly constructed adapters.
    std::unordered_map<GLFWwindow*, CE::Platform::GLFW::InputSystem*> attached_inputs;

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

namespace CE::Platform::GLFW {
    namespace Detail {
        class InputDevice : public gainput::InputDevice {
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
            InputDevice(
                gainput::InputManager& manager,
                const gainput::DeviceId id,
                const unsigned index,
                const DeviceVariant,
                const bool mouse
            )
            : gainput::InputDevice(manager, id, index == AutoIndex ? manager.GetDeviceCountByType(mouse ? DT_MOUSE : DT_KEYBOARD) : index),
              mouse_(mouse) {
                // Keep current and previous Gainput state for delta generation;
                // GLFW callbacks enqueue changes instead of mutating either here.
                const unsigned count =
                    mouse_ ? static_cast<unsigned>(gainput::MouseButtonCount_) : static_cast<unsigned>(gainput::KeyCount_);
                state_ = manager.GetAllocator().New<gainput::InputState>(manager.GetAllocator(), count);
                previousState_ = manager.GetAllocator().New<gainput::InputState>(manager.GetAllocator(), count);
            }

            ~InputDevice() override {
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

        class GlfwKeyboardDevice final : public InputDevice {
        public:
            GlfwKeyboardDevice(gainput::InputManager& manager, gainput::DeviceId id, unsigned index, DeviceVariant variant)
            : InputDevice(manager, id, index, variant, false) {}
        };

        class GlfwMouseDevice final : public InputDevice {
        public:
            GlfwMouseDevice(gainput::InputManager& manager, gainput::DeviceId id, unsigned index, DeviceVariant variant)
            : InputDevice(manager, id, index, variant, true) {}
        };

    } // namespace Detail

    struct InputSystem::State {
        std::unique_ptr<Detail::GainputLifetime> gainput_lifetime_;
        gainput::InputManager manager_;
        Detail::GainputMapper bindings_{manager_};
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
    };

    InputSystem::InputSystem()
    : state_(std::make_unique<State>()) {}

    CE::Input::Actions::Bindings& InputSystem::bindings() {
        return state_->bindings_;
    }
    CE::Input::DeviceId InputSystem::keyboard_id() const {
        return state_->keyboard_id_;
    }
    CE::Input::DeviceId InputSystem::mouse_id() const {
        return state_->mouse_id_;
    }
    CE::Input::DeviceId InputSystem::gamepad_id() const {
        return state_->gamepad_id_;
    }
    DeviceSnapshot InputSystem::device(const CE::Input::DeviceId id) const {
        const auto* device = state_->manager_.GetDevice(id);
        return {static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(device)), device && device->IsAvailable()};
    }
    std::uint64_t InputSystem::backend_time_ms() const {
        return state_->manager_.GetTime();
    }

    void InputSystem::set_gamepad_diagnostics(const bool enabled) noexcept {
        state_->gamepad_diagnostics_ = enabled;
        state_->next_gamepad_diagnostics_ = {};
        state_->bindings_.set_gamepad_diagnostics(enabled);
    }

    InputSystem::~InputSystem() {
        close_gamepad_availability(false); // Destruction never publishes callbacks.
        // Explicit runtime teardown reports failures. Destruction must still
        // finish detachment without allowing a publication failure to escape.
        try {
            deinitialize();
        } catch (...) {
            Diagnostics::report_failure("input destruction", std::current_exception());
        }
        // Gainput's explicit Exit owns the delta buffer, HID support and devices.
        // Window detachment retains them for a later attachment to this adapter.
        if (state_->manager_initialized_)
            state_->manager_.Exit();
    }

    void InputSystem::initialize(CE::Platform::Window& window) {
        // This adapter needs the GLFW-backed window; a different backend supplies its own input adapter.
        auto* glfw_window = dynamic_cast<Window*>(&window);
        if (!glfw_window)
            throw Exceptions::invalid_args(CE_HERE, "GLFW input requires a GLFW window");
        if (state_->window_ && state_->window_ != glfw_window)
            throw Exceptions::failed_operation(CE_HERE, "Input is already attached to another window");
        if (state_->window_)
            return;

        auto* handle = glfw_window->native_handle();
        if (const auto found = attached_inputs.find(handle); found != attached_inputs.end() && found->second != this)
            throw Exceptions::failed_operation(CE_HERE, "The window already has an input adapter");
        open_gamepad_availability();
        try {
#if defined(_WIN32)
            void* gainput_window = glfwGetWin32Window(handle);
#else
            // GLFW owns keyboard/mouse delivery. Gainput's non-Windows HID
            // initialization does not consume a native window handle.
            void* gainput_window = nullptr;
#endif
            if (!state_->manager_initialized_) {
                // Release the claim if Init fails. Once initialized, retain it through
                // Exit and member destruction so no adapter can tear down another's HID.
                auto lifetime = std::make_unique<Detail::GainputLifetime>(gainput_ownership, gainput_window);
                CE_LOG_DEBUG(
                    CE::Logging::Categories::platform, "subsystem=input domain={} operation=gainput_init phase=begin", state_->domain_
                );
                state_->manager_.Init(gainput_window);
                CE_LOG_DEBUG(
                    CE::Logging::Categories::platform, "subsystem=input domain={} operation=gainput_init phase=returned", state_->domain_
                );
                state_->gainput_lifetime_ = std::move(lifetime);
                state_->manager_initialized_ = true;
            } else
                state_->gainput_lifetime_->require_window(gainput_window);

            // Create Gainput devices once; reinitialization attaches them to the current GLFW window.
            if (!state_->keyboard_) {
                state_->keyboard_ = state_->manager_.CreateAndGetDevice<Detail::GlfwKeyboardDevice>();
                state_->keyboard_id_ = state_->keyboard_->GetDeviceId();
            }
            if (!state_->mouse_) {
                state_->mouse_ = state_->manager_.CreateAndGetDevice<Detail::GlfwMouseDevice>();
                state_->mouse_id_ = state_->mouse_->GetDeviceId();
            }
            if (state_->gamepad_id_ == gainput::InvalidDeviceId)
                state_->gamepad_id_ = state_->manager_.CreateDevice<gainput::InputDevicePad>();
            CE_LOG_DEBUG(
                CE::Logging::Categories::platform,
                "subsystem=input domain={} operation=gamepad_setup device={} hid_report_device={} hid_report_device_known={} diagnostics={}",
                state_->domain_, state_->gamepad_id_, gainput_hid_report_device, gainput_hid_report_device_known,
                state_->gamepad_diagnostics_
            );
            // Map GLFW controls in their shared callback order. Gainput's per-device
            // queue order must not erase a keyboard-modified mouse tap in this poll.
            state_->bindings_.use_external_state(state_->keyboard_id_);
            state_->bindings_.use_external_state(state_->mouse_id_);

            attached_inputs.emplace(handle, this);
            state_->window_ = glfw_window;
            state_->pad_axes_.assign(gainput::PadButtonMax_, 0.0f);
            state_->pad_buttons_.assign(gainput::PadButtonMax_, false);
            state_->next_gamepad_diagnostics_ = {};
            glfwGetCursorPos(handle, &state_->cursor_x_, &state_->cursor_y_);
            const auto size = window.logical_size();
            state_->manager_.SetDisplaySize(std::max(size.width, 1), std::max(size.height, 1));
            // GLFW callbacks only queue transitions. Gainput processes them together in update().
            glfwSetInputMode(handle, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
            glfwSetCharCallback(handle, on_character);
            glfwSetCursorPosCallback(handle, on_cursor);
            glfwSetKeyCallback(handle, on_key);
            glfwSetMouseButtonCallback(handle, on_mouse_button);
            glfwSetScrollCallback(handle, on_scroll);
            state_->last_update_ = CE::Input::ObservationClock::now();
            state_->observed_window_focus_ = glfwGetWindowAttrib(handle, GLFW_FOCUSED) == GLFW_TRUE;
            CE_LOG_DEBUG(
                CE::Logging::Categories::platform,
                "subsystem=input domain={} window={} operation=attach keyboard={} mouse={} gamepad={} focused={}", state_->domain_,
                state_->window_->diagnostic_id(), state_->keyboard_id_, state_->mouse_id_, state_->gamepad_id_,
                state_->observed_window_focus_
            );
        } catch (...) {
            close_gamepad_availability();
            throw;
        }
    }

    void InputSystem::update() {
        if (!state_->window_)
            throw Exceptions::failed_operation(CE_HERE, "Input must be initialized before updating");
        state_->window_->check_native_failure();
        if (state_->callback_failure_) {
            Diagnostics::report_outcome("input", state_->domain_, "native_callback", "failed");
            std::rethrow_exception(std::exchange(state_->callback_failure_, {}));
        }
        const auto focused = glfwGetWindowAttrib(state_->window_->native_handle(), GLFW_FOCUSED) == GLFW_TRUE;
        if (focused != state_->observed_window_focus_) {
            state_->observed_window_focus_ = focused;
            CE_LOG_DEBUG(
                CE::Logging::Categories::platform, "subsystem=input domain={} operation=window_focus focused={}", state_->domain_, focused
            );
        }
        const auto size = state_->window_->logical_size();
        const auto width = std::max(size.width, 1);
        const auto height = std::max(size.height, 1);
        state_->manager_.SetDisplaySize(width, height);
        // An absolute pointer axis needs only the final cursor position. Sampling it here
        // also updates normalized coordinates when the window resized without a move event.
        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(state_->window_->native_handle(), &x, &y);
        state_->mouse_->queue_axis(gainput::MouseAxisX, static_cast<float>(x / width));
        state_->mouse_->queue_axis(gainput::MouseAxisY, static_cast<float>(y / height));
        state_->bindings_.on_axis({state_->mouse_id_, CE::Input::MouseControl::X}, static_cast<float>(x / width));
        state_->bindings_.on_axis({state_->mouse_id_, CE::Input::MouseControl::Y}, static_cast<float>(y / height));
        const auto updated_at = CE::Input::ObservationClock::now();
        const auto delta_time = std::chrono::duration<float>(updated_at - state_->last_update_).count();
        state_->last_update_ = updated_at;
        state_->manager_.Update(delta_time);
        // Gainput only notifies changes. Reconcile the pad's full state so a held button
        // survives reattachment and a disconnected pad cannot leave an action stuck.
        const auto* pad = state_->manager_.GetDevice(state_->gamepad_id_);
        const bool available = pad && pad->IsAvailable();
        if (available != state_->observed_gamepad_available_) {
            state_->observed_gamepad_available_ = available;
            CE_LOG_DEBUG(
                CE::Logging::Categories::platform, "subsystem=input domain={} operation=device_availability device={} available={}",
                state_->domain_, state_->gamepad_id_, available
            );
        }
        for (gainput::DeviceButtonId button = 0; button < gainput::PadButtonMax_; ++button) {
            const auto control = Detail::gamepad_control(button);
            if (control == CE::Input::invalid_control)
                continue;
            const bool valid = available && pad->IsValidButtonId(button);
            if (button < gainput::PadButtonStart) {
                const float value = valid ? pad->GetFloat(button) : 0.0f;
                if (value != state_->pad_axes_[button]) {
                    if (state_->gamepad_diagnostics_)
                        CE_LOG_TRACE(
                            CE::Logging::Categories::platform,
                            "subsystem=input domain={} operation=gamepad_sample kind=axis device={} control={} old={} new={}",
                            state_->domain_, state_->gamepad_id_, button, state_->pad_axes_[button], value
                        );
                    capture_buffer().record(state_->gamepad_id_, CE::Input::DeviceKind::Gamepad, CE::Input::Events::Axis{control, value});
                }
                state_->pad_axes_[button] = value;
                state_->bindings_.on_axis({state_->gamepad_id_, control}, value);
            } else {
                const bool held = valid && pad->GetBool(button);
                if (held != state_->pad_buttons_[button]) {
                    if (state_->gamepad_diagnostics_)
                        CE_LOG_TRACE(
                            CE::Logging::Categories::platform,
                            "subsystem=input domain={} operation=gamepad_sample kind=button device={} control={} old={} new={}",
                            state_->domain_, state_->gamepad_id_, button, static_cast<bool>(state_->pad_buttons_[button]), held
                        );
                    capture_buffer().record(
                        state_->gamepad_id_, CE::Input::DeviceKind::Gamepad,
                        CE::Input::Events::Button{
                            control, held ? CE::Input::Events::ButtonPhase::Press : CE::Input::Events::ButtonPhase::Release
                        }
                    );
                }
                state_->pad_buttons_[button] = held;
                state_->bindings_.on_button({state_->gamepad_id_, control}, held);
            }
        }
        if (state_->gamepad_diagnostics_ && (state_->next_gamepad_diagnostics_ == CE::Input::ObservationClock::time_point{} ||
                                             updated_at >= state_->next_gamepad_diagnostics_)) {
            state_->next_gamepad_diagnostics_ = updated_at + std::chrono::seconds(1);
            CE_LOG_TRACE(
                CE::Logging::Categories::platform,
                "subsystem=input domain={} operation=gamepad_poll device={} state={} available={} a_valid={} a_sampled={}", state_->domain_,
                state_->gamepad_id_, pad ? static_cast<int>(pad->GetState()) : -1, available,
                pad && pad->IsValidButtonId(gainput::PadButtonA), static_cast<bool>(state_->pad_buttons_[gainput::PadButtonA])
            );
        }
        // Listeners have now updated pending physical state. Commit the complete semantic sample.
        stage_gamepad_availability(state_->gamepad_id_, available);
        (void)publish_input();
    }

    void InputSystem::begin_poll() {
        if (!state_->window_)
            throw Exceptions::failed_operation(CE_HERE, "Input must be initialized before beginning a poll");
        begin_input_poll();
        // Wheel buttons are one-poll State pulses; actual scroll records
        // and relative offsets retain all delivered movement independently.
        state_->bindings_.on_button({state_->mouse_id_, CE::Input::MouseControl::WheelUp}, false);
        state_->bindings_.on_button({state_->mouse_id_, CE::Input::MouseControl::WheelDown}, false);
    }

    void InputSystem::poll() {
        if (!state_->window_)
            throw Exceptions::failed_operation(CE_HERE, "Input must be initialized before polling");
        // Pump GLFW on the platform thread before Gainput converts queued changes to listener calls.
        begin_poll();
        glfwPollEvents();
        update();
    }

    void InputSystem::deinitialize() {
        if (state_->callback_failure_)
            Diagnostics::report_failure("unconsumed input callback", std::exchange(state_->callback_failure_, {}));
        if (!state_->window_) {
            discard_captured_input();
            state_->callback_failure_ = {};
            return;
        }
        auto* handle = state_->window_->native_handle();
        // Detach callbacks while the native window is still alive, then clear per-window input state.
        glfwSetKeyCallback(handle, nullptr);
        glfwSetMouseButtonCallback(handle, nullptr);
        glfwSetScrollCallback(handle, nullptr);
        glfwSetCharCallback(handle, nullptr);
        glfwSetCursorPosCallback(handle, nullptr);
        attached_inputs.erase(handle);
        CE_LOG_DEBUG(
            CE::Logging::Categories::platform, "subsystem=input domain={} window={} operation=detach", state_->domain_,
            state_->window_->diagnostic_id()
        );
        state_->window_ = nullptr;
        state_->keyboard_->reset();
        state_->mouse_->reset();
        // Release routing/capture first: the final State publication may allocate
        // and fail, but callbacks, focus, and pending records must already be detached.
        discard_captured_input();
        state_->pad_axes_.clear();
        state_->pad_buttons_.clear();
        state_->callback_failure_ = {};
        state_->bindings_.clear();
    }

    InputSystem* InputSystem::attached(GLFWwindow* handle) noexcept {
        const auto found = attached_inputs.find(handle);
        return found == attached_inputs.end() ? nullptr : found->second;
    }

    void InputSystem::on_key(GLFWwindow* handle, const int key, const int scancode, const int action, const int modifiers) noexcept {
        if (auto* input = attached(handle))
            input->state_->receive([&] {
                const auto phase = action == GLFW_REPEAT  ? CE::Input::Events::ButtonPhase::Repeat
                                   : action == GLFW_PRESS ? CE::Input::Events::ButtonPhase::Press
                                                          : CE::Input::Events::ButtonPhase::Release;
                const auto identity = Detail::keyboard_key(key);
                const auto button = CE::Input::control_id(identity);
                // Preserve delivered callbacks before State ignores repeats or condenses transitions.
                input->capture_buffer().record(
                    input->state_->keyboard_id_, CE::Input::DeviceKind::Keyboard,
                    CE::Input::Events::Button{button, phase, input_modifiers(modifiers), key, scancode, identity}
                );
                if (action != GLFW_REPEAT && button != gainput::InvalidDeviceButtonId) {
                    input->state_->bindings_.on_button({input->state_->keyboard_id_, button}, action == GLFW_PRESS);
                    input->state_->keyboard_->queue_button(Detail::gainput_keyboard_control(key), action == GLFW_PRESS);
                }
            });
    }

    void InputSystem::on_mouse_button(GLFWwindow* handle, const int button, const int action, const int modifiers) noexcept {
        if (auto* input = attached(handle))
            input->state_->receive([&] {
                const auto identity = Detail::mouse_button(button);
                const auto control = CE::Input::control_id(identity);
                double x = 0.0;
                double y = 0.0;
                glfwGetCursorPos(handle, &x, &y);
                input->capture_buffer().record(
                    input->state_->mouse_id_, CE::Input::DeviceKind::Mouse,
                    CE::Input::Events::Button{
                        control, action == GLFW_PRESS ? CE::Input::Events::ButtonPhase::Press : CE::Input::Events::ButtonPhase::Release,
                        input_modifiers(modifiers), button, -1, CE::Input::KeyboardKey::Unknown, identity, CE::Input::Events::Pointer{x, y}
                    }
                );
                input->state_->mouse_->queue_button(Detail::gainput_mouse_control(button), action == GLFW_PRESS);
                input->state_->bindings_.on_button({input->state_->mouse_id_, control}, action == GLFW_PRESS);
            });
    }

    void InputSystem::on_scroll(GLFWwindow* handle, const double x, const double y) noexcept {
        if (auto* input = attached(handle))
            input->state_->receive([&] {
                double pointer_x = 0.0;
                double pointer_y = 0.0;
                glfwGetCursorPos(handle, &pointer_x, &pointer_y);
                input->capture_buffer().record(
                    input->state_->mouse_id_, CE::Input::DeviceKind::Mouse,
                    CE::Input::Events::Scroll{x, y, CE::Input::Events::Pointer{pointer_x, pointer_y}}
                );
                input->state_->bindings_.on_delta({input->state_->mouse_id_, CE::Input::MouseControl::ScrollX}, static_cast<float>(x));
                input->state_->bindings_.on_delta({input->state_->mouse_id_, CE::Input::MouseControl::ScrollY}, static_cast<float>(y));
                if (y != 0.0) {
                    const auto button = y > 0.0 ? CE::Input::MouseControl::WheelUp : CE::Input::MouseControl::WheelDown;
                    input->state_->bindings_.on_button({input->state_->mouse_id_, button}, true);
                    input->state_->mouse_->queue_pulse(y > 0.0 ? gainput::MouseButtonWheelUp : gainput::MouseButtonWheelDown);
                }
            });
    }

    void InputSystem::on_character(GLFWwindow* handle, const unsigned int codepoint) noexcept {
        if (auto* input = attached(handle))
            input->state_->receive([&] {
                input->capture_buffer().record(
                    input->state_->keyboard_id_, CE::Input::DeviceKind::Keyboard, CE::Input::Events::Text{static_cast<char32_t>(codepoint)}
                );
            });
    }

    void InputSystem::on_cursor(GLFWwindow* handle, const double x, const double y) noexcept {
        if (auto* input = attached(handle))
            input->state_->receive([&] {
                input->capture_buffer().record(input->state_->mouse_id_, CE::Input::DeviceKind::Mouse, CE::Input::Events::Pointer{x, y});
                input->state_->bindings_.on_delta(
                    {input->state_->mouse_id_, CE::Input::MouseControl::DeltaX}, static_cast<float>(x - input->state_->cursor_x_)
                );
                input->state_->bindings_.on_delta(
                    {input->state_->mouse_id_, CE::Input::MouseControl::DeltaY}, static_cast<float>(y - input->state_->cursor_y_)
                );
                input->state_->cursor_x_ = x;
                input->state_->cursor_y_ = y;
            });
    }
}

#if CHERYL_NATIVE_TEST_SUPPORT
namespace CE::Platform::GLFW::Detail {
    CE::Input::KeyboardKey InputTestAccess::keyboard_key(const int key) {
        return CE::Platform::GLFW::Detail::keyboard_key(key);
    }
    CE::Input::MouseButton InputTestAccess::mouse_button(const int button) {
        return CE::Platform::GLFW::Detail::mouse_button(button);
    }
    void InputTestAccess::use_external_state(InputSystem& input, const CE::Input::DeviceId device) {
        input.state_->bindings_.use_external_state(device);
    }
    bool InputTestAccess::notify_button(
        InputSystem& input,
        const CE::Input::DeviceId device,
        const CE::Input::ControlId control,
        const bool old_value,
        const bool new_value
    ) {
        return input.state_->bindings_.OnDeviceButtonBool(device, control, old_value, new_value);
    }
    bool InputTestAccess::notify_axis(
        InputSystem& input,
        const CE::Input::DeviceId device,
        const CE::Input::ControlId control,
        const float old_value,
        const float new_value
    ) {
        return input.state_->bindings_.OnDeviceButtonFloat(0.0f, device, control, old_value, new_value);
    }
}
#endif
