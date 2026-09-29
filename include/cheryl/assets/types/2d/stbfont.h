#pragma once
#include <assets/types/2d/font.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace CE::Assets {
    struct ResourceProvider;

    inline constexpr unsigned char first_font_character = 32;
    inline constexpr unsigned char last_font_character = 126;
    inline constexpr std::size_t font_character_count = last_font_character - first_font_character + 1;

    struct STBFontData {
        std::shared_ptr<Geometry2D> geometry;
        std::shared_ptr<Image> texture;
        std::array<float, font_character_count> advances{};
        float line_height{};
    };

    // TODO: A Unicode/text-layout service must decode code points and shape glyph runs before
    // drawing; this atlas covers only printable ASCII and the current draw loop treats bytes as
    // characters (multi-byte UTF-8 sequences each produce separate fallback glyphs).
    /** Baked ASCII glyph quads, advances, and alpha atlas. Layout reads immutable
     * metrics, so published text commands can share a font without changing it.
     */
    struct STBFont final : Font {
        explicit STBFont(STBFontData data);
        ~STBFont() override = default;
        void print(std::string text, FontDrawInfo* format) override;
        [[nodiscard]] static STBFontData load_font(const std::filesystem::path& font_path, int font_size,
                                                   ResourceProvider& provider);

        [[nodiscard]] const Geometry2D& glyph_geometry() const { return *geometry; }
        [[nodiscard]] const Image& glyph_atlas() const { return *texture; }

        // Emit a baked glyph index and its local pen offset without storing the
        // message or changing the font. The caller supplies its own draw policy.
        template <typename SubmitGlyph>
        void for_each_glyph(std::string_view text, SubmitGlyph&& submit) const {
            float cursor_x = 0.0f;
            float cursor_y = 0.0f;
            constexpr auto fallback = static_cast<unsigned char>('?');
            constexpr auto space_index = static_cast<std::size_t>(' ' - first_font_character);
            for (const unsigned char requested : text) {
                if (requested == '\n') {
                    cursor_x = 0.0f;
                    cursor_y -= line_height_;
                    continue;
                }
                if (requested == '\r') continue;
                if (requested == '\t') {
                    cursor_x += advances_[space_index] * 4.0f;
                    continue;
                }
                const auto letter = requested < first_font_character || requested > last_font_character
                    ? fallback : requested;
                const auto index = static_cast<std::size_t>(letter - first_font_character);
                if (letter != ' ') submit(index, cursor_x, cursor_y);
                cursor_x += advances_[index];
            }
        }

    private:
        std::array<float, font_character_count> advances_{};
        float line_height_{};
    };
}
