#include "host.h"
#include <cheryl/tools/console/view.h>
#include <cheryl/tools/console/input.h>
#include <cheryl/text/font-service.h>
#include <cheryl/ui/rmlui/session.h>

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>

#include <string_view>
#include <initializer_list>

namespace Independent {
    namespace {
        void require(bool condition, const char* message) {
            if (!condition) throw std::runtime_error(message);
        }

        class Input final : public CE::Input::System {
            CE::Input::Actions::Bindings bindings_;

        public:
            void initialize(CE::Platform::Window&) override {}
            void poll() override {}
            void deinitialize() override {}
            CE::Input::Actions::Bindings& bindings() override { return bindings_; }
            CE::Input::DeviceId keyboard_id() const override { return 1; }
            CE::Input::DeviceId mouse_id() const override { return 2; }
            CE::Input::DeviceId gamepad_id() const override { return 3; }
            bool supports(CE::Input::Capture::Channel) const override { return true; }
            bool supports_focus() const override { return true; }

            std::shared_ptr<const CE::Input::PollSnapshot> emit(std::initializer_list<CE::Input::Events::Payload> records) {
                using namespace CE::Input;
                begin_input_poll();
                for (const auto& record : records) {
                    const auto* button = std::get_if<Events::Button>(&record);
                    const bool mouse = std::holds_alternative<Events::Pointer>(record) ||
                        (button && button->mouse_button != MouseButton::Unknown);
                    capture_buffer().record(mouse ? mouse_id() : keyboard_id(), mouse ? DeviceKind::Mouse : DeviceKind::Keyboard, record);
                }
                return publish_input();
            }

            static CE::Input::Events::Button key(CE::Input::KeyboardKey selected,
                CE::Input::Events::ButtonPhase phase = CE::Input::Events::ButtonPhase::Press,
                CE::Input::Modifiers modifiers = CE::Input::Modifiers::None) {
                return {CE::Input::control_id(selected), phase, modifiers, -1, -1, selected};
            }

            std::shared_ptr<const CE::Input::PollSnapshot> text(std::u32string_view value) {
                begin_input_poll();
                for (const auto character : value)
                    capture_buffer().record(keyboard_id(), CE::Input::DeviceKind::Keyboard, CE::Input::Events::Text{character});
                return publish_input();
            }

            std::shared_ptr<const CE::Input::PollSnapshot> enter() {
                begin_input_poll();
                capture_buffer().record(keyboard_id(), CE::Input::DeviceKind::Keyboard,
                    CE::Input::Events::Button{800, CE::Input::Events::ButtonPhase::Press,
                        CE::Input::Modifiers::None, -1, -1, CE::Input::KeyboardKey::Enter});
                return publish_input();
            }
        };
    }

