#include "developer-console-routing.h"
#include "developer-console.h"
#include "developer-console-model.h"
#include "ui-routing.h"
#include <cheryl/tools/console/input.h>

#ifdef CHERYL_DEMO_TGUI
#include "tgui-controls.h"
#include <ui/tgui/session.h>
#endif

#include <internals/exceptions.h>
#include <ui/rmlui/session.h>

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

#include <string>
#include <utility>
#include <vector>

namespace DemoConsole {
    namespace {
        Rml::Element& required(Rml::ElementDocument& document, const char* id) {
            auto* element = document.GetElementById(id);
            if (!element)
                throw CE::Exceptions::failed_operation(CE_HERE, "The demo RML document is missing element: " + std::string{id});
            return *element;
        }
        enum class Focus { None, RmlPointer, RmlField, TguiPointer, TguiField, Console };
        enum class Pointer { None, Original, Tgui, Console };
        bool rml_focus(Focus focus) { return focus == Focus::RmlPointer || focus == Focus::RmlField || focus == Focus::Console; }
        bool tgui_focus(Focus focus) { return focus == Focus::TguiPointer || focus == Focus::TguiField; }
        struct TguiStep {
            bool pointer = false;
            bool release = false;
            bool visible = false;
            bool effects = false;
        };
    }

    struct Routing::State final : Rml::EventListener {
        CE::UI::RmlUi::Session& session;
        Rml::ElementDocument& document;
        View* console;
        std::unique_ptr<CE::Tools::Console::Presentation::Input> console_input;
        Rml::Element& panel;
        Rml::ElementFormControlInput& field;
        Rml::Element& reset;
        Rml::Element& image;
        Rml::Element& picture;
        Input input;
        DemoUiRouting::Effects effects;
        bool visible = true;
        bool alternate_image = false;
        bool left_down = false;
        bool left_quarantined = false;
        Pointer gesture = Pointer::None;
        Focus last_focus = Focus::None;

        State(CE::UI::RmlUi::Session& selected_session, Rml::ElementDocument& original, View* selected_console)
        : session(selected_session), document(original), console(selected_console), panel(required(original, "panel")),
          field(dynamic_cast<Rml::ElementFormControlInput&>(required(original, "field"))), reset(required(original, "reset")),
          image(required(original, "change-image")), picture(required(original, "image")) {
            if (console)
                console_input = std::make_unique<CE::Tools::Console::Presentation::Input>(session, *console);
            reset.AddEventListener("click", this);
            try {
                image.AddEventListener("click", this);
            } catch (...) {
                reset.RemoveEventListener("click", this);
                throw;
            }
        }

        ~State() override {
            reset.RemoveEventListener("click", this);
            image.RemoveEventListener("click", this);
        }

        bool console_visible() const { return console && console->visible(); }

        void ProcessEvent(Rml::Event& event) override {
            if (event.GetCurrentElement() == &reset)
                effects.request_reset();
            else if (event.GetCurrentElement() == &image)
                effects.request_image();
        }

        bool inside(const Rml::Element& element, const CE::Input::Events::Pointer& position) const {
            auto* hit = session.context().GetElementAtPoint({static_cast<float>(position.x), static_cast<float>(position.y)});
            for (; hit; hit = hit->GetParentNode())
                if (hit == &element)
                    return true;
            return false;
        }

        bool handle_input(std::span<const CE::Input::Events::Record> records, bool drawable, DemoTguiControls* tgui) {
            if (!console_input)
                return drain_input(records, drawable, tgui);
            bool allowed = false;
            const bool console_allowed = console_input->handle_input(records, drawable, [&](auto batch) {
                allowed = drain_input(batch, drawable, tgui);
            });
            return console_allowed && allowed;
        }

