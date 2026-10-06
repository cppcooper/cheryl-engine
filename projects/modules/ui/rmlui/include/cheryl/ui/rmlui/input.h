#pragma once

#include <core/controls/input-record.h>

#include <RmlUi/Core/Input.h>

namespace CE::UI::RmlUi {
    // Physical identities are independent of committed text and native codes.
    [[nodiscard]] Rml::Input::KeyIdentifier keyboard_key(Input::KeyboardKey key);
    [[nodiscard]] int modifiers(Input::Modifiers value);
}
