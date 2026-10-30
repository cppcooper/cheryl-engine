#include <gtest/gtest.h>
#include <core/controls/input-system.h>
#include "backends/native-glfw/test-access.h"
#include "core/controls/gainput-lifetime.h"

#include <atomic>
#include <stdexcept>
#include <unordered_set>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

TEST(input_mapper, cross_device_chord_order) {
    CE::Platform::GLFW::InputSystem input;
    auto& bindings = input.bindings();
    CE::Platform::GLFW::Detail::InputTestAccess::use_external_state(input, 1);
    CE::Platform::GLFW::Detail::InputTestAccess::use_external_state(input, 2);
    const CE::Input::Actions::Id modified_click{1};
    const CE::Input::Actions::Id pad_action{2};
    (void)bindings.bind_button(CE::Input::Actions::Chord{{{1, 1}, {2, 1}}}, modified_click);
    (void)bindings.bind_button({3, CE::Input::GamepadControl::A}, pad_action);

    // The modifier really enclosed the click. Later per-device Gainput callbacks
    // must not replay this order as all keyboard changes followed by all mouse changes.
    bindings.on_button({1, 1}, true);
    bindings.on_button({2, 1}, true);
    bindings.on_button({2, 1}, false);
    bindings.on_button({1, 1}, false);
    (void)CE::Platform::GLFW::Detail::InputTestAccess::notify_button(input, 1, 1, false, true);
    (void)CE::Platform::GLFW::Detail::InputTestAccess::notify_button(input, 1, 1, true, false);
    (void)CE::Platform::GLFW::Detail::InputTestAccess::notify_button(input, 2, 1, false, true);
    (void)CE::Platform::GLFW::Detail::InputTestAccess::notify_button(input, 2, 1, true, false);
    // Gainput's A report (50) is normalized before reaching the engine mapper.
    (void)CE::Platform::GLFW::Detail::InputTestAccess::notify_button(input, 3, 50, false, true);
    const auto poll = bindings.publish_actions();
    EXPECT_EQ(poll->button(modified_click).press_count, 1u);
    EXPECT_EQ(poll->button(modified_click).release_count, 1u);
    EXPECT_FALSE(poll->button(modified_click).held());
    EXPECT_TRUE(poll->button(pad_action).pressed());
}

TEST(glfw_bindings, portable_keys) {
    using CE::Input::KeyboardKey;
    // Check range boundaries as well as editing, shortcuts and keypad identity.
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_A), KeyboardKey::A);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_Z), KeyboardKey::Z);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_0), KeyboardKey::Digit0);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_9), KeyboardKey::Digit9);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_F1), KeyboardKey::F1);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_F25), KeyboardKey::F25);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_KP_0), KeyboardKey::Keypad0);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_KP_9), KeyboardKey::Keypad9);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_ENTER), KeyboardKey::Enter);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_KP_ENTER), KeyboardKey::KeypadEnter);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_BACKSPACE), KeyboardKey::Backspace);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_LEFT), KeyboardKey::Left);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_LEFT_CONTROL), KeyboardKey::LeftControl);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_RIGHT_CONTROL), KeyboardKey::RightControl);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_GRAVE_ACCENT), KeyboardKey::GraveAccent);
    // All portable GLFW keys can bind even when the mirrored Gainput device lacks them.
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_F20), KeyboardKey::F20);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_UNKNOWN), KeyboardKey::Unknown);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_WORLD_1), KeyboardKey::Unknown);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_LAST + 1), KeyboardKey::Unknown);
}

TEST(glfw_bindings, portable_mouse) {
    using CE::Input::MouseButton;
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::mouse_button(GLFW_MOUSE_BUTTON_LEFT), MouseButton::Left);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::mouse_button(GLFW_MOUSE_BUTTON_RIGHT), MouseButton::Right);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::mouse_button(GLFW_MOUSE_BUTTON_MIDDLE), MouseButton::Middle);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::mouse_button(GLFW_MOUSE_BUTTON_4), MouseButton::Extra1);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::mouse_button(GLFW_MOUSE_BUTTON_8), MouseButton::Extra5);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::mouse_button(-1), MouseButton::Unknown);
    EXPECT_EQ(CE::Platform::GLFW::Detail::InputTestAccess::mouse_button(GLFW_MOUSE_BUTTON_LAST + 1), MouseButton::Unknown);
}