    void exercise_view() {
        Input input;
        CE::Text::FontService fonts;
        CE::UI::RmlUi::Session session(input, fonts, 7);
        session.set_view({1000, 800}, {1000, 800});
        auto host = std::make_shared<Host>();
        Core::Presentation::Model model(host);
        {
            Core::Presentation::View view(session, model);
            Core::Presentation::Input routing(session, view);
            auto* document = session.context().GetDocument(0);
            require(document != nullptr && !view.visible(), "View did not create its hidden native document");
            auto* field = dynamic_cast<Rml::ElementFormControlInput*>(document->GetElementById("console-command"));
            auto* output = document->GetElementById("console-output");
            auto* scope = document->GetElementById("console-scope");
            require(field && output && scope, "Embedded View controls are missing");
            require(scope->GetInnerRML().find("&lt;owner-thread&gt;") != Rml::String::npos,
                "View did not escape the host scope");
            require(!routing.handle_input(input.emit({Input::key(CE::Input::KeyboardKey::GraveAccent)})->records),
                "Default opening admitted same-batch gameplay");
            session.update_time(0);
            require(view.visible() && session.owns_keyboard_focus(), "View did not acquire routed keyboard focus");
            const auto command = input.text(U"simulation.step_limit.set 6");
            require(!command->records.empty() && command->records.front().target == 7,
                "Command input did not receive the Session routing identity");
            require(!routing.handle_input(command->records), "Console typing admitted gameplay");
            require(field->GetValue() == "simulation.step_limit.set 6", "Committed input did not reach the native View");
            require(!routing.handle_input(input.enter()->records), "Console submission admitted gameplay");
            require(model.entries().size() == 1 && field->GetValue().empty(), "View did not submit through the Model");
            require(host->simulation().step_limit == 4, "View bypassed owner progression");
            model.pump();
            view.update();
            session.update_time(0);
            require(host->simulation().step_limit == 6 && host->simulation().writes == 1 &&
                model.entries().back().result.stage == Core::Stage::Succeeded, "View command did not reach its real owner");
            const auto rendered = output->GetInnerRML();
            require(rendered.find("simulation.step_limit.set 6") != Rml::String::npos &&
                rendered.find("effective: 6") != Rml::String::npos, "View did not present the retained command result");
            const auto activity = view.activity();
            require(!routing.handle_input(input.emit({Input::key(CE::Input::KeyboardKey::GraveAccent,
                CE::Input::Events::ButtonPhase::Press, CE::Input::Modifiers::Shift)})->records),
                "Shift closing admitted gameplay");
            require(!view.visible() && !session.owns_keyboard_focus() && view.activity() > activity,
                "Hiding the View did not release focus");
            require(input.text(U"x")->records.empty(), "Hidden View retained committed-text capture");
            require(!routing.handle_input(input.emit({Input::key(CE::Input::KeyboardKey::Enter, CE::Input::Events::ButtonPhase::Release)})->records),
                "Stale command key release admitted gameplay");
            require(routing.handle_input(input.emit({})->records), "Released command key did not recover gameplay");
            require(!routing.handle_input(input.emit({Input::key(CE::Input::KeyboardKey::GraveAccent,
                CE::Input::Events::ButtonPhase::Press, CE::Input::Modifiers::Shift)})->records), "Shift opening admitted gameplay");
            view.update();
            session.update_time(0);
            require(view.visible() && session.owns_keyboard_focus() && output->GetInnerRML() == rendered &&
                model.entries().size() == 1 && host->simulation().writes == 1, "Reopening lost or repeated the retained result");
            require(!routing.handle_input(input.emit({Input::key(CE::Input::KeyboardKey::GraveAccent, CE::Input::Events::ButtonPhase::Repeat),
                CE::Input::Events::Text{U'~', CE::Input::KeyboardKey::GraveAccent},
                Input::key(CE::Input::KeyboardKey::GraveAccent, CE::Input::Events::ButtonPhase::Release)})->records),
                "Repeat/release admitted gameplay");
            require(view.visible() && field->GetValue().empty(), "Repeat/release toggled or inserted associated text");
            require(!routing.handle_input(input.text(U"`~paste")->records), "Pasted text admitted gameplay");
            require(field->GetValue() == "`~paste", "Unattributed backticks were filtered as a shortcut");
            field->SetValue({});
            auto successor = input.routing().focus(99);
            require(!routing.handle_input(input.emit({CE::Input::Events::Text{U'x'}})->records), "Preemption admitted gameplay");
            require(!view.visible() && successor.owns_focus(), "Console erased its successor or retained preempted focus");
            successor.reset();
            require(routing.handle_input(input.emit({})->records), "Focus release did not recover gameplay");
            routing.retire();
            routing.retire();
        }
        {
            Core::Presentation::View view(session, model);
            Core::Presentation::Input routing(session, view, {CE::Input::KeyboardKey::F10});
            require(routing.handle_input(input.emit({Input::key(CE::Input::KeyboardKey::GraveAccent),
                Input::key(CE::Input::KeyboardKey::GraveAccent, CE::Input::Events::ButtonPhase::Release)})->records),
                "Override retained the default binding");
            require(!routing.handle_input(input.emit({Input::key(CE::Input::KeyboardKey::W),
                CE::Input::Events::Pointer{10, 10}, Input::key(CE::Input::KeyboardKey::F10),
                CE::Input::Events::Pointer{20, 20}, Input::key(CE::Input::KeyboardKey::A)})->records),
                "Override transition leaked earlier or later gameplay");
            require(view.visible(), "Override did not open the ordinary View");
            require(!routing.handle_input(input.emit({Input::key(CE::Input::KeyboardKey::F10),
                Input::key(CE::Input::KeyboardKey::W, CE::Input::Events::ButtonPhase::Release),
                Input::key(CE::Input::KeyboardKey::A, CE::Input::Events::ButtonPhase::Release)})->records),
                "Stale held release admitted gameplay");
            require(routing.handle_input(input.emit({Input::key(CE::Input::KeyboardKey::W),
                Input::key(CE::Input::KeyboardKey::W, CE::Input::Events::ButtonPhase::Release)})->records),
                "Fresh gameplay gesture did not recover");
            routing.retire();
        }
        {
            Core::Presentation::View view(session, model);
            Core::Presentation::Input routing(session, view, {std::nullopt});
            require(routing.handle_input(input.emit({Input::key(CE::Input::KeyboardKey::GraveAccent)})->records)
                && !view.visible(), "Disabled toggle opened the View");
            view.open();
            require(!routing.handle_input(input.emit({})->records), "Explicit opening admitted gameplay");
            require(!routing.handle_input(input.emit({CE::Input::Events::Text{U'`', CE::Input::KeyboardKey::GraveAccent}})->records),
                "Explicitly opened disabled binding admitted gameplay");
            auto* field = dynamic_cast<Rml::ElementFormControlInput*>(
                session.context().GetDocument(0)->GetElementById("console-command"));
            require(field && field->GetValue() == "`", "Disabled binding filtered ordinary command text");
            routing.retire();
        }
        model.close();
        require(!model.accepting() && model.entries().size() == 1 &&
            model.entries().back().result.stage == Core::Stage::Succeeded, "Close discarded the View's retained result");
        session.update_time(0);
        require(session.context().GetNumDocuments() == 0, "View teardown retained a native document");
        session.close_after_quiescence();
        require(Rml::GetSystemInterface() == nullptr, "UI Session teardown retained toolkit globals");
    }
}
