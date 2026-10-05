#pragma once

#include <core/controls/input-record.h>
#include <TGUI/Event.hpp>

#include <optional>

namespace CE::UI::TGUI {
    /** Translate one application-selected record without changing its source.
     * The caller owns capture/focus/routing and passes keyboard/Text records only
     * to their selected consumer. Coordinates remain logical window units, floored
     * to TGUI's integer coordinates; configure the GUI's input view accordingly.
     * Known mouse buttons and vertical scrolling require the record's own position.
     * Missing/unrepresentable positions and invalid text/scroll values reject.
     * Unsupported controls/devices, keyboard releases and horizontal-only scroll
     * return no event. TGUI has no key-release event; modifier state still belongs
     * to the complete original record stream. Press and Repeat both become KeyPressed.
     */
    [[nodiscard]] std::optional<tgui::Event> translate_event(const Input::InputRecord& record);
}
