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

namespace CE::Assets {
    using FontResources = std::tuple<std::shared_ptr<Geometry2D>, std::shared_ptr<Image>>;
    // Index addresses a six-vertex glyph quad; offsets use local Y-up font metrics.
    struct GlyphPlacement2D {
        std::size_t index;
        float x;
        float y;
    };
    struct FontLayoutOptions {
        bool alternate_bank = false; // Legacy FFont's typed fancy-bank selection.
    };
    /** Retains glyph geometry/atlas; direct base construction does not validate them.
     * Immutable implementations support CPU layout without a graphics context.
     */
    struct Font : protected Asset2D {
        explicit Font(const FontResources& data)
        : Asset2D(std::get<0>(data), std::get<1>(data)) {}
        virtual ~Font() = default;
        // Returns owned placements without retaining text. Encoding/metrics and
        // option support belong to the implementation; this interface promises no shaping.
        [[nodiscard]] virtual std::vector<GlyphPlacement2D>
        layout(std::string_view text, FontLayoutOptions options = FontLayoutOptions{}) const = 0;
        // References borrow this Font; copy handles to retain resources beyond its lifetime.
        [[nodiscard]] const std::shared_ptr<Geometry2D>& glyph_geometry_handle() const { return geometry; }
        [[nodiscard]] const std::shared_ptr<Image>& glyph_atlas_handle() const { return texture; }
    };
}
#endif
