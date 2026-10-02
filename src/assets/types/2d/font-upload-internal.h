#pragma once

#include <assets/types/2d/stbfont.h>
#include <assets/types/primitives/vertex.h>

#include <array>
#include <memory>
#include <span>

namespace CE::Assets::FontDetail {
    // Private production upload boundary: baking supplies owned glyph vertices
    // and a borrowed alpha atlas, both copied by the provider before return.
    [[nodiscard]] STBFontData upload_baked_font(
        ResourceProvider& provider,
        std::shared_ptr<Vertex2D> vertices,
        std::span<const unsigned char> alpha,
        PixelSize atlas_size,
        const std::array<float, font_character_count>& advances,
        float line_height
    );
}
