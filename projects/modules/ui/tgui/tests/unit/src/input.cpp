#include <gtest/gtest.h>

#include <ui/tgui/input.h>
#include <internals/exceptions.h>

#include <array>
#include <limits>
#include <utility>

namespace {
    using CE::Input::ButtonEvent;
    using CE::Input::ButtonPhase;
    using CE::Input::DeviceKind;
    using CE::Input::KeyboardKey;
    using CE::Input::Modifiers;
    using CE::Input::MouseButton;
    using CE::Input::PointerEvent;
    using CE::Input::ScrollEvent;
    using CE::UI::TGUI::translate_event;

    CE::Input::InputRecord record(const DeviceKind kind, CE::Input::InputRecordData data) {
        return {17, CE::Input::InputClock::now(), 23, kind, std::move(data), 29, 31, false};
    }
}

TEST(ui_tgui_input, keys) {
    using ToolkitKey = tgui::Event::KeyboardKey;
    constexpr std::array pairs{std::pair{KeyboardKey::A, ToolkitKey::A},
                               std::pair{KeyboardKey::Z, ToolkitKey::Z},
                               std::pair{KeyboardKey::Digit0, ToolkitKey::Num0},
                               std::pair{KeyboardKey::Digit9, ToolkitKey::Num9},
                               std::pair{KeyboardKey::F1, ToolkitKey::F1},
                               std::pair{KeyboardKey::F15, ToolkitKey::F15},
                               std::pair{KeyboardKey::Keypad0, ToolkitKey::Numpad0},
                               std::pair{KeyboardKey::Keypad9, ToolkitKey::Numpad9},
                               std::pair{KeyboardKey::KeypadEnter, ToolkitKey::Enter},
                               std::pair{KeyboardKey::Backspace, ToolkitKey::Backspace},
                               std::pair{KeyboardKey::RightControl, ToolkitKey::RControl}};
    for (const auto [portable, toolkit] : pairs) {
        // Deliberately unrelated opaque/native codes cannot choose the key.
        const auto source = record(DeviceKind::Keyboard, ButtonEvent{900, ButtonPhase::Press, Modifiers::None, 800, 700, portable});
        const auto event = translate_event(source);
        ASSERT_TRUE(event);
        EXPECT_EQ(event->type, tgui::Event::Type::KeyPressed);
        EXPECT_EQ(event->key.code, toolkit);
    }
}

TEST(ui_tgui_input, repeat_modifiers) {
    const auto modifiers = Modifiers::Alt | Modifiers::Control | Modifiers::Shift | Modifiers::Super | Modifiers::CapsLock;
    const auto source = record(DeviceKind::Keyboard, ButtonEvent{9, ButtonPhase::Repeat, modifiers, -1, -1, KeyboardKey::A});
    const auto event = translate_event(source);
    ASSERT_TRUE(event);
    EXPECT_EQ(event->type, tgui::Event::Type::KeyPressed);
    EXPECT_EQ(event->key.code, tgui::Event::KeyboardKey::A);
    EXPECT_TRUE(event->key.alt);
    EXPECT_TRUE(event->key.control);
    EXPECT_TRUE(event->key.shift);
    EXPECT_TRUE(event->key.system);
    EXPECT_EQ(std::get<ButtonEvent>(source.data).phase, ButtonPhase::Repeat);
    EXPECT_EQ(source.sequence, 17u);
    EXPECT_EQ(source.target, 29u);
    EXPECT_EQ(source.focus_epoch, 31u);
}

TEST(ui_tgui_input, unhandled_keys) {
    EXPECT_FALSE(
        translate_event(record(DeviceKind::Keyboard, ButtonEvent{1, ButtonPhase::Release, Modifiers::None, -1, -1, KeyboardKey::A}))
    );
    // A familiar native token must not turn an Unknown portable key into text/key input.
    EXPECT_FALSE(translate_event(record(DeviceKind::Keyboard, ButtonEvent{1, ButtonPhase::Press, Modifiers::None, 65})));
    EXPECT_FALSE(
        translate_event(record(DeviceKind::Keyboard, ButtonEvent{1, ButtonPhase::Press, Modifiers::None, -1, -1, KeyboardKey::F16}))
    );
}

TEST(ui_tgui_input, committed_text) {
    const auto source = record(DeviceKind::Keyboard, CE::Input::TextEvent{U'\U0001f642'});
    const auto event = translate_event(source);
    ASSERT_TRUE(event);
    EXPECT_EQ(event->type, tgui::Event::Type::TextEntered);
    EXPECT_EQ(event->text.unicode, U'\U0001f642');
    EXPECT_EQ(std::get<CE::Input::TextEvent>(source.data).codepoint, U'\U0001f642');
}

TEST(ui_tgui_input, invalid_text) {
    EXPECT_THROW(translate_event(record(DeviceKind::Keyboard, CE::Input::TextEvent{0xD800})), CE::Exceptions::invalid_args);
    EXPECT_THROW(translate_event(record(DeviceKind::Keyboard, CE::Input::TextEvent{0x110000})), CE::Exceptions::invalid_args);
}

