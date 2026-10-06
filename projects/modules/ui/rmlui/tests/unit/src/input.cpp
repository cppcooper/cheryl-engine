#include <gtest/gtest.h>

#include <ui/rmlui/input.h>

TEST(ui_rmlui_input, keys) {
    using Key = CE::Input::KeyboardKey;
    using namespace Rml::Input;
    using CE::UI::RmlUi::keyboard_key;
    EXPECT_EQ(keyboard_key(Key::A), KI_A);
    EXPECT_EQ(keyboard_key(Key::Z), KI_Z);
    EXPECT_EQ(keyboard_key(Key::Digit9), KI_9);
    EXPECT_EQ(keyboard_key(Key::F24), KI_F24);
    EXPECT_EQ(keyboard_key(Key::F25), KI_UNKNOWN);
    EXPECT_EQ(keyboard_key(Key::Keypad7), KI_NUMPAD7);
    EXPECT_EQ(keyboard_key(Key::KeypadEnter), KI_NUMPADENTER);
    EXPECT_EQ(keyboard_key(Key::LeftSuper), KI_LMETA);
    EXPECT_EQ(keyboard_key(Key::Delete), KI_DELETE);
    EXPECT_EQ(keyboard_key(Key::Unknown), KI_UNKNOWN);
}

TEST(ui_rmlui_input, modifiers) {
    using M = CE::Input::Modifiers;
    const auto flags = M::Control | M::Shift | M::Super | M::CapsLock;
    EXPECT_EQ(CE::UI::RmlUi::modifiers(flags), Rml::Input::KM_CTRL | Rml::Input::KM_SHIFT | Rml::Input::KM_META | Rml::Input::KM_CAPSLOCK);
    EXPECT_EQ(CE::UI::RmlUi::modifiers(M::None), 0);
}
