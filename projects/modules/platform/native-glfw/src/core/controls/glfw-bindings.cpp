#include <core/controls/glfw-bindings.h>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

namespace CE::Input {
    KeyboardKey keyboard_key(const int key) {
        // These ranges are contiguous within each enumeration; their numeric
        // values have no cross-backend identity outside this translation.
        if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
            return static_cast<KeyboardKey>(static_cast<int>(KeyboardKey::A) + key - GLFW_KEY_A);
        if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
            return static_cast<KeyboardKey>(static_cast<int>(KeyboardKey::Digit0) + key - GLFW_KEY_0);
        if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25)
            return static_cast<KeyboardKey>(static_cast<int>(KeyboardKey::F1) + key - GLFW_KEY_F1);
        if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9)
            return static_cast<KeyboardKey>(static_cast<int>(KeyboardKey::Keypad0) + key - GLFW_KEY_KP_0);

        switch (key) {
            case GLFW_KEY_SPACE:
                return KeyboardKey::Space;
            case GLFW_KEY_APOSTROPHE:
                return KeyboardKey::Apostrophe;
            case GLFW_KEY_COMMA:
                return KeyboardKey::Comma;
            case GLFW_KEY_MINUS:
                return KeyboardKey::Minus;
            case GLFW_KEY_PERIOD:
                return KeyboardKey::Period;
            case GLFW_KEY_SLASH:
                return KeyboardKey::Slash;
            case GLFW_KEY_SEMICOLON:
                return KeyboardKey::Semicolon;
            case GLFW_KEY_EQUAL:
                return KeyboardKey::Equal;
            case GLFW_KEY_LEFT_BRACKET:
                return KeyboardKey::LeftBracket;
            case GLFW_KEY_BACKSLASH:
                return KeyboardKey::Backslash;
            case GLFW_KEY_RIGHT_BRACKET:
                return KeyboardKey::RightBracket;
            case GLFW_KEY_GRAVE_ACCENT:
                return KeyboardKey::GraveAccent;
            case GLFW_KEY_ESCAPE:
                return KeyboardKey::Escape;
            case GLFW_KEY_ENTER:
                return KeyboardKey::Enter;
            case GLFW_KEY_TAB:
                return KeyboardKey::Tab;
            case GLFW_KEY_BACKSPACE:
                return KeyboardKey::Backspace;
            case GLFW_KEY_INSERT:
                return KeyboardKey::Insert;
            case GLFW_KEY_DELETE:
                return KeyboardKey::Delete;
            case GLFW_KEY_RIGHT:
                return KeyboardKey::Right;
            case GLFW_KEY_LEFT:
                return KeyboardKey::Left;
            case GLFW_KEY_DOWN:
                return KeyboardKey::Down;
            case GLFW_KEY_UP:
                return KeyboardKey::Up;
            case GLFW_KEY_PAGE_UP:
                return KeyboardKey::PageUp;
            case GLFW_KEY_PAGE_DOWN:
                return KeyboardKey::PageDown;
            case GLFW_KEY_HOME:
                return KeyboardKey::Home;
            case GLFW_KEY_END:
                return KeyboardKey::End;
            case GLFW_KEY_CAPS_LOCK:
                return KeyboardKey::CapsLock;
            case GLFW_KEY_SCROLL_LOCK:
                return KeyboardKey::ScrollLock;
            case GLFW_KEY_NUM_LOCK:
                return KeyboardKey::NumLock;
            case GLFW_KEY_PRINT_SCREEN:
                return KeyboardKey::PrintScreen;
            case GLFW_KEY_PAUSE:
                return KeyboardKey::Pause;
            case GLFW_KEY_KP_DECIMAL:
                return KeyboardKey::KeypadDecimal;
            case GLFW_KEY_KP_DIVIDE:
                return KeyboardKey::KeypadDivide;
            case GLFW_KEY_KP_MULTIPLY:
                return KeyboardKey::KeypadMultiply;
            case GLFW_KEY_KP_SUBTRACT:
                return KeyboardKey::KeypadSubtract;
            case GLFW_KEY_KP_ADD:
                return KeyboardKey::KeypadAdd;
            case GLFW_KEY_KP_ENTER:
                return KeyboardKey::KeypadEnter;
            case GLFW_KEY_KP_EQUAL:
                return KeyboardKey::KeypadEqual;
            case GLFW_KEY_LEFT_SHIFT:
                return KeyboardKey::LeftShift;
            case GLFW_KEY_LEFT_CONTROL:
                return KeyboardKey::LeftControl;
            case GLFW_KEY_LEFT_ALT:
                return KeyboardKey::LeftAlt;
            case GLFW_KEY_LEFT_SUPER:
                return KeyboardKey::LeftSuper;
            case GLFW_KEY_RIGHT_SHIFT:
                return KeyboardKey::RightShift;
            case GLFW_KEY_RIGHT_CONTROL:
                return KeyboardKey::RightControl;
            case GLFW_KEY_RIGHT_ALT:
                return KeyboardKey::RightAlt;
            case GLFW_KEY_RIGHT_SUPER:
                return KeyboardKey::RightSuper;
            case GLFW_KEY_MENU:
                return KeyboardKey::Menu;
            default:
                return KeyboardKey::Unknown;
        }
    }

    MouseButton mouse_button(const int button) {
        switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT:
                return MouseButton::Left;
            case GLFW_MOUSE_BUTTON_RIGHT:
                return MouseButton::Right;
            case GLFW_MOUSE_BUTTON_MIDDLE:
                return MouseButton::Middle;
            case GLFW_MOUSE_BUTTON_4:
                return MouseButton::Extra1;
            case GLFW_MOUSE_BUTTON_5:
                return MouseButton::Extra2;
            case GLFW_MOUSE_BUTTON_6:
                return MouseButton::Extra3;
            case GLFW_MOUSE_BUTTON_7:
                return MouseButton::Extra4;
            case GLFW_MOUSE_BUTTON_8:
                return MouseButton::Extra5;
            default:
                return MouseButton::Unknown;
        }
    }

    gainput::DeviceButtonId gainput_key(const int key) {
        // Contiguous GLFW ranges map directly to corresponding Gainput ranges;
        // keys with different enum layouts need the explicit table below.
        if ((key >= GLFW_KEY_0 && key <= GLFW_KEY_9) || (key >= GLFW_KEY_A && key <= GLFW_KEY_Z))
            return static_cast<gainput::DeviceButtonId>(key);
        if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F19)
            return gainput::KeyF1 + key - GLFW_KEY_F1;
        if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9)
            return gainput::KeyKpInsert + key - GLFW_KEY_KP_0;

        switch (key) {
            case GLFW_KEY_SPACE:
                return gainput::KeySpace;
            case GLFW_KEY_APOSTROPHE:
                return gainput::KeyApostrophe;
            case GLFW_KEY_COMMA:
                return gainput::KeyComma;
            case GLFW_KEY_MINUS:
                return gainput::KeyMinus;
            case GLFW_KEY_PERIOD:
                return gainput::KeyPeriod;
            case GLFW_KEY_SLASH:
                return gainput::KeySlash;
            case GLFW_KEY_SEMICOLON:
                return gainput::KeySemicolon;
            case GLFW_KEY_EQUAL:
                return gainput::KeyEqual;
            case GLFW_KEY_LEFT_BRACKET:
                return gainput::KeyBracketLeft;
            case GLFW_KEY_BACKSLASH:
                return gainput::KeyBackslash;
            case GLFW_KEY_RIGHT_BRACKET:
                return gainput::KeyBracketRight;
            case GLFW_KEY_GRAVE_ACCENT:
                return gainput::KeyGrave;
            case GLFW_KEY_ESCAPE:
                return gainput::KeyEscape;
            case GLFW_KEY_PRINT_SCREEN:
                return gainput::KeyPrint;
            case GLFW_KEY_SCROLL_LOCK:
                return gainput::KeyScrollLock;
            case GLFW_KEY_PAUSE:
                return gainput::KeyBreak;
            case GLFW_KEY_LEFT:
                return gainput::KeyLeft;
            case GLFW_KEY_RIGHT:
                return gainput::KeyRight;
            case GLFW_KEY_UP:
                return gainput::KeyUp;
            case GLFW_KEY_DOWN:
                return gainput::KeyDown;
            case GLFW_KEY_INSERT:
                return gainput::KeyInsert;
            case GLFW_KEY_HOME:
                return gainput::KeyHome;
            case GLFW_KEY_DELETE:
                return gainput::KeyDelete;
            case GLFW_KEY_END:
                return gainput::KeyEnd;
            case GLFW_KEY_PAGE_UP:
                return gainput::KeyPageUp;
            case GLFW_KEY_PAGE_DOWN:
                return gainput::KeyPageDown;
            case GLFW_KEY_NUM_LOCK:
                return gainput::KeyNumLock;
            case GLFW_KEY_KP_EQUAL:
                return gainput::KeyKpEqual;
            case GLFW_KEY_KP_DIVIDE:
                return gainput::KeyKpDivide;
            case GLFW_KEY_KP_MULTIPLY:
                return gainput::KeyKpMultiply;
            case GLFW_KEY_KP_SUBTRACT:
                return gainput::KeyKpSubtract;
            case GLFW_KEY_KP_ADD:
                return gainput::KeyKpAdd;
            case GLFW_KEY_KP_ENTER:
                return gainput::KeyKpEnter;
            case GLFW_KEY_KP_DECIMAL:
                return gainput::KeyKpDelete;
            case GLFW_KEY_BACKSPACE:
                return gainput::KeyBackSpace;
            case GLFW_KEY_TAB:
                return gainput::KeyTab;
            case GLFW_KEY_ENTER:
                return gainput::KeyReturn;
            case GLFW_KEY_CAPS_LOCK:
                return gainput::KeyCapsLock;
            case GLFW_KEY_LEFT_SHIFT:
                return gainput::KeyShiftL;
            case GLFW_KEY_LEFT_CONTROL:
                return gainput::KeyCtrlL;
            case GLFW_KEY_LEFT_SUPER:
                return gainput::KeySuperL;
            case GLFW_KEY_LEFT_ALT:
                return gainput::KeyAltL;
            case GLFW_KEY_RIGHT_ALT:
                return gainput::KeyAltR;
            case GLFW_KEY_RIGHT_SUPER:
                return gainput::KeySuperR;
            case GLFW_KEY_MENU:
                return gainput::KeyMenu;
            case GLFW_KEY_RIGHT_CONTROL:
                return gainput::KeyCtrlR;
            case GLFW_KEY_RIGHT_SHIFT:
                return gainput::KeyShiftR;
            default:
                return gainput::InvalidDeviceButtonId;
        }
    }

    gainput::DeviceButtonId gainput_mouse_button(const int button) {
        // Preserve left/right/middle's named IDs before offsetting the extra
        // mouse buttons into Gainput's numbered range.
        if (button < GLFW_MOUSE_BUTTON_1 || button > GLFW_MOUSE_BUTTON_LAST)
            return gainput::InvalidDeviceButtonId;
        if (button == GLFW_MOUSE_BUTTON_LEFT)
            return gainput::MouseButtonLeft;
        if (button == GLFW_MOUSE_BUTTON_RIGHT)
            return gainput::MouseButtonRight;
        if (button == GLFW_MOUSE_BUTTON_MIDDLE)
            return gainput::MouseButtonMiddle;
        return gainput::MouseButton5 + button - GLFW_MOUSE_BUTTON_4;
    }
}
