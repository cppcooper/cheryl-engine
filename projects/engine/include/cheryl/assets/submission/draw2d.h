#pragma once

#include <assets/types/2d/font.h>
#include <assets/types/2d/graphic.h>
#include <assets/types/2d/sprite.h>
#include <assets/types/2d/tileset.h>
#include <assets/types/2d/unicode-text.h>
#include <core/rendering/draw-packet.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace CE::Rendering::Submission {
    struct ImageParameter2D {
        std::string key; // Public pipeline key, never a GLSL uniform name.
        std::uint32_t unit = 0;
    };
    // Owned per-pass values. image optionally inserts the asset's image in the draw
    // layer; its key must be nonempty and absent from DrawStyle2D::parameters.
    struct Context2D {
        CE::Rendering::Parameters::PassSemantics pass;
        CE::Rendering::Parameters::Set parameters;
        CE::Rendering::Pipelines::PassConstraints2D constraints;
        std::optional<ImageParameter2D> image;
    };

    /** Resolve one sprite cell into a validated draw packet without backend calls.
     * Keep inputs stable during CPU resolution. No binding, playback advancement
     * or frame publication occurs; invalid cells/materials/ranges or image parameters
     * reject resolution, and allocation failures propagate.
     * @param sprite CE::Assets::Sprites::Asset whose geometry/image handles are retained by the packet.
     * @param cell Zero-based grid cell, checked against the sprite definition.
     * @param style Copied transform/material/draw values; an absent material uses the sprite's.
     * @param context Pass values and optional image insertion; an image key must be
     * nonempty and absent from style.parameters.
     * @return Owned packet ready to add to a render pass; native use requires its live resource domain.
     */
    [[nodiscard]] DrawPacket2D resolve_sprite(
        const CE::Assets::Sprites::Asset& sprite,
        CE::Assets::Grids::CellIndex cell,
        const DrawStyle2D& style,
        const Context2D& context
    );
    // CPU-only resolution from stable inputs: copies values and retains resources,
    // without binding or advancing playback. Failures publish no frame packets.
    // Cell overloads check grid bounds; all returned packets validate material/ranges.
    [[nodiscard]] DrawPacket2D resolve_tile(
        const CE::Assets::Tilesets::Asset& tileset,
        CE::Assets::Grids::CellIndex cell,
        const DrawStyle2D& style,
        const Context2D& context
    );
    [[nodiscard]] DrawPacket2D resolve_tile(const CE::Assets::Tilesets::Tile& tile, const DrawStyle2D& style, const Context2D& context);
    [[nodiscard]] DrawPacket2D
    resolve_tile(const CE::Assets::Tilesets::Animation& animation, const DrawStyle2D& style, const Context2D& context);
    [[nodiscard]] DrawPacket2D
    resolve_graphic(const CE::Assets::Graphics::Asset& graphic, const DrawStyle2D& style, const Context2D& context);
    // Layout does not retain text. Glyph offsets are scaled then transformed by the
    // caller's model; packets retain the font resources. Empty layout returns no draws.
    [[nodiscard]] std::vector<DrawPacket2D> resolve_text(
        const CE::Text::Legacy::Font& font,
        std::string_view text,
        const DrawStyle2D& style,
        const Context2D& context,
        CE::Text::Legacy::LayoutOptions options = CE::Text::Legacy::LayoutOptions{}
    );
    /** Resolve an uploaded Unicode text generation into ordered glyph draw packets.
     * CPU-only resolution uses stable inputs and performs no font discovery, layout,
     * rasterization or upload. Invalid image keys/materials/ranges reject the complete
     * result without publishing frame packets; allocation failures propagate.
     * @param text Immutable generation whose page resources are retained by the packets.
     * @param style Copied draw values; glyph offsets are scaled and then transformed
     * by its model matrix.
     * @param context Pass values; image must supply a nonempty page-image key absent
     * from style.parameters.
     * @return Owned packets in authored glyph order, empty when there are no glyph draws;
     * native use requires the original live resource domain.
     */
    [[nodiscard]] std::vector<DrawPacket2D>
    resolve_text(const CE::Text::Rendering::Generation& text, const DrawStyle2D& style, const Context2D& context);
    // Explicitly requires positioned_runs preparation and a shader applying the
    // common scale/model transform to positioned vertices, without per-glyph or
    // draw-identity semantics. Emits contiguous page runs in authored glyph order;
    // the caller's model applies to the whole text. Other shaders use resolve_text.
    // Regrouped floating-point transforms can change boundary pixels. Use
    // resolve_text when the original per-glyph raster evaluation is required.
    [[nodiscard]] std::vector<DrawPacket2D>
    resolve_text_runs(const CE::Text::Rendering::Generation& text, const DrawStyle2D& style, const Context2D& context);
}
