#pragma once
#ifndef CEFONT_H
#define CEFONT_H
#include "base/asset2d.h"

#include <string>
#include <tuple>
#include <string_view>
#include <vector>
#include <cstddef>
#include <memory>

namespace CE::Text::Legacy {
    using FontResources = std::tuple<std::shared_ptr<CE::Rendering::Geometry::Buffer2D>, std::shared_ptr<CE::Rendering::Resources::Image>>;
    // Index addresses a six-vertex glyph quad; offsets use local Y-up font metrics.
    struct GlyphPlacement {
        std::size_t index;
        float x;
        float y;
    };
    struct LayoutOptions {
        bool alternate_bank = false; // Legacy CE::Text::Legacy::BitmapFont's typed fancy-bank selection.
    };
    /** Retains glyph geometry/atlas; direct base construction does not validate them.
     * Immutable implementations support CPU layout without a graphics context.
     */
    struct Font : protected CE::Assets::Content2D {
        explicit Font(const FontResources& data)
        : CE::Assets::Content2D(std::get<0>(data), std::get<1>(data)) {}
        virtual ~Font() = default;
        // Returns owned placements without retaining text. Encoding/metrics and
        // option support belong to the implementation; this interface promises no shaping.
        [[nodiscard]] virtual std::vector<GlyphPlacement> layout(std::string_view text, LayoutOptions options = LayoutOptions{}) const = 0;
        // References borrow this Font; copy handles to retain resources beyond its lifetime.
        [[nodiscard]] const std::shared_ptr<CE::Rendering::Geometry::Buffer2D>& glyph_geometry_handle() const { return geometry; }
        [[nodiscard]] const std::shared_ptr<CE::Rendering::Resources::Image>& glyph_atlas_handle() const { return texture; }
    };
}

#endif
