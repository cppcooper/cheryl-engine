#include <cheryl/ui/rmlui/input.h>
#include <cheryl/core/logging/process-lifetime.h>
#include <cheryl/ui/rmlui/scene.h>
#include <cheryl/ui/rmlui/session.h>
#include <cheryl/text/font-service.h>
#include "../../support/document.h"

#include <RmlUi/Core/Core.h>

namespace {
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
        std::shared_ptr<const CE::Input::PollSnapshot> text() {
            begin_input_poll();
            capture_buffer().record(1, CE::Input::DeviceKind::Keyboard, CE::Input::Events::Text{U'A'});
            return publish_input();
        }
    };
}

int main() {
    CE::Logging::ProcessLifetime logging_lifetime;
    Input input;
    CE::UI::RmlUi::RecordedScene retained;
    {
        CE::Text::FontService fonts;
        const auto selected = fonts.default_font();
        CE::UI::RmlUi::Session catalogue_session(input, fonts, 9);
        catalogue_session.set_view({320, 240}, {320, 240});
        auto* document = catalogue_session.context().LoadDocumentFromMemory("<rml><body>Engine font consumer</body></rml>");
        if (!document || catalogue_session.font_family(selected) != selected.reference())
            return 1;
        catalogue_session.set_font(*document, selected);
        document->Show();
        if (catalogue_session.record().draws().empty())
            return 1;
    }
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
    CE::Rendering::Frame frame;
    CE::Rendering::FrameWriter writer(frame);
    CE::UI::RmlUi::Scene{}.write(writer);
    return Rml::GetVersion() == "6.3" && Rml::GetSystemInterface() == nullptr && !retained.draws().empty() &&
                   CE::UI::RmlUi::keyboard_key(CE::Input::KeyboardKey::Enter) == Rml::Input::KI_RETURN && frame.passes().empty()
               ? 0
               : 1;
}
