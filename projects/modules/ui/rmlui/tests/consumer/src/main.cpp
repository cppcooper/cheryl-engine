#include <cheryl/ui/rmlui/input.h>
#include <cheryl/ui/rmlui/scene.h>
#include <cheryl/ui/rmlui/session.h>
#include "../../support/document.h"

#include <RmlUi/Core/Core.h>

namespace {
    class Input final : public CE::Input::iInputSystem {
        CE::Input::InputBindings bindings_;

    public:
        void initialize(CE::iWindow&) override {}
        void poll() override {}
        void deinitialize() override {}
        CE::Input::InputBindings& bindings() override { return bindings_; }
        CE::Input::DeviceId keyboard_id() const override { return 1; }
        CE::Input::DeviceId mouse_id() const override { return 2; }
        CE::Input::DeviceId gamepad_id() const override { return 3; }
        bool supports(CE::Input::InputMode) const override { return true; }
        bool supports_focus() const override { return true; }
        std::shared_ptr<const CE::Input::PollSnapshot> text() {
            begin_input_poll();
            capture_buffer().record(1, CE::Input::DeviceKind::Keyboard, CE::Input::TextEvent{U'A'});
            return publish_input();
        }
    };
}

int main() {
    Input input;
    CE::UI::RmlUi::RecordedScene retained;
    {
        CE::UI::RmlUi::Session session(input, 7);
        auto& document = RmlUiTests::document(session);
        auto& field = RmlUiTests::field(document);
        session.request_keyboard_focus();
        if (!field.Focus())
            return 1;
        session.handle_input(input.text()->records, false);
        session.update_time(0.01);
        retained = session.record();
        if (field.GetValue() != "A" || retained.draws().empty())
            return 1;
    }
    CE::RenderAPIs::RenderFrame frame;
    CE::RenderAPIs::RenderFrameWriter writer(frame);
    CE::UI::RmlUi::Scene{}.write(writer);
    return Rml::GetVersion() == "6.3" && Rml::GetSystemInterface() == nullptr && !retained.draws().empty() &&
                   CE::UI::RmlUi::keyboard_key(CE::Input::KeyboardKey::Enter) == Rml::Input::KI_RETURN && frame.passes().empty()
               ? 0
               : 1;
}
