#pragma once

#include <cheryl/export.h>

#include <text/font-collection.h>
#include <text/font.h>
#include <text/utf8.h>

#include <cstddef>
#include <cstdint>
#include <compare>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace CE::Text {
    /** Paragraph base direction: Automatic uses the first strong character, defaulting to LTR; explicit values set the bidi base, not glyph source order. */
    enum class ParagraphDirection { Automatic /**< First strong character, otherwise LTR. */, LeftToRight /**< Explicit LTR paragraph base. */, RightToLeft /**< Explicit RTL paragraph base. */ };
    /** Owned shaping options; positions and widths use local Y-up pixels, independent of framebuffer scaling. */
    struct LayoutOptions {
        std::uint32_t pixel_height = 32; ///< Positive font em size in pixels.
        ::CE::Text::ParagraphDirection direction = ::CE::Text::ParagraphDirection::Automatic; ///< Bidi paragraph base direction.
        std::optional<float> maximum_width; ///< Positive finite local wrapping width; unset uses explicit line breaks only.
        std::string language = "und"; ///< BCP 47 shaping hint; und is unspecified, not an admitted-script restriction.
        FontStyle font_style; ///< Requested face style when shaping a retained Font; collection overload uses its existing faces.
    };
    /** Original UTF-8 byte and decoded-scalar ranges; indices are logical source positions, not visual glyph positions. */
    struct SourceRange {
        std::size_t byte_offset{}; ///< Zero-based original byte offset.
        std::size_t byte_count{}; ///< Number of original bytes represented.
        std::size_t scalar_offset{}; ///< Zero-based decoded scalar offset.
        std::size_t scalar_count{}; ///< Number of decoded scalars represented.
        [[nodiscard]] bool operator==(const SourceRange&) const = default;
    };
    /** Face-qualified glyph identity, meaningful only within the associated retained FontCollection. */
    struct GlyphId {
        std::size_t face{}; ///< Zero-based retained collection face; not a process-global font ID.
        std::uint32_t glyph{}; ///< Native glyph index within that face.
        [[nodiscard]] bool operator==(const GlyphId&) const = default;
        [[nodiscard]] auto operator<=>(const GlyphId&) const = default;
    };
    /** One glyph in visual draw order with its original shaping-cluster range. */
    struct ShapedGlyph {
        ::CE::Text::GlyphId id; ///< Qualified face/glyph used during rasterization.
        ::CE::Text::SourceRange source; ///< Original shaping cluster; several glyphs can share it.
        float x{} /**< Local pen X. */, y{} /**< Local Y-up pen position. */, advance{}; ///< Horizontal glyph advance in local pixels.
        bool missing{}; ///< An uncovered grapheme was replaced by the embedded U+FFFD.
    };
    /** One accepted line, retaining source membership and its range in the visual glyph array. */
    struct TextLine {
        ::CE::Text::SourceRange source; ///< Original logical source range of this line.
        std::size_t first_glyph{} /**< First glyph-array index. */, glyph_count{}; ///< Number of glyphs in this line.
        float width{} /**< Accepted visual width in local pixels. */, baseline{}; ///< Local Y-up baseline position.
        bool right_to_left{} /**< Resolved paragraph direction. */, overflow{}; ///< An indivisible grapheme exceeds maximum_width.
    };

    /** Owned immutable layout and original scalar/byte mappings. No source string,
     * provider, native resources or mutable font face is retained. Instances retain
     * their font-byte snapshot, so subsequent glyph preparation uses the same faces.
     */
    class ShapedText {
        ::CE::Text::FontCollection fonts_;
        ::CE::Text::LayoutOptions options_;
        std::vector<::CE::Text::Utf8Scalar> scalars_;
        std::vector<::CE::Text::ShapedGlyph> glyphs_;
        std::vector<::CE::Text::TextLine> lines_;
        float line_height_{};

        CE_API ShapedText(::CE::Text::FontCollection fonts, ::CE::Text::LayoutOptions options);
        friend CE_API ShapedText layout_text(const FontCollection&, std::string_view, const LayoutOptions&);

    public:
        [[nodiscard]] const ::CE::Text::FontCollection& fonts() const noexcept { return fonts_; }
        [[nodiscard]] const ::CE::Text::LayoutOptions& options() const noexcept { return options_; }
        [[nodiscard]] std::span<const ::CE::Text::Utf8Scalar> scalars() const noexcept { return scalars_; }
        [[nodiscard]] std::span<const ::CE::Text::ShapedGlyph> glyphs() const noexcept { return glyphs_; }
        [[nodiscard]] std::span<const ::CE::Text::TextLine> lines() const noexcept { return lines_; }
        [[nodiscard]] float line_height() const noexcept { return line_height_; }
    };

    [[nodiscard]] CE_API ShapedText layout_text(const FontCollection& fonts, std::string_view text, const LayoutOptions& options = {});
    [[nodiscard]] CE_API ShapedText layout_text(const Font& font, std::string_view text, const LayoutOptions& options = {});
}

