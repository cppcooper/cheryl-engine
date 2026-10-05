#include <cheryl/ui/tgui/input.h>

int main() {
    const CE::Input::InputRecord record{1, CE::Input::InputClock::now(), 7, CE::Input::DeviceKind::Keyboard, CE::Input::TextEvent{U'A'}};
    const auto event = CE::UI::TGUI::translate_event(record);
    return event && event->type == tgui::Event::Type::TextEntered && event->text.unicode == U'A' ? 0 : 1;
}
