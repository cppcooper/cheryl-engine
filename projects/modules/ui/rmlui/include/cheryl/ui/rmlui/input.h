#pragma once

#include <cheryl/export.h>

#include <core/controls/input-record.h>

#include <RmlUi/Core/Input.h>

namespace CE::UI::RmlUi {
    // Physical identities are independent of committed text and native codes.
    [[nodiscard]] CE_UI_RMLUI_API Rml::Input::KeyIdentifier keyboard_key(CE::Input::KeyboardKey key);
    [[nodiscard]] CE_UI_RMLUI_API int modifiers(CE::Input::Modifiers value);
}

/** @fn Rml::Input::KeyIdentifier CE::UI::RmlUi::keyboard_key(CE::Input::KeyboardKey key)
 * Map a portable physical key to RmlUi6.3 identity, returning KI_UNKNOWN for unsupported keys. Does not create committed text or inspect native codes.
 * @see [Input identities](cheryl-guide:modules/rmlui#input-identities)
 */

/** @fn int CE::UI::RmlUi::modifiers(CE::Input::Modifiers value)
 * Map supported Engine modifier flags to a RmlUi bitmask without reading mutable platform state; unrelated bits contribute nothing.
 */