/** @fn const CE::Text::FontCollection& CE::Text::ShapedText::fonts() const noexcept
 * Borrow the immutable font collection retained by this layout. Keep this ShapedText alive and unmoved while using the reference; copy the collection for independent retention.
 */

/** @fn const CE::Text::LayoutOptions& CE::Text::ShapedText::options() const noexcept
 * Borrow the options copied during layout. The reference is invalidated by destruction/move of this value.
 */

/** @fn std::span<const CE::Text::Utf8Scalar> CE::Text::ShapedText::scalars() const noexcept
 * Borrow decoded logical-order scalar/source records, including malformed-input replacements; retain this unmoved layout.
 */

/** @fn std::span<const CE::Text::ShapedGlyph> CE::Text::ShapedText::glyphs() const noexcept
 * Borrow visually ordered glyphs and their logical shaping-cluster ranges; retain this unmoved layout.
 */

/** @fn std::span<const CE::Text::TextLine> CE::Text::ShapedText::lines() const noexcept
 * Borrow accepted line records and glyph ranges; empty/source-only text can have no visible glyphs. Retain this unmoved layout.
 */

/** @fn float CE::Text::ShapedText::line_height() const noexcept
 * Return the selected font line advance in local pixels, independent of the number of lines.
 */

/** @fn CE::Text::ShapedText CE::Text::layout_text(const FontCollection& fonts, std::string_view text, const LayoutOptions& options)
 * Shape on CPU with bidi resolution and whole-grapheme fallback; retain font bytes but not the input view. Soft wrapping reshapes accepted lines and can break oversized words only at grapheme boundaries. Explicit CR/LF/CRLF/NEL/paragraph/line separators work; trailing ASCII spaces/tabs have no visual advance and tabs expand to four spaces.
 * Invalid size, width, direction/language or moved-from collection rejects layout; shaping/allocation errors propagate.
 * @param fonts Stable immutable collection retained in the returned value.
 * @param text UTF-8 borrowed for the call; replacements preserve malformed source byte ranges.
 * @param options Local sizing, direction, wrapping and language settings.
 * @return Owned immutable layout ready for CPU glyph preparation, without native resources.
 * @see [Text layout](cheryl-guide:assets/text-layout#shaping-direction-and-width)
 */

/** @fn CE::Text::ShapedText CE::Text::layout_text(const Font& font, std::string_view text, const LayoutOptions& options)
 * Shape using font.collection(options.font_style), selecting one nearest face per retained family. The original Context is unnecessary; an empty Font rejects. Source ownership, wrapping and errors otherwise follow the collection overload.
 */

/** @fn bool CE::Text::SourceRange::operator==(const SourceRange&) const
 * Compare both original byte and decoded scalar ranges by value.
 */

/** @fn bool CE::Text::GlyphId::operator==(const GlyphId&) const
 * Compare face and glyph indices within the same retained collection. Equal numbers from unrelated collections do not identify the same font glyph.
 */

/** @fn auto CE::Text::GlyphId::operator<=>(const GlyphId&) const
 * Order first by collection-local face index, then native glyph index; this is a storage order rather than visual text order.
 */
