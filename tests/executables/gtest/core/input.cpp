#include <gtest/gtest.h>

#include <core/controls/glfw-bindings.h>
#include <core/controls/input-bindings.h>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

TEST(input_bindings, dispatches_buttons_and_axes_to_their_own_bindings) {
    CE::Input::InputBindings bindings;
    const CE::Input::DeviceBind key{2, gainput::KeyEscape};
    const CE::Input::DeviceBind axis{3, gainput::MouseAxisX};
    int presses = 0;
    float last_axis = 0.0f;

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
    bindings.on_button({3, gainput::KeyEscape}, false, true);
    EXPECT_EQ(presses, 1);
    EXPECT_FLOAT_EQ(last_axis, 0.75f);

    bindings.bind_button(key, {});
    bindings.on_button(key, false, true);
    EXPECT_EQ(presses, 1);
    bindings.bind_button(key, [&](bool, bool) { presses += 10; });
    bindings.on_button(key, false, true);
    EXPECT_EQ(presses, 11);
    bindings.clear();
    bindings.on_button(key, false, true);
    bindings.on_axis(axis, 0.0f, 0.25f);
    EXPECT_EQ(presses, 11);
    EXPECT_FLOAT_EQ(last_axis, 0.75f);
}

TEST(glfw_bindings, translates_keys_and_mouse_buttons_to_gainput_ids) {
    EXPECT_EQ(CE::Input::gainput_key(GLFW_KEY_A), gainput::KeyA);
    EXPECT_EQ(CE::Input::gainput_key(GLFW_KEY_8), gainput::Key8);
    EXPECT_EQ(CE::Input::gainput_key(GLFW_KEY_ESCAPE), gainput::KeyEscape);
    EXPECT_EQ(CE::Input::gainput_key(GLFW_KEY_F12), gainput::KeyF12);
    EXPECT_EQ(CE::Input::gainput_key(GLFW_KEY_KP_5), gainput::KeyKpBegin);
    EXPECT_EQ(CE::Input::gainput_key(GLFW_KEY_RIGHT_CONTROL), gainput::KeyCtrlR);
    EXPECT_EQ(CE::Input::gainput_key(GLFW_KEY_F20), gainput::InvalidDeviceButtonId);

    EXPECT_EQ(CE::Input::gainput_mouse_button(GLFW_MOUSE_BUTTON_LEFT), gainput::MouseButtonLeft);
    EXPECT_EQ(CE::Input::gainput_mouse_button(GLFW_MOUSE_BUTTON_RIGHT), gainput::MouseButtonRight);
    EXPECT_EQ(CE::Input::gainput_mouse_button(GLFW_MOUSE_BUTTON_MIDDLE), gainput::MouseButtonMiddle);
    EXPECT_EQ(CE::Input::gainput_mouse_button(GLFW_MOUSE_BUTTON_4), gainput::MouseButton5);
    EXPECT_EQ(CE::Input::gainput_mouse_button(-1), gainput::InvalidDeviceButtonId);
}