TEST(ui_tgui_input, pointer_position) {
    const auto event = translate_event(record(DeviceKind::Mouse, PointerEvent{12.75, -0.25}));
    ASSERT_TRUE(event);
    EXPECT_EQ(event->type, tgui::Event::Type::MouseMoved);
    EXPECT_EQ(event->mouseMove.x, 12);
    EXPECT_EQ(event->mouseMove.y, -1); // A fractional position outside stays outside.
}

TEST(ui_tgui_input, mouse_buttons) {
    using ToolkitButton = tgui::Event::MouseButton;
    constexpr std::array pairs{std::pair{MouseButton::Left, ToolkitButton::Left}, std::pair{MouseButton::Right, ToolkitButton::Right},
                               std::pair{MouseButton::Middle, ToolkitButton::Middle}};
    for (const auto [portable, toolkit] : pairs) {
        const auto source = record(
            DeviceKind::Mouse,
            ButtonEvent{9, ButtonPhase::Press, Modifiers::None, 88, -1, KeyboardKey::Unknown, portable, PointerEvent{17.5, 31.5}}
        );
        const auto event = translate_event(source);
        ASSERT_TRUE(event);
        EXPECT_EQ(event->type, tgui::Event::Type::MouseButtonPressed);
        EXPECT_EQ(event->mouseButton.button, toolkit);
        EXPECT_EQ(event->mouseButton.x, 17);
        EXPECT_EQ(event->mouseButton.y, 31);
    }
}

TEST(ui_tgui_input, click_snapshot) {
    // A first click needs no prior move; a later move cannot change this event.
    const auto source = record(
        DeviceKind::Mouse,
        ButtonEvent{9, ButtonPhase::Release, Modifiers::None, -1, -1, KeyboardKey::Unknown, MouseButton::Right, PointerEvent{17, 31}}
    );
    const auto first = translate_event(source);
    ASSERT_TRUE(first);
    EXPECT_EQ(first->type, tgui::Event::Type::MouseButtonReleased);
    (void)translate_event(record(DeviceKind::Mouse, PointerEvent{100, 200}));
    const auto repeated = translate_event(source);
    ASSERT_TRUE(repeated);
    EXPECT_EQ(repeated->mouseButton.x, 17);
    EXPECT_EQ(repeated->mouseButton.y, 31);
}

TEST(ui_tgui_input, fractional_wheel) {
    const auto source = record(DeviceKind::Mouse, ScrollEvent{0.5, -0.25, PointerEvent{11.5, 23.5}});
    const auto event = translate_event(source);
    ASSERT_TRUE(event);
    EXPECT_EQ(event->type, tgui::Event::Type::MouseWheelScrolled);
    EXPECT_FLOAT_EQ(event->mouseWheel.delta, -0.25f);
    EXPECT_EQ(event->mouseWheel.x, 11);
    EXPECT_EQ(event->mouseWheel.y, 23);
    EXPECT_DOUBLE_EQ(std::get<ScrollEvent>(source.data).x, 0.5);
    EXPECT_FALSE(translate_event(record(DeviceKind::Mouse, ScrollEvent{0.5, 0.0})));
}

TEST(ui_tgui_input, missing_position) {
    EXPECT_THROW(
        translate_event(
            record(DeviceKind::Mouse, ButtonEvent{1, ButtonPhase::Press, Modifiers::None, -1, -1, KeyboardKey::Unknown, MouseButton::Left})
        ),
        CE::Exceptions::invalid_args
    );
    EXPECT_THROW(translate_event(record(DeviceKind::Mouse, ScrollEvent{0, 1})), CE::Exceptions::invalid_args);
}

TEST(ui_tgui_input, invalid_coordinates) {
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(translate_event(record(DeviceKind::Mouse, PointerEvent{nan, 0})), CE::Exceptions::invalid_args);
    EXPECT_THROW(
        translate_event(record(DeviceKind::Mouse, PointerEvent{static_cast<double>(std::numeric_limits<int>::max()) + 1.0, 0})),
        CE::Exceptions::invalid_args
    );
    EXPECT_THROW(translate_event(record(DeviceKind::Mouse, ScrollEvent{0, nan, PointerEvent{0, 0}})), CE::Exceptions::invalid_args);
    EXPECT_THROW(
        translate_event(
            record(DeviceKind::Mouse, ScrollEvent{0, static_cast<double>(std::numeric_limits<float>::max()) * 2, PointerEvent{0, 0}})
        ),
        CE::Exceptions::invalid_args
    );
}

TEST(ui_tgui_input, other_controls) {
    EXPECT_FALSE(translate_event(record(DeviceKind::Gamepad, ButtonEvent{1, ButtonPhase::Press, Modifiers::None, -1, -1, KeyboardKey::A})));
    EXPECT_FALSE(translate_event(
        record(DeviceKind::Mouse, ButtonEvent{1, ButtonPhase::Press, Modifiers::None, -1, -1, KeyboardKey::Unknown, MouseButton::Extra1})
    ));
    EXPECT_FALSE(translate_event(record(DeviceKind::Mouse, CE::Input::AxisEvent{1, 0.5f})));
}
