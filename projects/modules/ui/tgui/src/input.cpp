#include <ui/tgui/input.h>

#include "dependency-contract.h"
#include <internals/exceptions.h>

#include <cmath>
#include <limits>

namespace CE::UI::TGUI {
    namespace {
        using Key = Input::KeyboardKey;
        using ToolkitKey = tgui::Event::KeyboardKey;

        ToolkitKey keyboard_key(const Key key) {
            if (key >= Key::A && key <= Key::Z)
                return static_cast<ToolkitKey>(static_cast<int>(ToolkitKey::A) + static_cast<int>(key) - static_cast<int>(Key::A));
            if (key >= Key::Digit0 && key <= Key::Digit9)
                return static_cast<ToolkitKey>(static_cast<int>(ToolkitKey::Num0) + static_cast<int>(key) - static_cast<int>(Key::Digit0));
            if (key >= Key::F1 && key <= Key::F15)
                return static_cast<ToolkitKey>(static_cast<int>(ToolkitKey::F1) + static_cast<int>(key) - static_cast<int>(Key::F1));
            if (key >= Key::Keypad0 && key <= Key::Keypad9)
                return static_cast<ToolkitKey>(
                    static_cast<int>(ToolkitKey::Numpad0) + static_cast<int>(key) - static_cast<int>(Key::Keypad0)
                );

            switch (key) {
                case Key::Space:
                    return ToolkitKey::Space;
                case Key::Apostrophe:
                    return ToolkitKey::Quote;
                case Key::Comma:
                    return ToolkitKey::Comma;
                case Key::Minus:
                    return ToolkitKey::Minus;
                case Key::Period:
                    return ToolkitKey::Period;
                case Key::Slash:
                    return ToolkitKey::Slash;
                case Key::Semicolon:
                    return ToolkitKey::Semicolon;
                case Key::Equal:
                    return ToolkitKey::Equal;
                case Key::LeftBracket:
                    return ToolkitKey::LBracket;
                case Key::Backslash:
                    return ToolkitKey::Backslash;
                case Key::RightBracket:
                    return ToolkitKey::RBracket;
                case Key::GraveAccent:
                    return ToolkitKey::Tilde;
                case Key::Escape:
                    return ToolkitKey::Escape;
                case Key::Enter:
                    return ToolkitKey::Enter;
                case Key::Tab:
                    return ToolkitKey::Tab;
                case Key::Backspace:
                    return ToolkitKey::Backspace;
                case Key::Insert:
                    return ToolkitKey::Insert;
                case Key::Delete:
                    return ToolkitKey::Delete;
                case Key::Right:
                    return ToolkitKey::Right;
                case Key::Left:
                    return ToolkitKey::Left;
                case Key::Down:
                    return ToolkitKey::Down;
                case Key::Up:
                    return ToolkitKey::Up;
                case Key::PageUp:
                    return ToolkitKey::PageUp;
                case Key::PageDown:
                    return ToolkitKey::PageDown;
                case Key::Home:
                    return ToolkitKey::Home;
                case Key::End:
                    return ToolkitKey::End;
                case Key::Pause:
                    return ToolkitKey::Pause;
                case Key::KeypadDecimal:
                    return ToolkitKey::Period;
                case Key::KeypadDivide:
                    return ToolkitKey::Divide;
                case Key::KeypadMultiply:
                    return ToolkitKey::Multiply;
                case Key::KeypadSubtract:
                    return ToolkitKey::Subtract;
                case Key::KeypadAdd:
                    return ToolkitKey::Add;
                case Key::KeypadEnter:
                    return ToolkitKey::Enter;
                case Key::KeypadEqual:
                    return ToolkitKey::Equal;
                case Key::LeftShift:
                    return ToolkitKey::LShift;
                case Key::LeftControl:
                    return ToolkitKey::LControl;
                case Key::LeftAlt:
                    return ToolkitKey::LAlt;
                case Key::LeftSuper:
                    return ToolkitKey::LSystem;
                case Key::RightShift:
                    return ToolkitKey::RShift;
                case Key::RightControl:
                    return ToolkitKey::RControl;
                case Key::RightAlt:
                    return ToolkitKey::RAlt;
                case Key::RightSuper:
                    return ToolkitKey::RSystem;
                case Key::Menu:
                    return ToolkitKey::Menu;
                default:
                    return ToolkitKey::Unknown;
            }
        }