        bool drain_input(std::span<const CE::Input::Events::Record> records, bool drawable, DemoTguiControls* tgui) {
            using namespace CE::Input;
            bool tgui_owned = false;
            bool tgui_visible = false;
#ifdef CHERYL_DEMO_TGUI
            if (tgui) {
                tgui_owned = tgui->session().owns_keyboard_focus();
                tgui_visible = tgui->visible();
            }
#else
            static_cast<void>(tgui);
#endif
            Focus desired = session.owns_keyboard_focus()
                ? (console_visible() ? Focus::Console : session.context().GetFocusElement() == &field ? Focus::RmlField : Focus::RmlPointer)
                : tgui_owned ? Focus::TguiPointer : Focus::None;
            input.begin(session.owns_keyboard_focus() || tgui_owned || console_visible());
            bool blocked = console_visible() || !drawable || (console_input && console_input->transitioned());
            effects.begin(!blocked);
#ifdef CHERYL_DEMO_TGUI
            if (tgui)
                tgui->begin_input(!blocked);
#endif
            bool release_tgui = false;
            bool rml_retired = false;
            bool tgui_retired = false;
            const auto retire_rml = [&] {
                session.release_keyboard_focus();
                rml_retired = true;
            };
            const auto transition = [&] {
                blocked = true;
                effects.cancel();
#ifdef CHERYL_DEMO_TGUI
                if (tgui)
                    tgui->cancel_effects();
#endif
                left_quarantined = left_quarantined || left_down;
                input.suppress();
            };
            const auto handoff = [&](Focus next) {
                if (console_input && next != Focus::Console)
                    console_input->cancel_pending_open();
                transition();
                retire_rml();
                release_tgui = true;
                tgui_retired = true;
                desired = next;
            };
            if (!drawable || (rml_focus(last_focus) && !session.owns_keyboard_focus()) || (tgui_focus(last_focus) && !tgui_owned)) {
                handoff(Focus::None);
                if (console)
                    console->hide();
            }
            if (left_quarantined)
                input.suppress();
            const auto activity = console ? console->activity() : 0;
            bool observed_visible = console_visible();
            const auto synchronize = [&] {
                if (observed_visible != console_visible()) {
                    handoff(Focus::None);
                    observed_visible = console_visible();
                }
                if (console_visible() && (!drawable || !session.owns_keyboard_focus())) {
                    handoff(Focus::None);
                    console->hide();
                    observed_visible = false;
                }
            };
            std::vector<TguiStep> steps(records.size());
            std::size_t index = 0;
            synchronize();
            session.handle_input(records, false, [&](const Events::Record& record) {
                synchronize();
                input.observe(record);
                const auto toggle = console_input ? console_input->route_toggle(record) : std::nullopt;
                const auto* button = std::get_if<Events::Button>(&record.data);
                const bool keyboard = record.is_text() || record.device_kind == DeviceKind::Keyboard;
                const bool left = button && record.device_kind == DeviceKind::Mouse && button->mouse_button == MouseButton::Left;
                const bool press = left && button->phase == Events::ButtonPhase::Press;
                const bool release = left && button->phase == Events::ButtonPhase::Release;
                const bool stale_release = release && (left_quarantined || !left_down);
                if (press)
                    left_down = true;
                if (release)
                    left_down = false;
                if (button && record.device_kind == DeviceKind::Keyboard && button->phase == Events::ButtonPhase::Press) {
                    if (toggle) {
                        handoff(*toggle && drawable ? Focus::Console : Focus::None);
                        console->hide();
                    } else if (button->key == KeyboardKey::F4 && visible && drawable) {
                        const bool opening = desired != Focus::RmlField;
                        handoff(opening ? Focus::RmlField : Focus::None);
                        if (console)
                            console->hide();
                    } else if (button->key == KeyboardKey::F2 && tgui_visible && drawable) {
                        const bool opening = !tgui_focus(desired);
                        handoff(opening ? Focus::TguiField : Focus::None);
                        if (console)
                            console->hide();
                    } else if (button->key == KeyboardKey::F6) {
                        visible = !visible;
                        if (visible)
                            document.Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
                        else {
                            document.Hide();
                            transition();
                            if (rml_focus(desired) && desired != Focus::Console)
                                handoff(Focus::None);
                        }
                    } else if (button->key == KeyboardKey::F3 && tgui) {
                        tgui_visible = !tgui_visible;
                        if (!tgui_visible) {
                            transition();
                            if (tgui_focus(desired))
                                handoff(Focus::None);
                        }
                    } else if (button->key == KeyboardKey::Escape && desired != Focus::None) {
                        handoff(Focus::None);
                        if (console)
                            console->hide();
                    }
                }
                const Events::Pointer* position = std::get_if<Events::Pointer>(&record.data);
                if (button && button->position)
                    position = &*button->position;
                if (const auto* scroll = std::get_if<Events::Scroll>(&record.data); scroll && scroll->position)
                    position = &*scroll->position;
                Pointer selected = Pointer::None;
                if (drawable && (!record.ui_consumed || release) && record.device_kind == DeviceKind::Mouse) {
                    if (console_visible()) {
                        if (position && console->inside(*position))
                            selected = Pointer::Console;
                        else if (press) {
                            handoff(Focus::None);
                            console->hide();
                        }
                    } else if (!blocked && position) {
                        if (visible && inside(panel, *position))
                            selected = Pointer::Original;
#ifdef CHERYL_DEMO_TGUI
                        else if (tgui && tgui_visible && tgui->contains(*position))
                            selected = Pointer::Tgui;
#endif
                    }
                    if (left_down && !press && !left_quarantined && gesture != Pointer::None &&
                        std::holds_alternative<Events::Pointer>(record.data))
                        selected = gesture;
                    if (left_quarantined)
                        selected = Pointer::None;
                    // A real release clears only its old native gesture, with effects disabled after a takeover.
                    if (release)
                        selected = gesture;
                }
                if (press) {
                    if (blocked && !console_visible())
                        left_quarantined = true;
                    gesture = selected;
                    if (!blocked && !left_quarantined) {
                        auto next = selected == Pointer::Original ? Focus::RmlPointer
                            : selected == Pointer::Tgui ? Focus::TguiPointer : Focus::None;
                        if (selected == Pointer::Original && position && inside(field, *position))
                            next = Focus::RmlField;
#ifdef CHERYL_DEMO_TGUI
                        if (selected == Pointer::Tgui && position && tgui->field_contains(*position))
                            next = Focus::TguiField;
#endif
                        if (rml_focus(desired) && !rml_focus(next))
                            retire_rml();
                        if (tgui_focus(desired) && !tgui_focus(next)) {
                            release_tgui = true;
                            tgui_retired = true;
                        }
                        if (desired != next)
                            input.suppress();
                        desired = next;
                    }
                }
                if (selected != Pointer::None || stale_release || left_quarantined)
                    input.suppress();
                const bool ordinary_effect = drawable && !record.ui_consumed && !blocked && !stale_release && !left_quarantined;
                effects.select(ordinary_effect && (keyboard || selected == Pointer::Original));
                if (keyboard && rml_retired)
                    session.release_keyboard_focus();
                if (console)
                    console->select_input(!(console_input && console_input->transitioned()) &&
                                          !record.ui_consumed && !stale_release && !left_quarantined &&
                                          ((keyboard && !rml_retired) || selected == Pointer::Console));
                observed_visible = console_visible();
                const bool retire_tgui = std::exchange(release_tgui, false) || (keyboard && tgui_retired);
                steps[index++] = {selected == Pointer::Tgui, retire_tgui, tgui_visible,
                                  ordinary_effect && (keyboard || selected == Pointer::Tgui)};
                if (release) {
                    gesture = Pointer::None;
                    left_quarantined = false;
                }
                return selected == Pointer::Original || selected == Pointer::Console;
            });
            synchronize();
            if (console)
                console->select_input(false);
#ifdef CHERYL_DEMO_TGUI
            if (tgui) {
                index = 0;
                tgui->session().handle_input(records, false, [&](const Events::Record&) {
                    const auto& step = steps[index++];
                    tgui->set_visible(step.visible);
                    if (step.release)
                        tgui->session().release_keyboard_focus();
                    tgui->select_effects(step.effects);
                    return step.pointer;
                });
                if (release_tgui)
                    tgui->session().release_keyboard_focus();
                tgui->set_visible(tgui_visible);
            }
#endif
            if (drawable) {
                if (desired == Focus::Console && console && !console_visible())
                    console->open();
                else if ((desired == Focus::RmlField || desired == Focus::RmlPointer) && visible) {
                    if (!session.owns_keyboard_focus())
                        session.request_keyboard_focus();
                    if (desired == Focus::RmlField && !field.Focus(true)) {
                        session.release_keyboard_focus();
                        throw CE::Exceptions::failed_operation(CE_HERE, "Cannot focus the demo RmlUi text input");
                    }
                }
#ifdef CHERYL_DEMO_TGUI
                else if (tgui && tgui_visible && tgui_focus(desired)) {
                    if (!tgui->session().owns_keyboard_focus())
                        tgui->session().request_keyboard_focus();
                    if (desired == Focus::TguiField)
                        tgui->focus_field();
                }
#endif
            }
            last_focus = desired;
            if ((console && activity != console->activity()) || session.owns_keyboard_focus() || tgui_focus(desired))
                input.suppress();
            if (effects.finish_image()) {
                alternate_image = !alternate_image;
                picture.SetAttribute("src", std::string{alternate_image ? "../textures/ui/quadrants-alt.png" : "../textures/ui/quadrants.png"});
            }
#ifdef CHERYL_DEMO_TGUI
            if (tgui)
                tgui->finish_input();
#endif
            return input.gameplay_allowed();
        }
    };

    Routing::Routing(CE::UI::RmlUi::Session& session, Rml::ElementDocument& original, View* console)
    : state_(std::make_unique<State>(session, original, console)) {}
    Routing::~Routing() = default;
    bool Routing::handle_input(std::span<const CE::Input::Events::Record> records, bool drawable, DemoTguiControls* tgui) {
        return state_->handle_input(records, drawable, tgui);
    }
    bool Routing::consume_reset() noexcept { return state_->effects.consume_reset(); }
    bool Routing::original_visible() const noexcept { return state_->visible; }
    void Routing::retire() {
        if (state_->console_input)
            state_->console_input->retire();
        state_->effects.cancel();
    }
}
