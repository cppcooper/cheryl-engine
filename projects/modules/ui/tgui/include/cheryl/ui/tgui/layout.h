#pragma once

#include <TGUI/Vector2.hpp>

#include <limits>
#include <optional>

namespace CE::UI::TGUI {
    struct Offset {
        tgui::Vector2f fixed{0, 0};
        tgui::Vector2f relative{0, 0}; // Fractions of parent content width/height.
    };

    struct Scalable {
        std::optional<float> width;
        std::optional<float> height;
        float min_width = 0;
        float max_width = std::numeric_limits<float>::infinity();
        float min_height = 0;
        float max_height = std::numeric_limits<float>::infinity();
    };

    struct WidgetLayout {
        tgui::Vector2f anchor{0, 0};
        std::optional<tgui::Vector2f> origin; // Defaults to anchor.
        Offset offset;
        std::optional<Scalable> scalable;
    };
}