        std::optional<tgui::Event::MouseButton> mouse_button(const Input::MouseButton button) {
            switch (button) {
                case Input::MouseButton::Left:
                    return tgui::Event::MouseButton::Left;
                case Input::MouseButton::Right:
                    return tgui::Event::MouseButton::Right;
                case Input::MouseButton::Middle:
                    return tgui::Event::MouseButton::Middle;
                default:
                    return {};
            }
        }

        int coordinate(const double value) {
            const auto rounded = std::floor(value);
            if (!std::isfinite(rounded) || rounded < std::numeric_limits<int>::min() || rounded > std::numeric_limits<int>::max())
                throw Exceptions::invalid_args(CE_HERE, "TGUI input requires a finite representable logical coordinate");
            return static_cast<int>(rounded);
        }

        const Input::PointerEvent& position(const std::optional<Input::PointerEvent>& value) {
            if (!value)
                throw Exceptions::invalid_args(CE_HERE, "TGUI pointer input requires an observation-time position");
            return *value;
        }
    }

    std::optional<tgui::Event> translate_event(const Input::InputRecord& record) {
        tgui::Event event{};
        if (const auto* text = std::get_if<Input::TextEvent>(&record.data)) {
            if (text->codepoint > 0x10FFFF || (text->codepoint >= 0xD800 && text->codepoint <= 0xDFFF))
                throw Exceptions::invalid_args(CE_HERE, "TGUI text input requires a Unicode scalar value");
            event.type = tgui::Event::Type::TextEntered;
            event.text = {text->codepoint};
            return event;
        }
        if (const auto* button = std::get_if<Input::ButtonEvent>(&record.data)) {
            if (record.device_kind == Input::DeviceKind::Keyboard) {
                if (button->phase == Input::ButtonPhase::Release)
                    return {};
                const auto key = keyboard_key(button->key);
                if (key == ToolkitKey::Unknown)
                    return {};
                event.type = tgui::Event::Type::KeyPressed;
                event.key = {key, Input::has_modifier(button->modifiers, Input::Modifiers::Alt),
                             Input::has_modifier(button->modifiers, Input::Modifiers::Control),
                             Input::has_modifier(button->modifiers, Input::Modifiers::Shift),
                             Input::has_modifier(button->modifiers, Input::Modifiers::Super)};
                return event;
            }
            if (record.device_kind == Input::DeviceKind::Mouse && button->phase != Input::ButtonPhase::Repeat) {
                const auto translated = mouse_button(button->mouse_button);
                if (!translated)
                    return {};
                const auto& pointer = position(button->position);
                event.type = button->phase == Input::ButtonPhase::Press ? tgui::Event::Type::MouseButtonPressed
                                                                        : tgui::Event::Type::MouseButtonReleased;
                event.mouseButton = {*translated, coordinate(pointer.x), coordinate(pointer.y)};
                return event;
            }
            return {};
        }
        if (record.device_kind != Input::DeviceKind::Mouse)
            return {};
        if (const auto* pointer = std::get_if<Input::PointerEvent>(&record.data)) {
            event.type = tgui::Event::Type::MouseMoved;
            event.mouseMove = {coordinate(pointer->x), coordinate(pointer->y)};
            return event;
        }
        if (const auto* scroll = std::get_if<Input::ScrollEvent>(&record.data)) {
            if (!std::isfinite(scroll->x) || !std::isfinite(scroll->y) || std::abs(scroll->y) > std::numeric_limits<float>::max())
                throw Exceptions::invalid_args(CE_HERE, "TGUI wheel input requires a finite representable offset");
            if (scroll->y == 0.0)
                return {};
            const auto& pointer = position(scroll->position);
            event.type = tgui::Event::Type::MouseWheelScrolled;
            event.mouseWheel = {static_cast<float>(scroll->y), coordinate(pointer.x), coordinate(pointer.y)};
            return event;
        }
        return {};
    }
}
