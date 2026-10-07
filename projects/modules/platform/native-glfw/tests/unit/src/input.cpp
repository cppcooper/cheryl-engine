#include <gtest/gtest.h>
#include <core/controls/glfw-bindings.h>
#include <core/controls/input-mapper.h>
#include "core/controls/gainput-lifetime.h"

#include <atomic>
#include <stdexcept>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

TEST(input_mapper, cross_device_chord_order) {
    gainput::InputManager manager;
    CE::Input::InputMapper bindings(manager);
    bindings.use_external_state(1);
    bindings.use_external_state(2);
    const CE::Input::ActionId modified_click{1};
    const CE::Input::ActionId pad_action{2};
    (void)bindings.bind_button(CE::Input::InputChord{{{1, 1}, {2, 1}}}, modified_click);
    (void)bindings.bind_button({3, 1}, pad_action);

    // The modifier really enclosed the click. Later per-device Gainput callbacks
    // must not replay this order as all keyboard changes followed by all mouse changes.
    bindings.on_button({1, 1}, true);
    bindings.on_button({2, 1}, true);
    bindings.on_button({2, 1}, false);
    bindings.on_button({1, 1}, false);
    (void)bindings.OnDeviceButtonBool(1, 1, false, true);
    (void)bindings.OnDeviceButtonBool(1, 1, true, false);
    (void)bindings.OnDeviceButtonBool(2, 1, false, true);
    (void)bindings.OnDeviceButtonBool(2, 1, true, false);
    (void)bindings.OnDeviceButtonBool(3, 1, false, true); // A Gainput-owned pad still maps normally.
    const auto poll = bindings.publish_actions();
    EXPECT_EQ(poll->button(modified_click).press_count, 1u);
    EXPECT_EQ(poll->button(modified_click).release_count, 1u);
    EXPECT_FALSE(poll->button(modified_click).held());
    EXPECT_TRUE(poll->button(pad_action).pressed());
}

TEST(glfw_bindings, key_and_mouse_mapping) {
    // Cover ordinary keys, modifiers, keypad keys, and an unsupported key.
    EXPECT_EQ(CE::Input::gainput_key(GLFW_KEY_A), gainput::KeyA);
    EXPECT_EQ(CE::Input::gainput_key(GLFW_KEY_8), gainput::Key8);
    EXPECT_EQ(CE::Input::gainput_key(GLFW_KEY_ESCAPE), gainput::KeyEscape);
    EXPECT_EQ(CE::Input::gainput_key(GLFW_KEY_F12), gainput::KeyF12);
    EXPECT_EQ(CE::Input::gainput_key(GLFW_KEY_KP_5), gainput::KeyKpBegin);
    EXPECT_EQ(CE::Input::gainput_key(GLFW_KEY_RIGHT_CONTROL), gainput::KeyCtrlR);
    EXPECT_EQ(CE::Input::gainput_key(GLFW_KEY_F20), gainput::InvalidDeviceButtonId);

    // Mouse buttons use a separate translation table, including a rejected
    // out-of-range button.
    EXPECT_EQ(CE::Input::gainput_mouse_button(GLFW_MOUSE_BUTTON_LEFT), gainput::MouseButtonLeft);
    EXPECT_EQ(CE::Input::gainput_mouse_button(GLFW_MOUSE_BUTTON_RIGHT), gainput::MouseButtonRight);
    EXPECT_EQ(CE::Input::gainput_mouse_button(GLFW_MOUSE_BUTTON_MIDDLE), gainput::MouseButtonMiddle);
    EXPECT_EQ(CE::Input::gainput_mouse_button(GLFW_MOUSE_BUTTON_4), gainput::MouseButton5);
    EXPECT_EQ(CE::Input::gainput_mouse_button(-1), gainput::InvalidDeviceButtonId);
}

