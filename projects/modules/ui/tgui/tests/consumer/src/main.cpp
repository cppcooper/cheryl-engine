#include <cheryl/ui/tgui/input.h>
#include <cheryl/ui/tgui/layout.h>
#include <cheryl/ui/tgui/rendering.h>
#include <cheryl/ui/tgui/scene.h>
#include <cheryl/ui/tgui/session.h>

#include <TGUI/Widgets/Label.hpp>

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
    };
}

int main() {
    const CE::Input::InputRecord record{1, CE::Input::InputClock::now(), 7, CE::Input::DeviceKind::Keyboard, CE::Input::TextEvent{U'A'}};
    const auto event = CE::UI::TGUI::translate_event(record);
    CE::UI::TGUI::RenderTarget target;
    target.setView({0, 0, 320, 240}, {0, 0, 320, 240}, {320, 240});
    target.begin_recording();
    target.drawFilledRect({}, {20, 10}, tgui::Color::Red);
    const auto scene = target.finish_recording();
    CE::RenderAPIs::RenderFrame frame;
    CE::RenderAPIs::RenderFrameWriter writer(frame);
    CE::UI::TGUI::Scene{}.write(writer);
    Input input;
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