TEST(glfw_bindings, all_keys) {
    std::unordered_set<CE::Input::KeyboardKey> identities;
    for (int native = 0; native <= GLFW_KEY_LAST; ++native) {
        const auto key = CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(native);
        if (key != CE::Input::KeyboardKey::Unknown)
            EXPECT_TRUE(identities.insert(key).second);
    }
    EXPECT_EQ(identities.size(), static_cast<std::size_t>(CE::Input::KeyboardKey::Menu));
}

TEST(glfw_bindings, direct_controls) {
    using namespace CE::Input;
    constexpr Control keyboard{1, KeyboardKey::Escape};
    constexpr Control mouse{2, MouseButton::Left};
    constexpr Control gamepad{3, GamepadControl::A};
    EXPECT_EQ(keyboard.control, control_id(CE::Platform::GLFW::Detail::InputTestAccess::keyboard_key(GLFW_KEY_ESCAPE)));
    EXPECT_EQ(mouse.control, control_id(CE::Platform::GLFW::Detail::InputTestAccess::mouse_button(GLFW_MOUSE_BUTTON_LEFT)));
    EXPECT_NE(gamepad.control, keyboard.control);
    EXPECT_NE(mouse.control, CE::Input::MouseControl::WheelUp);
    EXPECT_NE(mouse.control, CE::Input::MouseControl::X);

    CE::Platform::GLFW::InputSystem input;
    const Actions::Id action{1};
    const Actions::Id throttle{2};
    static_cast<void>(input.bindings().bind_button(gamepad, action));
    static_cast<void>(input.bindings().bind_axis({gamepad.device, GamepadControl::LeftTrigger}, throttle));
    // Two reports within a poll must retain both edges after normalization.
    static_cast<void>(CE::Platform::GLFW::Detail::InputTestAccess::notify_button(input, gamepad.device, 50, false, true));
    static_cast<void>(CE::Platform::GLFW::Detail::InputTestAccess::notify_button(input, gamepad.device, 50, true, false));
    static_cast<void>(CE::Platform::GLFW::Detail::InputTestAccess::notify_axis(input, gamepad.device, 4, 0.0f, 0.75f));
    const auto sample = input.bindings().publish_actions();
    EXPECT_EQ(sample->button(action).press_count, 1u);
    EXPECT_EQ(sample->button(action).release_count, 1u);
    EXPECT_FALSE(sample->button(action).held());
    EXPECT_FLOAT_EQ(sample->axis(throttle).current, 0.75f);
}

TEST(input_lifetime, owner) {
    std::atomic_flag ownership = ATOMIC_FLAG_INIT;
    {
        CE::Platform::GLFW::Detail::GainputLifetime first(ownership, nullptr);
        EXPECT_THROW((void)CE::Platform::GLFW::Detail::GainputLifetime(ownership, nullptr), CE::Exceptions::failed_operation);
        // A rejected claimant must leave the existing owner in control.
        EXPECT_THROW((void)CE::Platform::GLFW::Detail::GainputLifetime(ownership, nullptr), CE::Exceptions::failed_operation);
        EXPECT_NO_THROW(first.require_window(nullptr));
    }
    EXPECT_NO_THROW((void)CE::Platform::GLFW::Detail::GainputLifetime(ownership, nullptr));
}

TEST(input_lifetime, failed_init) {
    std::atomic_flag ownership = ATOMIC_FLAG_INIT;
    const auto initialize = [&] {
        CE::Platform::GLFW::Detail::GainputLifetime lifetime(ownership, nullptr);
        throw std::runtime_error("initialization failed");
    };
    EXPECT_THROW(initialize(), std::runtime_error);
    EXPECT_NO_THROW((void)CE::Platform::GLFW::Detail::GainputLifetime(ownership, nullptr));
}

TEST(input_lifetime, window) {
    std::atomic_flag ownership = ATOMIC_FLAG_INIT;
    int first = 0;
    int second = 0;
    CE::Platform::GLFW::Detail::GainputLifetime lifetime(ownership, &first);
    EXPECT_NO_THROW(lifetime.require_window(&first));
    EXPECT_THROW(lifetime.require_window(&second), CE::Exceptions::failed_operation);
    EXPECT_NO_THROW(lifetime.require_window(&first));
    EXPECT_THROW((void)CE::Platform::GLFW::Detail::GainputLifetime(ownership, &second), CE::Exceptions::failed_operation);
}
