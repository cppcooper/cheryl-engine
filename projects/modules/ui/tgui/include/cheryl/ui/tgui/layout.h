#pragma once

#include <TGUI/Vector2.hpp>

#include <limits>
#include <optional>

namespace CE::UI::TGUI {
    /** Placement offset added to a parent-content anchor in logical UI coordinates. */
    struct Offset {
        tgui::Vector2f fixed{0, 0}; ///< Fixed X/Y logical offsets.
        tgui::Vector2f relative{0, 0}; ///< Fractions of the current parent's content width/height.
    };

    /** Optional parent-relative size axes, each clamped between logical bounds. Fractions/bounds must produce finite valid dimensions. */
    struct Scalable {
        std::optional<float> width; ///< Parent-content width fraction in [0,1]; unset leaves native width unmanaged.
        std::optional<float> height; ///< Parent-content height fraction in [0,1]; unset leaves native height unmanaged.
        float min_width = 0; ///< Nonnegative logical minimum width.
        float max_width = std::numeric_limits<float>::infinity(); ///< Logical maximum width, at least min_width; infinity leaves it unbounded.
        float min_height = 0; ///< Nonnegative logical minimum height.
        float max_height = std::numeric_limits<float>::infinity(); ///< Logical maximum height, at least min_height; infinity leaves it unbounded.
    };

    /** Typed placement applied through Session::set_layout to a widget already in that GUI.
     * @see [Layout rules](cheryl-guide:modules/tgui#typed-layout) */
    struct WidgetLayout {
        tgui::Vector2f anchor{0, 0}; ///< Parent-content anchor fractions, each finite in [0,1].
        std::optional<tgui::Vector2f> origin; ///< Widget origin fractions, each finite in [0,1]; unset uses anchor.
        Offset offset; ///< Fixed and parent-relative offsets added to anchor.
        std::optional<Scalable> scalable; ///< Optional managed size rules; when present at least one axis is required. Releasing a managed axis freezes its current size.
    };
}
