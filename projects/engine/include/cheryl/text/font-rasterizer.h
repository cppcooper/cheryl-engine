#pragma once

#include <cheryl/export.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace CE::Text {
    enum class FontSizing { Pixels, Characters };
    struct FontRasterSize {
        std::uint32_t height{};
        FontSizing sizing = FontSizing::Pixels;
    };
    struct FontRasterProperties {
        std::string family;
        std::string style;
        std::uint16_t weight = 400;
        bool italic{};
        bool scalable{};
        bool kerning{};
    };
    struct FontVariation {
        std::uint16_t weight = 400;
        std::uint16_t width = 100;
        std::uint32_t named_instance{};
    };
    struct FontRasterMetrics {
        float ascent{};
        float descent{}; ///< Negative below the baseline.
        float line_spacing{};
        float design_ascent{}; ///< Scaled design metrics before pixel rounding.
        float design_descent{};
        float design_height{};      ///< Combined ascent-minus-descent scaling before pixel rounding.
        float underline_position{}; ///< Positive below the baseline.
        float underline_thickness{};
        float x_height{};
        float bitmap_scale = 1; ///< Fixed bitmap strikes may require downscaling in character sizing.
        bool has_ellipsis{};
    };
    struct FontRasterOptions {
        bool force_autohint{};
        bool bold{};
        float outline{}; ///< Stroke radius in raster pixels, zero for an unstroked glyph.
        bool color{};
    };
    enum class FontBitmapFormat { Alpha, PremultipliedRGBA };
    struct FontRasterGlyph {
        std::uint32_t width{};
        std::uint32_t height{};
        int left{};
        int top{}; ///< Positive above the baseline.
        float advance{};
        float left_hint_delta{};
        float right_hint_delta{};
        float bitmap_scale = 1;
        FontBitmapFormat format = FontBitmapFormat::Alpha;
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
        [[nodiscard]] CE_API static std::vector<FontVariation>
        variations(std::span<const unsigned char> bytes, std::uint32_t face_index = 0);
        [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(state_); }
        [[nodiscard]] CE_API FontRasterProperties properties() const;
        [[nodiscard]] CE_API bool covers(char32_t scalar) const;
        /** Unsupported sizes return no result; dimensions and metrics remain in
         * raster pixels. A fixed strike's bitmap_scale describes optional downscaling.
         */
        [[nodiscard]] CE_API std::optional<FontRasterMetrics> metrics(FontRasterSize size) const;
        /** Missing scalars rasterize the face's replacement glyph. Use covers()
         * when a caller needs explicit fallback selection. Failed rasterization
         * returns no result; allocation failures propagate.
         */
        [[nodiscard]] CE_API std::optional<FontRasterGlyph>
        rasterize(char32_t scalar, FontRasterSize size, FontRasterOptions options = {}) const;
        [[nodiscard]] CE_API float kerning(char32_t left, char32_t right, FontRasterSize size, bool unfitted = false) const;
    };
}
