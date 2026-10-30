#pragma once

#include <cheryl/export.h>

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
    /** Optional asset-image insertion into the highest-priority custom draw layer. */
    struct ImageParameter2D {
        std::string key; ///< Nonempty public sampler key, never a backend uniform name.
        std::uint32_t unit = 0; ///< Zero-based texture unit; complete draws require distinct sampler units.
    };
    // Owned per-pass values. image optionally inserts the asset's image in the draw
    // layer; its key must be nonempty and absent from DrawStyle2D::parameters.
    // The standard image binding is automatic; nullopt selects manual bindings.
    /** Owned pass inputs for CPU asset resolution.
     * The default inserts the asset image as {image,0}; nullopt opts out for ordinary
     * assets/legacy fonts. Unicode generation submission requires page-image insertion.
     * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
     */
    struct Context2D {
        CE::Rendering::Parameters::PassSemantics pass; ///< Copied engine projection/view semantics.
        CE::Rendering::Parameters::Set parameters; ///< Owned custom pass values, below material/draw values.
        CE::Rendering::Pipelines::PassConstraints2D constraints; ///< Required pipeline fixed-state choices.
        std::optional<ImageParameter2D> image = ImageParameter2D{"image", 0}; ///< Automatic image key/unit, or nullopt for manual ordinary-asset bindings.
    };

    [[nodiscard]] CE_API DrawPacket2D resolve_sprite(
        const CE::Assets::Sprites::Asset& sprite,
        CE::Assets::Grids::CellIndex cell,
        const DrawStyle2D& style,
        const Context2D& context
    );
    // CPU-only resolution from stable inputs: copies values and retains resources,
    // without binding or advancing playback. Failures publish no frame packets.
    // Cell overloads check grid bounds; all returned packets validate material/ranges.
    [[nodiscard]] CE_API DrawPacket2D resolve_tile(
        const CE::Assets::Tilesets::Asset& tileset,
        CE::Assets::Grids::CellIndex cell,
        const DrawStyle2D& style,
        const Context2D& context
    );
    [[nodiscard]] CE_API DrawPacket2D
    resolve_tile(const CE::Assets::Tilesets::Tile& tile, const DrawStyle2D& style, const Context2D& context);
    [[nodiscard]] CE_API DrawPacket2D
    resolve_tile(const CE::Assets::Tilesets::Animation& animation, const DrawStyle2D& style, const Context2D& context);
    [[nodiscard]] CE_API DrawPacket2D
    resolve_graphic(const CE::Assets::Graphics::Asset& graphic, const DrawStyle2D& style, const Context2D& context);
    // Layout does not retain text. Glyph offsets are scaled then transformed by the
    // caller's model; packets retain the font resources. Empty layout returns no draws.
    [[nodiscard]] CE_API std::vector<DrawPacket2D> resolve_text(
        const CE::Text::Legacy::Font& font,
        std::string_view text,
        const DrawStyle2D& style,
        const Context2D& context,
        CE::Text::Legacy::LayoutOptions options = CE::Text::Legacy::LayoutOptions{}
    );
    [[nodiscard]] CE_API std::vector<DrawPacket2D>
    resolve_text(const CE::Text::Rendering::Generation& text, const DrawStyle2D& style, const Context2D& context);
    // Explicitly requires positioned_runs preparation and a shader applying the
    // common scale/model transform to positioned vertices, without per-glyph or
    // draw-identity semantics. Emits contiguous page runs in authored glyph order;
    // the caller's model applies to the whole text. Other shaders use resolve_text.
    // Regrouped floating-point transforms can change boundary pixels. Use
    // resolve_text when the original per-glyph raster evaluation is required.
    [[nodiscard]] CE_API std::vector<DrawPacket2D>
    resolve_text_runs(const CE::Text::Rendering::Generation& text, const DrawStyle2D& style, const Context2D& context);
}

/**
 * @fn CE::Rendering::DrawPacket2D CE::Rendering::Submission::resolve_sprite(const CE::Assets::Sprites::Asset& sprite, CE::Assets::Grids::CellIndex cell, const DrawStyle2D& style, const Context2D& context)
 * @brief Resolve one sprite grid cell into a validated retained CPU packet.
 * @param sprite Stable sprite definition and retained grid resources.
 * @param cell Zero-based grid cell, not an animation clip index.
 * @param style Copied draw values; empty material selects the sprite's retained default.
 * @param context Stable pass inputs and optional image insertion.
 * @return Owned four-vertex strip packet. No playback advancement, native bind or frame publication occurs.
 * Invalid cell/range/material/parameters or a duplicate/empty image key rejects resolution; allocation errors propagate. The selected image key must be absent from style.parameters.
 */

