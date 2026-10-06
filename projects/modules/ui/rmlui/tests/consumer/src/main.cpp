#include <cheryl/ui/rmlui/input.h>

#include <RmlUi/Core/Core.h>

int main() {
    return Rml::GetVersion() == "6.3" && CE::UI::RmlUi::keyboard_key(CE::Input::KeyboardKey::Enter) == Rml::Input::KI_RETURN ? 0 : 1;
}
