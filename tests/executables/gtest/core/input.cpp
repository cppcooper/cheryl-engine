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

TEST(input_bindings, chords_and_short_taps) {
    CE::Input::InputBindings bindings;
    const CE::Input::DeviceBind w{1, 87};
    const CE::Input::DeviceBind f{1, 70};
    const CE::Input::DeviceBind u{1, 85};
    const CE::Input::ActionId move{1};
    const CE::Input::ActionId chord{2};
    (void)bindings.bind_button(w, move);
    (void)bindings.bind_button(CE::Input::InputChord{{f, u}}, chord);

    // A single held key stays held across polls but is pressed only in the first sample.
    bindings.on_button(w, false, true);
    const auto first = bindings.publish_actions();
    EXPECT_TRUE(first->button(move).held());
    EXPECT_TRUE(first->button(move).pressed());
    const auto second = bindings.publish_actions();
    EXPECT_TRUE(second->button(move).held());
    EXPECT_FALSE(second->button(move).pressed());
    EXPECT_EQ(second->poll(), first->poll() + 1);

    // The chord becomes active when its second member arrives. A release in the same poll
    // preserves both transition edges even though the final sample is inactive.
    bindings.on_button(f, false, true);
    bindings.on_button(u, false, true);
    bindings.on_button(u, true, false);
    bindings.on_button(w, true, false);
    const auto third = bindings.publish_actions();
    EXPECT_TRUE(third->button(chord).pressed());
    EXPECT_TRUE(third->button(chord).released());
    EXPECT_FALSE(third->button(chord).held());
    EXPECT_TRUE(third->button(move).released());
    EXPECT_TRUE(first->button(move).held()); // A retained snapshot never changes under a later poll.
}

TEST(input_bindings, alternatives_and_remapping) {
    CE::Input::InputBindings bindings;
    const CE::Input::DeviceBind keyboard{1, 32};
    const CE::Input::DeviceBind gamepad{2, 32};
    const CE::Input::ActionId jump{1};
    const auto keyboard_binding = bindings.bind_button(keyboard, jump);
    (void)bindings.bind_button(gamepad, jump);

    // Either physical control holds Jump. Releasing one cannot release the action
    // until the other alternative is also inactive.
    bindings.on_button(keyboard, false, true);
    (void)bindings.publish_actions();
    bindings.on_button(gamepad, false, true);
    bindings.on_button(keyboard, true, false);
    const auto still_held = bindings.publish_actions();
    EXPECT_TRUE(still_held->button(jump).held());
    EXPECT_FALSE(still_held->button(jump).released());

    // Removing a binding or the entire action takes effect at the next commit.
    EXPECT_TRUE(bindings.unbind(keyboard_binding));
    EXPECT_FALSE(bindings.unbind(keyboard_binding));
    bindings.unbind_action(jump);
    const auto removed = bindings.publish_actions();
    EXPECT_TRUE(removed->button(jump).released());
    EXPECT_FALSE(bindings.publish_actions()->button(jump).released());
}

TEST(input_bindings, scaled_axis_with_a_modifier) {
    CE::Input::InputBindings bindings;
    const CE::Input::DeviceBind shift{1, 1};
    const CE::Input::DeviceBind stick{2, 1};
    const CE::Input::DeviceBind second_stick{3, 1};
    const CE::Input::ActionId look{1};
    (void)bindings.bind_axis(CE::Input::InputChord{{shift}}, stick, look, {-2.0f, 0.2f});
    (void)bindings.bind_axis(second_stick, look);

    // The first stick is gated by Shift and rescales 0.6 beyond its 0.2 dead zone
    // to 0.5; its negative scale then contributes -1 alongside the other stick's 0.25.
    bindings.on_axis(stick, 0.0f, 0.6f);
    bindings.on_axis(second_stick, 0.0f, 0.25f);
    EXPECT_FLOAT_EQ(bindings.publish_actions()->axis(look).current, 0.25f);
    bindings.on_button(shift, false, true);
    const auto active = bindings.publish_actions();
    EXPECT_FLOAT_EQ(active->axis(look).current, -0.75f);
    EXPECT_FLOAT_EQ(active->axis(look).delta(), -1.0f);
    bindings.on_button(shift, true, false);
    EXPECT_FLOAT_EQ(bindings.publish_actions()->axis(look).current, 0.25f);
    EXPECT_FLOAT_EQ(active->axis(look).current, -0.75f);
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
