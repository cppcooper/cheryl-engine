#pragma once
#include <assets/abstracts.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

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
    /** Baked ASCII glyph quads, advances, and alpha atlas for immediate text submission.
     * print() sets text/angle and draws synchronously through the supplied material.
     */
    struct STBFont final : Font {
        explicit STBFont(STBFontData data);
        ~STBFont() override = default;
        void print(std::string text, FontDrawInfo* format) override;
        [[nodiscard]] static STBFontData load_font(const std::filesystem::path& font_path, int font_size,
                                                   ResourceProvider& provider);

    protected:
        void draw(const DrawInfo& info) override;

    private:
        std::array<float, font_character_count> advances_{};
        float line_height_{};
        float print_angle_{};
        std::string print_message_;
    };
}