TEST(glfw_bindings, portable_keys) {
    using CE::Input::KeyboardKey;
    // Check range boundaries as well as editing, shortcuts and keypad identity.
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_A), KeyboardKey::A);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_Z), KeyboardKey::Z);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_0), KeyboardKey::Digit0);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_9), KeyboardKey::Digit9);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_F1), KeyboardKey::F1);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_F25), KeyboardKey::F25);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_KP_0), KeyboardKey::Keypad0);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_KP_9), KeyboardKey::Keypad9);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_ENTER), KeyboardKey::Enter);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_KP_ENTER), KeyboardKey::KeypadEnter);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_BACKSPACE), KeyboardKey::Backspace);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_LEFT), KeyboardKey::Left);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_LEFT_CONTROL), KeyboardKey::LeftControl);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_RIGHT_CONTROL), KeyboardKey::RightControl);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_GRAVE_ACCENT), KeyboardKey::GraveAccent);
    // A portable record may recognize a key that has no State binding token.
    EXPECT_EQ(CE::Input::gainput_key(GLFW_KEY_F20), gainput::InvalidDeviceButtonId);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_F20), KeyboardKey::F20);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_UNKNOWN), KeyboardKey::Unknown);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_WORLD_1), KeyboardKey::Unknown);
    EXPECT_EQ(CE::Input::keyboard_key(GLFW_KEY_LAST + 1), KeyboardKey::Unknown);
}

TEST(glfw_bindings, portable_mouse) {
    using CE::Input::MouseButton;
    EXPECT_EQ(CE::Input::mouse_button(GLFW_MOUSE_BUTTON_LEFT), MouseButton::Left);
    EXPECT_EQ(CE::Input::mouse_button(GLFW_MOUSE_BUTTON_RIGHT), MouseButton::Right);
    EXPECT_EQ(CE::Input::mouse_button(GLFW_MOUSE_BUTTON_MIDDLE), MouseButton::Middle);
    EXPECT_EQ(CE::Input::mouse_button(GLFW_MOUSE_BUTTON_4), MouseButton::Extra1);
    EXPECT_EQ(CE::Input::mouse_button(GLFW_MOUSE_BUTTON_8), MouseButton::Extra5);
    EXPECT_EQ(CE::Input::mouse_button(-1), MouseButton::Unknown);
    EXPECT_EQ(CE::Input::mouse_button(GLFW_MOUSE_BUTTON_LAST + 1), MouseButton::Unknown);
}

TEST(input_lifetime, owner) {
    std::atomic_flag ownership = ATOMIC_FLAG_INIT;
    {
        CE::Input::GainputLifetime first(ownership, nullptr);
        EXPECT_THROW((void)CE::Input::GainputLifetime(ownership, nullptr), CE::Exceptions::failed_operation);
        // A rejected claimant must leave the existing owner in control.
        EXPECT_THROW((void)CE::Input::GainputLifetime(ownership, nullptr), CE::Exceptions::failed_operation);
        EXPECT_NO_THROW(first.require_window(nullptr));
    }
    EXPECT_NO_THROW((void)CE::Input::GainputLifetime(ownership, nullptr));
}

TEST(input_lifetime, failed_init) {
    std::atomic_flag ownership = ATOMIC_FLAG_INIT;
    const auto initialize = [&] {
        CE::Input::GainputLifetime lifetime(ownership, nullptr);
        throw std::runtime_error("initialization failed");
    };
    EXPECT_THROW(initialize(), std::runtime_error);
    EXPECT_NO_THROW((void)CE::Input::GainputLifetime(ownership, nullptr));
}

TEST(input_lifetime, window) {
    std::atomic_flag ownership = ATOMIC_FLAG_INIT;
    int first = 0;
    int second = 0;
    CE::Input::GainputLifetime lifetime(ownership, &first);
    EXPECT_NO_THROW(lifetime.require_window(&first));
    EXPECT_THROW(lifetime.require_window(&second), CE::Exceptions::failed_operation);
    EXPECT_NO_THROW(lifetime.require_window(&first));
    EXPECT_THROW((void)CE::Input::GainputLifetime(ownership, &second), CE::Exceptions::failed_operation);
}
