#include "host.h"
#include <cheryl/tools/console/view.h>
#include <cheryl/text/font-service.h>
#include <cheryl/ui/rmlui/session.h>

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>

#include <string_view>

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
            auto* document = session.context().GetDocument(0);
            require(document != nullptr && !view.visible(), "View did not create its hidden native document");
            auto* field = dynamic_cast<Rml::ElementFormControlInput*>(document->GetElementById("console-command"));
            auto* output = document->GetElementById("console-output");
            auto* scope = document->GetElementById("console-scope");
            require(field && output && scope, "Embedded View controls are missing");
            require(scope->GetInnerRML().find("&lt;owner-thread&gt;") != Rml::String::npos,
                "View did not escape the host scope");
            view.open();
            session.update_time(0);
            require(view.visible() && session.owns_keyboard_focus(), "View did not acquire routed keyboard focus");
            const auto command = input.text(U"simulation.step_limit.set 6");
            require(!command->records.empty() && command->records.front().target == 7,
                "Command input did not receive the Session routing identity");
            session.handle_input(command->records, false);
            require(field->GetValue() == "simulation.step_limit.set 6", "Committed input did not reach the native View");
            session.handle_input(input.enter()->records, false);
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
            view.hide();
            require(!view.visible() && !session.owns_keyboard_focus() && view.activity() > activity,
                "Hiding the View did not release focus");
            require(input.text(U"x")->records.empty(), "Hidden View retained committed-text capture");
            view.open();
            view.update();
            session.update_time(0);
            require(view.visible() && session.owns_keyboard_focus() && output->GetInnerRML() == rendered &&
                model.entries().size() == 1 && host->simulation().writes == 1, "Reopening lost or repeated the retained result");
            view.hide();
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
