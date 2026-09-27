#include <gtest/gtest.h>

#ifndef CHERYL_SANDBOX_BUILD
#include <core/controls/glfw-bindings.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#endif

#include <core/controls/input-bindings.h>

TEST(input_bindings, device_routing) {
    CE::Input::InputBindings bindings;
    constexpr CE::Input::DeviceButtonId key_code = 256;
    constexpr CE::Input::DeviceButtonId axis_code = 257;
    const CE::Input::DeviceBind key{2, key_code};
    const CE::Input::DeviceBind axis{3, axis_code};
    int presses = 0;
    float last_axis = 0.0f;

    // Register independent callbacks for a button and an axis, each with
    // expectations about the previous value passed by the binding system.
    bindings.bind_button(key, [&](bool previous, bool current) {
        EXPECT_FALSE(previous);
        EXPECT_TRUE(current);
        ++presses;
    });
    bindings.bind_axis(axis, [&](float previous, float current) {
        EXPECT_FLOAT_EQ(previous, 0.0f);
        last_axis = current;
    });

    bindings.on_button(key, false, true);
    bindings.on_axis(axis, 0.0f, 0.75f);
    // A matching code from a different device must not trigger the button.
    bindings.on_button({3, key_code}, false, true);
    EXPECT_EQ(presses, 1);
    EXPECT_FLOAT_EQ(last_axis, 0.75f);
}

TEST(input_bindings, callback_management) {
    CE::Input::InputBindings bindings;
    const CE::Input::DeviceBind key{2, 256};
    const CE::Input::DeviceBind axis{3, 257};
    int original_presses = 0;
    int replacement_presses = 0;
    float last_axis = 0.0f;
    bindings.bind_button(key, [&](bool, bool) { ++original_presses; });
    bindings.bind_axis(axis, [&](float, float current) { last_axis = current; });

    // Remove, replace, and finally clear the callbacks. Only the replacement
    // should observe an event; clearing stops both types of dispatch.
    bindings.bind_button(key, {});
    bindings.on_button(key, false, true);
    EXPECT_EQ(original_presses, 0);
    bindings.bind_button(key, [&](bool, bool) { ++replacement_presses; });
    bindings.on_button(key, false, true);
    EXPECT_EQ(original_presses, 0);
    EXPECT_EQ(replacement_presses, 1);
    bindings.clear();
    bindings.on_button(key, false, true);
    bindings.on_axis(axis, 0.0f, 0.25f);
    EXPECT_EQ(replacement_presses, 1);
    EXPECT_FLOAT_EQ(last_axis, 0.0f);
}

#ifndef CHERYL_SANDBOX_BUILD
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
#endif
