#pragma once

#include <cheryl/export.h>

#include <core/controls/input-record.h>

#include <RmlUi/Core/Input.h>

namespace CE::UI::RmlUi {
    // Physical identities are independent of committed text and native codes.
    [[nodiscard]] CE_UI_RMLUI_API Rml::Input::KeyIdentifier keyboard_key(CE::Input::KeyboardKey key);
    [[nodiscard]] CE_UI_RMLUI_API int modifiers(CE::Input::Modifiers value);
}
