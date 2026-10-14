#pragma once

#include <assets/types/2d/stbfont.h>
#include <assets/types/primitives/vertex.h>

#include <array>
#include <memory>
#include <span>

namespace CE::Text::Legacy::Detail {
    // Private production upload boundary: baking supplies owned glyph vertices
    // and a borrowed alpha atlas, both copied by the provider before return.
    [[nodiscard]] AsciiFontData upload_baked_font(
        CE::Rendering::Resources::Provider& provider,
        std::shared_ptr<CE::Rendering::Geometry::Vertex2D> vertices,
        std::span<const unsigned char> alpha,
        CE::Rendering::Geometry::PixelSize atlas_size,
        const std::array<float, ascii_character_count>& advances,
        float line_height
    );
}