/**
 * @fn CE::Rendering::DrawPacket2D CE::Rendering::Submission::resolve_tile(const CE::Assets::Tilesets::Asset& tileset, CE::Assets::Grids::CellIndex cell, const DrawStyle2D& style, const Context2D& context)
 * @brief Resolve a checked tileset grid cell as one four-vertex strip packet.
 * @param tileset Stable grid definition and retained resources.
 * @param cell Zero-based grid cell inside the tileset.
 * @param style Copied transform/custom values; explicit material wins over the asset default.
 * @param context Pass inputs; automatic image insertion rejects empty/duplicate keys.
 * @return Owned validated packet; no native work, animation advancement or publication occurs.
 * Invalid grid/range/material/parameters or allocation failure returns no packet.
 */

/**
 * @fn CE::Rendering::DrawPacket2D CE::Rendering::Submission::resolve_tile(const CE::Assets::Tilesets::Tile& tile, const DrawStyle2D& style, const Context2D& context)
 * @brief Resolve an already selected tile value using its retained resources and cell offset.
 * @param tile Stable tile; the material/geometry range still undergoes packet validation.
 * @param style Explicit material overrides the tile default.
 * @param context Pass values and optional automatic image insertion.
 * @return Owned four-vertex strip packet; no grid metadata lookup, time advancement or native work occurs.
 * Invalid range/material/image parameters or allocation errors propagate without publication.
 */

/**
 * @fn CE::Rendering::DrawPacket2D CE::Rendering::Submission::resolve_tile(const CE::Assets::Tilesets::Animation& animation, const DrawStyle2D& style, const Context2D& context)
 * @brief Resolve the current clip entry's cell without advancing its cursor.
 * @param animation Stable animation; index selects a clip entry, whose cell selects the grid strip.
 * @param style Copied draw values with optional material override.
 * @param context Pass values and optional image insertion.
 * @return Owned validated packet retaining animation resources.
 * An invalid clip index throws out_of_range; normal range/material/parameter/allocation failures propagate. No native call or frame publication occurs.
 */

/**
 * @fn CE::Rendering::DrawPacket2D CE::Rendering::Submission::resolve_graphic(const CE::Assets::Graphics::Asset& graphic, const DrawStyle2D& style, const Context2D& context)
 * @brief Resolve a whole-image graphic as six independent-triangle vertices.
 * @param graphic Stable retained image/geometry and default material.
 * @param style Explicit material wins; copied values are resolved with the pass.
 * @param context Pass constraints/custom values and optional automatic image insertion.
 * @return Owned validated packet; no binding, upload or publication occurs.
 * Invalid handles/ranges/parameters or allocation failure returns no packet.
 */

/**
 * @fn std::vector<CE::Rendering::DrawPacket2D> CE::Rendering::Submission::resolve_text(const CE::Text::Legacy::Font& font, std::string_view text, const DrawStyle2D& style, const Context2D& context, CE::Text::Legacy::LayoutOptions options)
 * @brief Lay out legacy text on the CPU and resolve ordered six-vertex glyph packets.
 * @param font Stable immutable metrics/resources; its encoding/option contract applies.
 * @param text Bytes borrowed for this call, never retained.
 * @param style Empty material selects the font default; offsets are scaled then transformed by model_matrix.
 * @param context Pass inputs; nullopt permits manual image binding for this legacy path.
 * @param options Implementation-specific legacy layout options.
 * @return Owned packets retaining glyph geometry/atlas/material, empty for no visible glyphs.
 * No native calls occur; layout/validation/allocation errors return no result vector and publish no partial frame packets.
 */

/**
 * @fn std::vector<CE::Rendering::DrawPacket2D> CE::Rendering::Submission::resolve_text(const CE::Text::Rendering::Generation& text, const DrawStyle2D& style, const Context2D& context)
 * @brief Resolve one immutable Unicode generation into authored-order glyph packets.
 * @param text Stable uploaded generation; no discovery, shaping, rasterization or upload is repeated.
 * @param style Explicit material wins over the generation default; glyph offsets are scaled then transformed.
 * @param context Pass values with required nonempty page-image key absent from style.parameters.
 * @return Owned six-vertex glyph packets retaining selected page resources; empty for no glyph draws.
 * Missing/duplicate image insertion or invalid ranges/material/parameters rejects the complete CPU result. Allocation failures propagate; no frame publication occurs.
 * @see [Text preparation and upload](cheryl-guide:assets/text-layout#preparation-upload-and-retained-submission)
 */

/**
 * @fn std::vector<CE::Rendering::DrawPacket2D> CE::Rendering::Submission::resolve_text_runs(const CE::Text::Rendering::Generation& text, const DrawStyle2D& style, const Context2D& context)
 * @brief Resolve prepositioned geometry as contiguous atlas-page runs in authored order.
 * @param text Generation prepared with positioned_runs enabled; otherwise invalid_args is thrown.
 * @param style Whole-text model/scale and optional material override.
 * @param context Pass inputs with required nonempty page-image insertion, absent from style.parameters.
 * @return Owned validated page-run packets, empty for no visible runs.
 * The shader must apply common scale/model to positioned vertices without per-glyph/draw-identity semantics. Regrouped floating-point transforms may change boundary pixels; use resolve_text() for original per-glyph evaluation. Validation/allocation failures publish nothing.
 * @see [Positioned text runs](cheryl-guide:assets/text-layout#positioned-text-runs)
 */
