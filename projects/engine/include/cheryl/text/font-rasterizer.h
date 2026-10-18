#pragma once

#include <cheryl/export.h>
#include <cheryl/text/font.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace CE::Text {
    /** Pixels requests native pixel sizing; Characters uses character-size metrics with fixed-strike selection/downscaling when necessary. */
    enum class FontSizing { Pixels /**< Request native pixel sizing. */, Characters /**< Request character sizing, permitting fixed-strike selection/downscaling. */ };
    /** Requested positive size; zero/unsupported size yields no metrics or rasterized glyph. */
    struct FontRasterSize {
        std::uint32_t height{}; ///< Requested size magnitude in the selected sizing mode.
        FontSizing sizing = FontSizing::Pixels; ///< Pixel or character sizing policy.
    };
    /** Owned native face metadata; an empty rasterizer returns default values. */
    struct FontRasterProperties {
        std::string family; ///< Native family name, possibly empty.
        std::string style; ///< Native style name, possibly empty.
        std::uint16_t weight = 400; ///< OpenType numeric weight; missing metadata uses regular 400 or bold 700.
        bool italic{}; ///< Native face is italic.
        bool scalable{}; ///< Native face supports scalable outlines.
        bool kerning{}; ///< Native face advertises pair kerning.
    };
    /** One named font variation; enumerated by weight then width. */
    struct FontVariation {
        std::uint16_t weight = 400; ///< Named variation weight coordinate, default 400 when unavailable.
        std::uint16_t width = 100; ///< Named variation width coordinate, default 100 when unavailable.
        std::uint32_t named_instance{}; ///< One-based native named-instance selector; zero selects the default face.
    };
    /** Owned face metrics in raster pixels for the selected size; design metrics precede grid rounding. */
    struct FontRasterMetrics {
        float ascent{}; ///< Native ascender above the baseline.
        float descent{}; ///< Negative below the baseline.
        float line_spacing{}; ///< Recommended baseline-to-baseline advance.
        float design_ascent{}; ///< Scaled design metrics before pixel rounding.
        float design_descent{}; ///< Scaled design descent, normally negative below baseline.
        float design_height{};      ///< Combined ascent-minus-descent scaling before pixel rounding.
        float underline_position{}; ///< Positive below the baseline.
        float underline_thickness{}; ///< Scaled underline thickness.
        float x_height{}; ///< Height of x, or half the line spacing when unavailable.
        float bitmap_scale = 1; ///< Fixed bitmap strikes may require downscaling in character sizing.
        bool has_ellipsis{}; ///< Face maps U+2026 to a glyph.
    };
    /** Per-glyph rendering options; outline must be finite and nonnegative. */
    struct FontRasterOptions {
        bool force_autohint{}; ///< Request the native autohinter and fitted advance.
        bool bold{}; ///< Apply synthetic one-pixel emboldening and increase advance.
        float outline{}; ///< Stroke radius in raster pixels, zero for an unstroked glyph.
        bool color{}; ///< Permit native color bitmap loading; returned format identifies actual pixels.
    };
    /** Alpha stores one coverage byte per pixel; PremultipliedRGBA stores four premultiplied color channels. */
    enum class FontBitmapFormat { Alpha /**< One coverage byte per pixel. */, PremultipliedRGBA /**< Four premultiplied R,G,B,A bytes per pixel. */ };
    /** Owned glyph bitmap and placement metrics; no pointer into a native face is retained. */
    struct FontRasterGlyph {
        std::uint32_t width{}; ///< Bitmap width in pixels.
        std::uint32_t height{}; ///< Bitmap row count.
        int left{}; ///< Horizontal bitmap bearing from the pen in pixels.
        int top{}; ///< Positive above the baseline.
        float advance{}; ///< Horizontal pen advance in raster pixels.
        float left_hint_delta{}; ///< Native left bearing grid-fit adjustment in pixels.
        float right_hint_delta{}; ///< Native right bearing grid-fit adjustment in pixels.
        float bitmap_scale = 1; ///< Fixed-strike downscaling factor for character sizing.
        FontBitmapFormat format = FontBitmapFormat::Alpha; ///< Channel count/alpha interpretation of pixels.
        std::vector<unsigned char> pixels; ///< Owned, tightly packed top-to-bottom pixels.
    };

    /** CPU font operations owned by Engine. Loading copies the font bytes; copies
     * retain the same face and serialize its native state. Returned values own their
     * storage and survive the rasterizer. No graphics context or resource provider is used.
     */
    class FontRasterizer {
        struct State;
        std::shared_ptr<State> state_;
        CE_API explicit FontRasterizer(std::shared_ptr<State> state);

    public:
        FontRasterizer() = default;
        [[nodiscard]] CE_API static FontRasterizer load(
            std::span<const unsigned char> bytes,
            std::uint32_t face_index = 0,
            std::uint32_t named_instance = 0,
            bool unicode_only = false
        );
        [[nodiscard]] CE_API static FontRasterizer load(const Font& font, FontStyle style = {}, bool unicode_only = false);
        [[nodiscard]] CE_API static FontRasterizer load(const FontFace& face, bool unicode_only = false);
        [[nodiscard]] CE_API static std::vector<FontVariation>
        variations(std::span<const unsigned char> bytes, std::uint32_t face_index = 0);
        [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(state_); }
        [[nodiscard]] CE_API FontRasterProperties properties() const;
        [[nodiscard]] CE_API bool covers(char32_t scalar) const;
        [[nodiscard]] CE_API std::optional<FontRasterMetrics> metrics(FontRasterSize size) const;
        [[nodiscard]] CE_API std::optional<FontRasterGlyph>
        rasterize(char32_t scalar, FontRasterSize size, FontRasterOptions options = {}) const;
        [[nodiscard]] CE_API float kerning(char32_t left, char32_t right, FontRasterSize size, bool unfitted = false) const;
    };
}

