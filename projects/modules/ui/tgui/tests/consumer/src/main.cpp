#include <cheryl/ui/tgui/input.h>
#include <cheryl/core/logging/process-lifetime.h>
#include <cheryl/ui/tgui/layout.h>
#include <cheryl/ui/tgui/rendering.h>
#include <cheryl/ui/tgui/scene.h>
#include <cheryl/ui/tgui/session.h>
#include <cheryl/text/font-service.h>

#include <TGUI/Widgets/Label.hpp>

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
    };
}

int main() {
    CE::Logging::ProcessLifetime logging_lifetime;
    const CE::Input::Events::Record record{
        1, CE::Input::ObservationClock::now(), 7, CE::Input::DeviceKind::Keyboard, CE::Input::Events::Text{U'A'}
    };
    const auto event = CE::UI::TGUI::translate_event(record);
    CE::UI::TGUI::RenderTarget target;
    target.setView({0, 0, 320, 240}, {0, 0, 320, 240}, {320, 240});
    target.begin_recording();
    target.drawFilledRect({}, {20, 10}, tgui::Color::Red);
    const auto scene = target.finish_recording();
    CE::Rendering::Frame frame;
    CE::Rendering::FrameWriter writer(frame);
    CE::UI::TGUI::Scene{}.write(writer);
    Input input;
    {
        CE::Text::FontService fonts;
        const auto selected = fonts.default_font();
        CE::UI::TGUI::Session catalogue_session(input, fonts, 9);
        catalogue_session.set_view({320, 240}, {320, 240});
        const auto label = tgui::Label::create("Engine font consumer");
        catalogue_session.gui().add(label);
        catalogue_session.set_font(label, selected);
        if (!catalogue_session.font(selected).getBackendFont() || catalogue_session.record().draws().empty())
            return 1;
    }
    CE::UI::TGUI::Session session(input, 1);
    session.set_view({320, 240}, {640, 480});
    const auto label = tgui::Label::create("Independent consumer");
    session.gui().add(label);
    session.set_layout(label, {.anchor = {0.5f, 0.5f}, .offset = {.relative = {0.125f, 0}}});
    session.update_time(0.01);
    const auto widgets = session.record();
    return event && event->type == tgui::Event::Type::TextEntered && event->text.unicode == U'A' && scene.draws().size() == 1 &&
                   scene.draws()[0].vertices.size() == 6 && frame.passes().empty() && !widgets.draws().empty() &&
                   label->getPosition() == tgui::Vector2f{200, 120}
               ? 0
               : 1;
}
