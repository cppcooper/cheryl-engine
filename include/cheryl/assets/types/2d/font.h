#pragma once
#ifndef CEFONT_H
#define CEFONT_H
#include "base/asset2d.h"
#include <core/rendering/draw-info.h>

#include <string>
#include <tuple>
#include <string_view>
#include <vector>

namespace CE::Assets {
    struct FontDrawInfo : DrawInfo {
        float angle = 0.f;
    };
    using FontResources = std::tuple<std::shared_ptr<Geometry2D>, std::shared_ptr<Image>>;
    struct GlyphPlacement2D {
        std::size_t index;
        float x;
        float y;
    };
    struct FontLayoutOptions {
        bool alternate_bank = false; // Legacy FFont's typed fancy-bank selection.
    };
    struct Font : protected Asset2D {
        explicit Font(const FontResources& data) : Asset2D(std::get<0>(data), std::get<1>(data)) {}
        virtual ~Font() = default;
        [[nodiscard]] virtual std::vector<GlyphPlacement2D> layout(std::string_view text, FontLayoutOptions options = FontLayoutOptions{}) const = 0;
        [[nodiscard]] const std::shared_ptr<Geometry2D>& glyph_geometry_handle() const { return geometry; }
        [[nodiscard]] const std::shared_ptr<Image>& glyph_atlas_handle() const { return texture; }
        virtual void print(std::string text, FontDrawInfo* format) = 0;
    };
}
#endif
