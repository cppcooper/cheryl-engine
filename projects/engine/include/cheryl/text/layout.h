#pragma once

#include <text/font-collection.h>
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
    enum class ParagraphDirection { Automatic, LeftToRight, RightToLeft };
    struct LayoutOptions {
        std::uint32_t pixel_height = 32; // Font em size; positions use local Y-up pixels.
        ParagraphDirection direction = ParagraphDirection::Automatic;
        std::optional<float> maximum_width; // Positive finite local width; unset uses explicit breaks.
        std::string language = "und"; // BCP 47 hint, not a restriction on admitted scripts.
    };
    struct SourceRange {
        std::size_t byte_offset{};
        std::size_t byte_count{};
        std::size_t scalar_offset{};
        std::size_t scalar_count{};
        [[nodiscard]] bool operator==(const SourceRange&) const = default;
    };
    struct GlyphId {
        std::size_t face{}; // Qualified by the retained FontCollection, not globally unique.
        std::uint32_t glyph{};
        [[nodiscard]] bool operator==(const GlyphId&) const = default;
        [[nodiscard]] auto operator<=>(const GlyphId&) const = default;
    };
    struct ShapedGlyph {
        GlyphId id;
        SourceRange source; // Shaping cluster; multiple glyphs can share the range.
        float x{}, y{}, advance{};
        bool missing{}; // A whole uncovered grapheme became a builtin U+FFFD.
    };
    struct TextLine {
        SourceRange source;
        std::size_t first_glyph{}, glyph_count{};
        float width{}, baseline{};
        bool right_to_left{}, overflow{}; // Overflow is an indivisible grapheme wider than the constraint.
    };

    /** Owned immutable layout and original scalar/byte mappings. No source string,
     * provider, native resources or mutable font face is retained. Instances retain
     * their font-byte snapshot, so subsequent glyph preparation uses the same faces.
     */
    class ShapedText {
        FontCollection fonts_;
        LayoutOptions options_;
        std::vector<Utf8Scalar> scalars_;
        std::vector<ShapedGlyph> glyphs_;
        std::vector<TextLine> lines_;
        float line_height_{};

        ShapedText(FontCollection fonts, LayoutOptions options);
        friend ShapedText layout_text(const FontCollection&, std::string_view, const LayoutOptions&);

    public:
        [[nodiscard]] const FontCollection& fonts() const noexcept { return fonts_; }
        [[nodiscard]] const LayoutOptions& options() const noexcept { return options_; }
        [[nodiscard]] std::span<const Utf8Scalar> scalars() const noexcept { return scalars_; }
        [[nodiscard]] std::span<const ShapedGlyph> glyphs() const noexcept { return glyphs_; }
        [[nodiscard]] std::span<const TextLine> lines() const noexcept { return lines_; }
        [[nodiscard]] float line_height() const noexcept { return line_height_; }
    };

    // CPU-only Unicode bidi/shaping. Fallback selects whole graphemes. Soft wrapping
    // reshapes chosen lines and breaks oversized words at grapheme boundaries; no
    // grapheme is split. Explicit CR/LF/CRLF/NEL/paragraph and line separators work.
    // Trailing ASCII spaces/tabs have no visual advance; tab expands to four spaces.
    [[nodiscard]] ShapedText layout_text(const FontCollection& fonts, std::string_view text, const LayoutOptions& options = {});
}
