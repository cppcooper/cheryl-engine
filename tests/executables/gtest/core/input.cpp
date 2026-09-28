#include <gtest/gtest.h>

#ifndef CHERYL_SANDBOX_BUILD
#include <core/controls/glfw-bindings.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#endif

#include <core/controls/input-bindings.h>
#include <core/controls/tick-input.h>

TEST(input_bindings, device_routing) {
    CE::Input::InputBindings bindings;
    constexpr CE::Input::DeviceButtonId key_code = 256;
    constexpr CE::Input::DeviceButtonId axis_code = 257;
    const CE::Input::DeviceBind key{2, key_code};
    const CE::Input::DeviceBind axis{3, axis_code};
    const CE::Input::ActionId jump{1};
    const CE::Input::ActionId look{2};

    (void)bindings.bind_button(key, jump);
    (void)bindings.bind_axis(axis, look);

    // A matching button code from a different device cannot activate Jump.
    bindings.on_button({3, key_code}, true);
    const auto unrelated = bindings.publish_actions();
    EXPECT_FALSE(unrelated->button(jump).held());

    bindings.on_button(key, true);
    bindings.on_axis(axis, 0.75f);
    const auto active = bindings.publish_actions();
    EXPECT_TRUE(active->button(jump).pressed());
    EXPECT_FLOAT_EQ(active->axis(look).current, 0.75f);
}

TEST(input_bindings, binding_held_controls_and_clearing) {
    CE::Input::InputBindings bindings;
    const CE::Input::DeviceBind key{2, 256};
    const CE::Input::ActionId jump{1};

    // Physical state is retained even before a game binds it. A binding installed
    // for a held key becomes active at the next publication.
    bindings.on_button(key, true);
    (void)bindings.publish_actions();
    (void)bindings.bind_button(key, jump);
    EXPECT_TRUE(bindings.publish_actions()->button(jump).pressed());

    // Clearing publishes one release and forgets held device state, so a later
    // binding starts inactive until its device reports a new press.
    bindings.clear();
    EXPECT_TRUE(bindings.action_snapshot()->button(jump).released());
    (void)bindings.bind_button(key, jump);
    EXPECT_FALSE(bindings.publish_actions()->button(jump).held());

    // A backend that discovers the key is still held can restore its state.
    bindings.on_button(key, true);
    EXPECT_TRUE(bindings.publish_actions()->button(jump).pressed());
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
    bindings.on_button(w, true);
    const auto first = bindings.publish_actions();
    EXPECT_TRUE(first->button(move).held());
    EXPECT_TRUE(first->button(move).pressed());
    const auto second = bindings.publish_actions();
    EXPECT_TRUE(second->button(move).held());
    EXPECT_FALSE(second->button(move).pressed());
    EXPECT_EQ(second->poll(), first->poll() + 1);

    // The chord becomes active when its second member arrives. A release in the same poll
    // preserves both transition edges even though the final sample is inactive.
    bindings.on_button(f, true);
    bindings.on_button(u, true);
    bindings.on_button(u, false);
    bindings.on_button(w, false);
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
    bindings.on_button(keyboard, true);
    (void)bindings.publish_actions();
    bindings.on_button(gamepad, true);
    bindings.on_button(keyboard, false);
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
    bindings.on_axis(stick, 0.6f);
    bindings.on_axis(second_stick, 0.25f);
    EXPECT_FLOAT_EQ(bindings.publish_actions()->axis(look).current, 0.25f);
    bindings.on_button(shift, true);
    const auto active = bindings.publish_actions();
    EXPECT_FLOAT_EQ(active->axis(look).current, -0.75f);
    EXPECT_FLOAT_EQ(active->axis(look).delta(), -1.0f);
    bindings.on_button(shift, false);
    EXPECT_FLOAT_EQ(bindings.publish_actions()->axis(look).current, 0.25f);
    EXPECT_FLOAT_EQ(active->axis(look).current, -0.75f);
}

TEST(input_bindings, unbinding_an_active_axis) {
    CE::Input::InputBindings bindings;
    const CE::Input::DeviceBind stick{2, 1};
    const CE::Input::ActionId look{1};
    const auto mapping = bindings.bind_axis(stick, look);

    bindings.on_axis(stick, 0.8f);
    const auto active = bindings.publish_actions();
    EXPECT_FLOAT_EQ(active->axis(look).current, 0.8f);

    // Removing an active axis delivers one zero sample with the previous value,
    // so a game can react to its last delta. Older snapshots stay unchanged.
    EXPECT_TRUE(bindings.unbind(mapping));
    const auto removed = bindings.publish_actions();
    EXPECT_FLOAT_EQ(removed->axis(look).previous, 0.8f);
    EXPECT_FLOAT_EQ(removed->axis(look).current, 0.0f);
    EXPECT_FLOAT_EQ(removed->axis(look).delta(), -0.8f);
    EXPECT_FLOAT_EQ(active->axis(look).current, 0.8f);
    EXPECT_FLOAT_EQ(bindings.publish_actions()->axis(look).delta(), 0.0f);
}

TEST(tick_input, press_and_release_between_updates) {
    CE::Input::InputBindings bindings;
    const CE::Input::DeviceBind key{1, 32};
    const CE::Input::ActionId jump{1};
    (void)bindings.bind_button(key, jump);
    const auto before = bindings.action_snapshot();

    bindings.on_button(key, true);
    const auto pressed = bindings.publish_actions();
    bindings.on_button(key, false);
    const auto released = bindings.publish_actions();

    // Both polls belong to this update. The final state is released, but the
    // short press remains visible and its poll order is available to the game.
    const CE::Input::TickInput tick(before, {pressed, released});
    EXPECT_FALSE(tick.button(jump).held());
    EXPECT_TRUE(tick.button(jump).pressed());
    EXPECT_TRUE(tick.button(jump).released());
    ASSERT_EQ(tick.polls().size(), 2u);
    EXPECT_TRUE(tick.polls()[0]->button(jump).pressed());
    EXPECT_TRUE(tick.polls()[1]->button(jump).released());

    // A second update with no new poll keeps the final held state without
    // replaying either edge.
    const CE::Input::TickInput next(tick.latest_poll(), {});
    EXPECT_FALSE(next.button(jump).held());
    EXPECT_FALSE(next.button(jump).pressed());
    EXPECT_FALSE(next.button(jump).released());
}

TEST(tick_input, held_values_without_new_polls) {
    CE::Input::InputBindings bindings;
    const CE::Input::DeviceBind key{1, 32};
    const CE::Input::DeviceBind stick{2, 1};
    const CE::Input::ActionId move{1};
    const CE::Input::ActionId look{2};
    (void)bindings.bind_button(key, move);
    (void)bindings.bind_axis(stick, look);
    const auto before = bindings.action_snapshot();

    bindings.on_button(key, true);
    bindings.on_axis(stick, 0.75f);
    const auto poll = bindings.publish_actions();
    const CE::Input::TickInput first(before, {poll});
    EXPECT_TRUE(first.button(move).pressed());
    EXPECT_FLOAT_EQ(first.axis(look).delta(), 0.75f);

    // Simulation can tick again before the platform polls. Held/axis values
    // persist; the earlier press and axis movement do not happen twice.
    const CE::Input::TickInput next(first.latest_poll(), {});
    EXPECT_TRUE(next.button(move).held());
    EXPECT_FALSE(next.button(move).pressed());
    EXPECT_FLOAT_EQ(next.axis(look).current, 0.75f);
    EXPECT_FLOAT_EQ(next.axis(look).delta(), 0.0f);
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
