#include <cheryl/ui/tgui/input.h>
#include <cheryl/ui/tgui/rendering.h>
#include <cheryl/ui/tgui/scene.h>

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
    return event && event->type == tgui::Event::Type::TextEntered && event->text.unicode == U'A' && scene.draws().size() == 1 &&
                   scene.draws()[0].vertices.size() == 6 && frame.passes().empty()
               ? 0
               : 1;
}
