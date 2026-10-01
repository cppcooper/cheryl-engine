#pragma once

#include <assets/types/2d/font.h>
#include <assets/types/2d/graphic.h>
#include <assets/types/2d/sprite.h>
#include <assets/types/2d/tileset.h>
#include <core/rendering/draw-packet.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace CE::Assets {
    struct ImageParameter2D {
        std::string key; // Public pipeline key, never a GLSL uniform name.
        std::uint32_t unit = 0;
    };
    struct SubmissionContext2D {
        ShaderPass pass;
        ParameterSet parameters;
        PassConstraints2D constraints;
        std::optional<ImageParameter2D> image;
    };

    [[nodiscard]] RenderAPIs::DrawPacket2D resolve_sprite(
        const Sprite& sprite, CellIndex cell, const RenderAPIs::DrawStyle2D& style, const SubmissionContext2D& context
    );
    [[nodiscard]] RenderAPIs::DrawPacket2D resolve_tile(
        const Tileset& tileset, CellIndex cell, const RenderAPIs::DrawStyle2D& style, const SubmissionContext2D& context
    );
    [[nodiscard]] RenderAPIs::DrawPacket2D resolve_tile(
        const Tile& tile, const RenderAPIs::DrawStyle2D& style, const SubmissionContext2D& context
    );
    [[nodiscard]] RenderAPIs::DrawPacket2D resolve_tile(
        const TileAnimation& animation, const RenderAPIs::DrawStyle2D& style, const SubmissionContext2D& context
    );
    [[nodiscard]] RenderAPIs::DrawPacket2D resolve_graphic(
        const Graphic& graphic, const RenderAPIs::DrawStyle2D& style, const SubmissionContext2D& context
    );
    [[nodiscard]] std::vector<RenderAPIs::DrawPacket2D> resolve_text(
        const Font& font,
        std::string_view text,
        const RenderAPIs::DrawStyle2D& style,
        const SubmissionContext2D& context,
        FontLayoutOptions options = FontLayoutOptions{}
    );
}