/** @fn CE::Text::FontRasterizer::FontRasterizer()
 * Construct an empty face. Boolean conversion is false; properties are default, coverage false, metrics/rasterize absent and kerning zero.
 */

/** @fn CE::Text::FontRasterizer CE::Text::FontRasterizer::load(std::span<const unsigned char> bytes, std::uint32_t face_index, std::uint32_t named_instance, bool unicode_only)
 * Copy bytes and open a retained CPU face, independent of the source buffer. Empty/oversized bytes, face_index above 65535 or named_instance above 32767 reject. Invalid fonts/charmaps throw failed_operation; allocation errors propagate.
 * @param face_index Zero-based collection face.
 * @param named_instance Zero for default, otherwise native one-based named variation.
 * @param unicode_only Require a Unicode character map when true; otherwise retain an existing map or try Apple Roman.
 * @see [Font rasterization](cheryl-guide:assets/text-layout)
 */

/** @fn CE::Text::FontRasterizer CE::Text::FontRasterizer::load(const Font& font, FontStyle style, bool unicode_only)
 * Open the selected primary family's closest style, copying retained bytes. Empty Font rejects. No discovery/path reopening occurs; named variation defaults to zero.
 */

/** @fn CE::Text::FontRasterizer CE::Text::FontRasterizer::load(const FontFace& face, bool unicode_only)
 * Open and copy this face's bytes and collection index; the source Font need survive only the call. Native named instance defaults to zero; errors follow byte loading.
 */

/** @fn std::vector<CE::Text::FontVariation> CE::Text::FontRasterizer::variations(std::span<const unsigned char> bytes, std::uint32_t face_index)
 * Return owned named variations ordered by weight/width, empty when none are available. Opens a temporary copied face; invalid source/index and allocation failures propagate.
 */

/** @fn CE::Text::FontRasterizer::operator bool() const noexcept
 * Return whether a native CPU face is retained. Copies share serialized native state.
 */

/** @fn CE::Text::FontRasterProperties CE::Text::FontRasterizer::properties() const
 * Return owned metadata under the shared face lock, or defaults for an empty handle.
 */

/** @fn bool CE::Text::FontRasterizer::covers(char32_t scalar) const
 * Return whether this face maps a valid Unicode scalar to a nonzero glyph. Empty handles, surrogates and out-of-range values return false; no fallback family is selected.
 */

/** @fn std::optional<CE::Text::FontRasterMetrics> CE::Text::FontRasterizer::metrics(FontRasterSize size) const
 * Return copied raster-pixel metrics, or nullopt for empty/unsupported size. Fixed-strike bitmap_scale describes downscaling. Shared face/cache access is serialized; allocation errors may propagate.
 */

/** @fn std::optional<CE::Text::FontRasterGlyph> CE::Text::FontRasterizer::rasterize(char32_t scalar, FontRasterSize size, FontRasterOptions options) const
 * Return owned top-to-bottom pixels and metrics. Empty/invalid scalar, unsupported size/options or native raster failure returns nullopt; allocation failures propagate. A missing character uses the face's native missing glyph: call covers() for explicit fallback policy. No graphics/provider calls occur.
 */

/** @fn float CE::Text::FontRasterizer::kerning(char32_t left, char32_t right, FontRasterSize size, bool unfitted) const
 * Return horizontal pair adjustment in pixels. Empty/invalid scalars, unsupported size, absent kerning or native failure return zero. unfitted requests unrounded native kerning; the shared face is serialized.
 */
