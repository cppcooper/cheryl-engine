#include <cheryl/tools/console/input.h>
#include <cheryl/tools/console/view.h>

#include <ui/rmlui/session.h>

#include <algorithm>
#include <map>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace CE::Tools::Console::Presentation {
    namespace Events = CE::Input::Events;

    struct Input::State {
        struct Held {
            bool gameplay = false;
            bool ui = false;
        };
        using Control = std::tuple<CE::Input::DeviceId, CE::Input::DeviceKind, CE::Input::ControlId>;
        CE::UI::RmlUi::Session& session;
        View& view;
        InputOptions options;
        std::map<Control, Held> held;
        std::map<Control, bool> pointer_buttons;
        bool visible = false;
        bool transition = false;
        bool handling = false;
        bool retired = false;
        bool pending_open = false;
        std::vector<std::uint64_t> toggles;
        std::vector<std::uint64_t> escapes;

        State(CE::UI::RmlUi::Session& selected, View& console, InputOptions configured)
            : session(selected), view(console), options(configured), visible(console.visible()) {
            if (options.toggle_key && (*options.toggle_key == CE::Input::KeyboardKey::Unknown ||
                *options.toggle_key > CE::Input::KeyboardKey::Menu))
                throw std::invalid_argument("Console toggle requires a known physical key");
        }

        void quarantine() {
            for (auto& [control, status] : held) {
                status.gameplay = true;
                status.ui = true;
            }
            for (auto& [control, selected] : pointer_buttons)
                selected = false;
        }

        bool handle(std::span<const CE::Input::Events::Record> records, bool drawable,
                    const std::function<void(std::span<const CE::Input::Events::Record>)>& drain) {
            using namespace CE::Input;
            if (retired)
                throw std::logic_error("Console input has retired");
            if (handling)
                throw std::logic_error("Console input cannot be reentered");
            handling = true;
            struct Reset {
                bool& flag;
                ~Reset() { flag = false; }
            } reset{handling};
            transition = visible != view.visible();
            pending_open = view.visible();
            if (!drawable || (pending_open && !session.owns_keyboard_focus())) {
                transition = transition || pending_open;
                pending_open = false;
            }
            const bool external_transition = transition;
            toggles.clear();
            escapes.clear();
            bool console_selected = pending_open;
            for (const auto& record : records) {
                const auto* button = std::get_if<Events::Button>(&record.data);
                if (record.ui_consumed || !button || record.device_kind != DeviceKind::Keyboard ||
                    button->phase != Events::ButtonPhase::Press)
                    continue;
                if (options.toggle_key && button->key == *options.toggle_key) {
                    toggles.push_back(record.sequence);
                    console_selected = !console_selected;
                    transition = true;
                } else if (button->key == KeyboardKey::Escape && console_selected) {
                    escapes.push_back(record.sequence);
                    console_selected = false;
                    transition = true;
                }
            }
            bool allowed = drawable && !view.visible() && !transition && !session.owns_keyboard_focus();
            for (const auto& [control, status] : held)
                if (status.gameplay)
                    allowed = false;
            if (transition) {
                quarantine();
                if (external_transition) {
                    view.hide();
                    session.release_keyboard_focus();
                }
            }
            std::vector<Events::Record> batch(records.begin(), records.end());
            bool keyboard_retired = external_transition;
            for (auto& record : batch) {
                const auto* button = std::get_if<Events::Button>(&record.data);
                const auto* text = std::get_if<Events::Text>(&record.data);
                const bool toggle = options.toggle_key &&
                    ((button && record.device_kind == DeviceKind::Keyboard && button->key == *options.toggle_key) ||
                     (text && text->source_key == options.toggle_key));
                const bool escape = std::ranges::find(escapes, record.sequence) != escapes.end();
                if (button && record.device_kind == DeviceKind::Keyboard && button->phase == Events::ButtonPhase::Press &&
                    !record.ui_consumed && (escape || (options.toggle_key && button->key == *options.toggle_key)))
                    keyboard_retired = true;
                const bool keyboard = record.is_text() || record.device_kind == DeviceKind::Keyboard;
                record.ui_consumed = record.ui_consumed || (keyboard ? keyboard_retired : transition) || !drawable || toggle;
                if (toggle)
                    allowed = false;
                if (button && !toggle && !drain) {
                    const auto identity = record.device_kind == DeviceKind::Keyboard && button->key != KeyboardKey::Unknown
                        ? control_id(button->key)
                        : record.device_kind == DeviceKind::Mouse && button->mouse_button != MouseButton::Unknown
                            ? control_id(button->mouse_button) : button->button;
                    const Control control{record.device, record.device_kind, identity};
                    auto found = held.find(control);
                    if (found != held.end()) {
                        allowed = allowed && !found->second.gameplay;
                        record.ui_consumed = record.ui_consumed || found->second.ui;
                    }
                    if (button->phase == Events::ButtonPhase::Release) {
                        if (found != held.end())
                            held.erase(found);
                    } else {
                        auto& status = held[control];
                        status.gameplay = status.gameplay || transition || view.visible();
                        status.ui = status.ui || transition;
                    }
                }
            }
            if (!allowed)
                for (auto& record : batch)
                    record.to_gameplay = false;
            const auto activity = view.activity();
            try {
                if (drain)
                    drain(batch);
                else {
                    view.select_input(!transition);
                    session.handle_input(batch, false, [&](const Events::Record& record) {
                        static_cast<void>(route_toggle(record));
                        view.select_input(!transition && !record.ui_consumed);
                        const auto* button = std::get_if<Events::Button>(&record.data);
                        const bool mouse = button && record.device_kind == DeviceKind::Mouse;
                        const Control control{record.device, record.device_kind,
                            mouse && button->mouse_button != MouseButton::Unknown
                                ? control_id(button->mouse_button) : button ? button->button : invalid_control};
                        if (mouse && button->phase == Events::ButtonPhase::Release && pointer_buttons.erase(control))
                            return true;
                        if (record.ui_consumed)
                            return false;
                        if (std::holds_alternative<Events::Pointer>(record.data)) {
                            bool captured = false, selected = false;
                            for (const auto& [pressed, active] : pointer_buttons)
                                if (std::get<0>(pressed) == record.device) {
                                    captured = true;
                                    selected = selected || active;
                                }
                            if (captured)
                                return view.visible() && selected;
                        }
                        const Events::Pointer* position = std::get_if<Events::Pointer>(&record.data);
                        if (button && button->position)
                            position = &*button->position;
                        if (const auto* scroll = std::get_if<Events::Scroll>(&record.data); scroll && scroll->position)
                            position = &*scroll->position;
                        const bool selected = position && view.inside(*position);
                        if (selected && mouse && button->phase == Events::ButtonPhase::Press)
                            pointer_buttons[control] = true;
                        return selected;
                    });
                    view.select_input(true);
                }
                if (transition && pending_open && drawable)
                    view.open();
            } catch (...) {
                pending_open = false;
                view.select_input(false);
                view.hide();
                visible = view.visible();
                quarantine();
                throw;
            }
            if (view.activity() != activity || view.visible() != visible) {
                transition = true;
                allowed = false;
                quarantine();
            }
            visible = view.visible();
            return allowed && !visible && !session.owns_keyboard_focus();
        }

        std::optional<bool> route_toggle(const CE::Input::Events::Record& record) {
            if (!handling)
                throw std::logic_error("Console toggle routing requires an active drain");
            const auto escape = std::ranges::find(escapes, record.sequence);
            if (escape != escapes.end()) {
                escapes.erase(escape);
                if (!pending_open)
                    return std::nullopt;
                pending_open = false;
                view.hide();
                session.release_keyboard_focus();
                return false;
            }
            const auto found = std::ranges::find(toggles, record.sequence);
            if (found == toggles.end())
                return std::nullopt;
            toggles.erase(found);
            pending_open = !pending_open;
            view.hide();
            session.release_keyboard_focus();
            return pending_open;
        }
    };

    Input::Input(CE::UI::RmlUi::Session& session, View& view, InputOptions options)
        : state_(std::make_unique<State>(session, view, options)) {}
    Input::~Input() = default;
    bool Input::handle_input(std::span<const CE::Input::Events::Record> records, bool drawable,
                            const std::function<void(std::span<const CE::Input::Events::Record>)>& drain) {
        return state_->handle(records, drawable, drain);
    }
    bool Input::transitioned() const noexcept { return state_->transition; }
    std::optional<bool> Input::route_toggle(const CE::Input::Events::Record& record) { return state_->route_toggle(record); }
    void Input::cancel_pending_open() noexcept { state_->pending_open = false; }
    void Input::retire() {
        if (state_->retired)
            return;
        if (state_->handling)
            throw std::logic_error("Console input cannot retire during drain");
        state_->view.hide();
        state_->pending_open = false;
        state_->held.clear();
        state_->pointer_buttons.clear();
        state_->retired = true;
    }
}
