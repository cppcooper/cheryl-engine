#include <ui/rmlui/input.h>

namespace CE::UI::RmlUi {
    Rml::Input::KeyIdentifier keyboard_key(const Input::KeyboardKey key) {
        using Key = Input::KeyboardKey;
        using namespace Rml::Input;
        if (key >= Key::A && key <= Key::Z)
            return static_cast<KeyIdentifier>(KI_A + static_cast<int>(key) - static_cast<int>(Key::A));
        if (key >= Key::Digit0 && key <= Key::Digit9)
            return static_cast<KeyIdentifier>(KI_0 + static_cast<int>(key) - static_cast<int>(Key::Digit0));
        if (key >= Key::F1 && key <= Key::F24)
            return static_cast<KeyIdentifier>(KI_F1 + static_cast<int>(key) - static_cast<int>(Key::F1));
        if (key >= Key::Keypad0 && key <= Key::Keypad9)
            return static_cast<KeyIdentifier>(KI_NUMPAD0 + static_cast<int>(key) - static_cast<int>(Key::Keypad0));
        switch (key) {
            case Key::Space:
                return KI_SPACE;
            case Key::Apostrophe:
                return KI_OEM_7;
            case Key::Comma:
                return KI_OEM_COMMA;
            case Key::Minus:
                return KI_OEM_MINUS;
            case Key::Period:
                return KI_OEM_PERIOD;
            case Key::Slash:
                return KI_OEM_2;
            case Key::Semicolon:
                return KI_OEM_1;
            case Key::Equal:
                return KI_OEM_PLUS;
            case Key::LeftBracket:
                return KI_OEM_4;
            case Key::Backslash:
                return KI_OEM_5;
            case Key::RightBracket:
                return KI_OEM_6;
            case Key::GraveAccent:
                return KI_OEM_3;
            case Key::Escape:
                return KI_ESCAPE;
            case Key::Enter:
                return KI_RETURN;
            case Key::Tab:
                return KI_TAB;
            case Key::Backspace:
                return KI_BACK;
            case Key::Insert:
                return KI_INSERT;
            case Key::Delete:
                return KI_DELETE;
            case Key::Right:
                return KI_RIGHT;
            case Key::Left:
                return KI_LEFT;
            case Key::Down:
                return KI_DOWN;
            case Key::Up:
                return KI_UP;
            case Key::PageUp:
                return KI_PRIOR;
            case Key::PageDown:
                return KI_NEXT;
            case Key::Home:
                return KI_HOME;
            case Key::End:
                return KI_END;
            case Key::CapsLock:
                return KI_CAPITAL;
            case Key::ScrollLock:
                return KI_SCROLL;
            case Key::NumLock:
                return KI_NUMLOCK;
            case Key::PrintScreen:
                return KI_SNAPSHOT;
            case Key::Pause:
                return KI_PAUSE;
            case Key::KeypadDecimal:
                return KI_DECIMAL;
            case Key::KeypadDivide:
                return KI_DIVIDE;
            case Key::KeypadMultiply:
                return KI_MULTIPLY;
            case Key::KeypadSubtract:
                return KI_SUBTRACT;
            case Key::KeypadAdd:
                return KI_ADD;
            case Key::KeypadEnter:
                return KI_NUMPADENTER;
            case Key::KeypadEqual:
                return KI_OEM_NEC_EQUAL;
            case Key::LeftShift:
                return KI_LSHIFT;
            case Key::LeftControl:
                return KI_LCONTROL;
            case Key::LeftAlt:
                return KI_LMENU;
            case Key::LeftSuper:
                return KI_LMETA;
            case Key::RightShift:
                return KI_RSHIFT;
            case Key::RightControl:
                return KI_RCONTROL;
            case Key::RightAlt:
                return KI_RMENU;
            case Key::RightSuper:
                return KI_RMETA;
            case Key::Menu:
                return KI_APPS;
            default:
                return KI_UNKNOWN;
        }
    }

    int modifiers(const Input::Modifiers value) {
        int result = 0;
        const auto add = [&](const Input::Modifiers flag, const Rml::Input::KeyModifier mapped) {
            if (Input::has_modifier(value, flag))
                result |= mapped;
        };
        add(Input::Modifiers::Control, Rml::Input::KM_CTRL);
        add(Input::Modifiers::Shift, Rml::Input::KM_SHIFT);
        add(Input::Modifiers::Alt, Rml::Input::KM_ALT);
        add(Input::Modifiers::Super, Rml::Input::KM_META);
        add(Input::Modifiers::CapsLock, Rml::Input::KM_CAPSLOCK);
        add(Input::Modifiers::NumLock, Rml::Input::KM_NUMLOCK);
        return result;
    }
}
