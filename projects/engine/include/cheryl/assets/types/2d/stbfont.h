#pragma once

#include <cheryl/export.h>
#include <assets/types/2d/font.h>
#include <text/utf8.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace CE::Rendering::Resources {
    struct Provider;
}

namespace CE::Text::Legacy {
    inline constexpr unsigned char ascii_first_character = 32;
    inline constexpr unsigned char ascii_last_character = 126;
    inline constexpr std::size_t ascii_character_count = ascii_last_character - ascii_first_character + 1;

    struct AsciiFontData {
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D> geometry;
        std::shared_ptr<CE::Rendering::Resources::Image> texture;
        std::array<float, ascii_character_count> advances{};
        float line_height{};
    };

    // TODO: Full glyph layout needs selected shaping, fallback fonts and retained atlas generations.
    // UTF-8 decoding here selects ASCII glyphs or one fallback per unsupported scalar/subpart.
    /** Baked ASCII glyph quads, advances, and alpha atlas. Layout reads immutable
     * metrics, so published text commands can share a font without changing it.
     */
    struct CE_TYPE_API AsciiFont final : Font {
    private:
        std::array<float, ascii_character_count> advances_{};
        float line_height_{};

    public:
        CE_API explicit AsciiFont(AsciiFontData data);
        ~AsciiFont() override = default;
        // Baked pixel offsets; newline moves down one line, CR is ignored, tab advances
        // four spaces, other unsupported UTF-8 scalars or malformed subparts select '?'.
        // This ASCII atlas has no shaping or non-ASCII glyphs. alternate_bank throws.
        [[nodiscard]] CE_API std::vector<GlyphPlacement>
        layout(std::string_view text, LayoutOptions options = LayoutOptions{}) const override;
        /** Bake printable ASCII from a caller-supplied font file at a positive
         * font_size in pixels. Upload obeys provider affinity; failure returns no data
         * and publishes no cache entry. System discovery is optional; bundled fonts
         * use this same path. The first face is selected for a collection.
         */
        [[nodiscard]] CE_API static AsciiFontData
        load_font(const std::filesystem::path& font_path, int font_size, CE::Rendering::Resources::Provider& provider);

        [[nodiscard]] const CE::Rendering::Geometry::Buffer2D& glyph_geometry() const { return *geometry; }
        [[nodiscard]] const CE::Rendering::Resources::Image& glyph_atlas() const { return *texture; }

        // Emit a baked glyph index and its local pen offset without storing the
        // message or changing the font. Callback exceptions propagate; prior callback
        // effects are not rolled back. The caller supplies its own draw policy.
        template <typename SubmitGlyph> void for_each_glyph(std::string_view text, SubmitGlyph&& submit) const {
            float cursor_x = 0.0f;
            float cursor_y = 0.0f;
            constexpr auto fallback = U'?';
            constexpr auto space_index = static_cast<std::size_t>(' ' - ascii_first_character);
            std::size_t offset = 0;
            while (const auto scalar = decode_utf8_scalar(text, offset)) {
                offset += scalar->byte_count;
                const auto requested = scalar->value;
                if (requested == '\n') {
                    cursor_x = 0.0f;
                    cursor_y -= line_height_;
                    continue;
                }
                if (requested == '\r')
                    continue;
                if (requested == '\t') {
                    cursor_x += advances_[space_index] * 4.0f;
                    continue;
                }
                const auto letter = requested < ascii_first_character || requested > ascii_last_character ? fallback : requested;
                const auto index = static_cast<std::size_t>(letter - ascii_first_character);
                if (letter != ' ')
                    submit(index, cursor_x, cursor_y);
                cursor_x += advances_[index];
            }
        }
    };
}
