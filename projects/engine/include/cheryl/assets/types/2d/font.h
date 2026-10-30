#pragma once

#include <cheryl/export.h>
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
    /** Shared glyph geometry/atlas pair retained by legacy font implementations. */
    using FontResources = std::tuple<std::shared_ptr<CE::Rendering::Geometry::Buffer2D>, std::shared_ptr<CE::Rendering::Resources::Image>>;
    /** One legacy six-vertex glyph quad with an implementation-defined local pen offset. */
    struct GlyphPlacement {
        std::size_t index; ///< Zero-based quad index, not a source-byte or Unicode index.
        float x; ///< Local X offset in the implementation's font units.
        float y; ///< Local Y-up offset in the implementation's font units.
    };
    /** Legacy implementation options; modern shaping uses CE::Text::LayoutOptions. */
    struct LayoutOptions {
        bool alternate_bank = false; ///< Select BitmapFont's alternate128-glyph bank; unsupported implementations reject it.
    };
    /** Retains glyph geometry/atlas; direct base construction does not validate them.
     * Immutable implementations support CPU layout without a graphics context.
     */
    struct CE_TYPE_API Font : protected CE::Assets::Content2D {
        explicit Font(const FontResources& data, std::shared_ptr<const CE::Rendering::Resources::Material> material = {})
        : CE::Assets::Content2D(std::get<0>(data), std::get<1>(data), std::move(material)) {}
        CE_API virtual ~Font();
        // Returns owned placements without retaining text. Encoding/metrics and
        // option support belong to the implementation; this interface promises no shaping.
        [[nodiscard]] virtual std::vector<GlyphPlacement> layout(std::string_view text, LayoutOptions options = LayoutOptions{}) const = 0;
        // References borrow this Font; copy handles to retain resources beyond its lifetime.
        [[nodiscard]] const std::shared_ptr<CE::Rendering::Geometry::Buffer2D>& glyph_geometry_handle() const { return geometry; }
        [[nodiscard]] const std::shared_ptr<CE::Rendering::Resources::Image>& glyph_atlas_handle() const { return texture; }
        [[nodiscard]] const std::shared_ptr<const CE::Rendering::Resources::Material>& material_handle() const { return material; }
    };
}

#endif

/** @fn CE::Text::Legacy::Font::Font(const FontResources& data, std::shared_ptr<const CE::Rendering::Resources::Material> material)
 * Retain supplied geometry, atlas and optional material without validating resources. Implementations determine metrics/encoding; callers must preserve the resource backend domain for native use.
 */

/** @fn CE::Text::Legacy::Font::~Font()
 * Release logical resource ownership; final native retirement follows the original backend rules.
 */

/** @fn std::vector<CE::Text::Legacy::GlyphPlacement> CE::Text::Legacy::Font::layout(std::string_view text, LayoutOptions options) const
 * Return owned placements without retaining the input bytes or requiring graphics calls. Encoding, metrics and supported options belong to the concrete implementation; the interface promises no shaping.
 * @param text Borrowed only for this call.
 * @param options Legacy bank selection, where supported.
 */

/** @fn const std::shared_ptr<CE::Rendering::Geometry::Buffer2D>& CE::Text::Legacy::Font::glyph_geometry_handle() const
 * Borrow the retained geometry handle. Copy it to retain ownership beyond this font; native use still requires the original live backend.
 */

/** @fn const std::shared_ptr<CE::Rendering::Resources::Image>& CE::Text::Legacy::Font::glyph_atlas_handle() const
 * Borrow the retained atlas handle; copy the handle for independent retention.
 */

/** @fn const std::shared_ptr<const CE::Rendering::Resources::Material>& CE::Text::Legacy::Font::material_handle() const
 * Borrow the optional retained material handle. Empty means no material was supplied to this base construction.
 */
