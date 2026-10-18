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
    /** First baked printable ASCII scalar, inclusive (space). */
    inline constexpr unsigned char ascii_first_character = 32;
    /** Last baked printable ASCII scalar, inclusive (~). */
    inline constexpr unsigned char ascii_last_character = 126;
    /** Number of baked printable ASCII glyph slots, including space. */
    inline constexpr std::size_t ascii_character_count = ascii_last_character - ascii_first_character + 1;

    /** Retained atlas/geometry and copied pixel metrics used to construct an immutable ASCII font. */
    struct AsciiFontData {
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D> geometry; ///< Triangle-list geometry with six vertices per printable ASCII slot.
        std::shared_ptr<CE::Rendering::Resources::Image> texture; ///< Baked alpha atlas in the selected provider domain.
        std::array<float, ascii_character_count> advances{}; ///< Horizontal advances in baked local pixels, indexed from ascii_first_character.
        float line_height{}; ///< Positive local pixel line advance.
        std::shared_ptr<const CE::Rendering::Resources::Material> material = {}; ///< Retained optional material; load_font supplies a provider default when absent.
    };

    // This legacy ASCII path decodes UTF-8 but does not perform Unicode shaping.
    // Use CE::Text::layout_text for retained shaping and whole-grapheme fallback.
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
        [[nodiscard]] CE_API static AsciiFontData load_font(
            const std::filesystem::path& font_path,
            int font_size,
            CE::Rendering::Resources::Provider& provider,
            std::shared_ptr<const CE::Rendering::Resources::Material> material = {}
        );

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

/** @fn CE::Text::Legacy::AsciiFont::AsciiFont(AsciiFontData data)
 * Retain/move baked resources and copy advances/line height. Reject missing geometry/atlas; metrics are copied without validation. Native use requires the resources' backend domain.
 */

/** @fn CE::Text::Legacy::AsciiFont::~AsciiFont()
 * Release retained handles; callers' resource copies remain independently owned.
 */

/** @fn std::vector<CE::Text::Legacy::GlyphPlacement> CE::Text::Legacy::AsciiFont::layout(std::string_view text, LayoutOptions options) const
 * Return owned baked pixel offsets after UTF-8 decoding. Newline moves down one line, CR is ignored, tab advances four spaces, and each unsupported scalar/malformed subpart selects ?. Spaces advance without a quad. alternate_bank throws; no shaping or non-ASCII glyphs are supplied.
 */

/** @fn CE::Text::Legacy::AsciiFontData CE::Text::Legacy::AsciiFont::load_font(const std::filesystem::path& font_path, int font_size, CE::Rendering::Resources::Provider& provider, std::shared_ptr<const CE::Rendering::Resources::Material> material)
 * Bake printable ASCII from the first face of the supplied file at positive font_size pixels, then upload on the provider owner/current context. Empty material requests the provider's default text material. File, bake, provider and allocation failures throw; no cache entry is published and transient native uploads retire normally. Discovery is optional and bundled fonts use the same path.
 * @see [Text path choices](cheryl-guide:assets/text-layout#choose-a-text-path)
 */

/** @fn const CE::Rendering::Geometry::Buffer2D& CE::Text::Legacy::AsciiFont::glyph_geometry() const
 * Borrow baked geometry; keep the font or a retained geometry handle alive. Native access requires the live backend domain.
 */

/** @fn const CE::Rendering::Resources::Image& CE::Text::Legacy::AsciiFont::glyph_atlas() const
 * Borrow the baked atlas; keep the font or copied atlas handle alive for the reference lifetime.
 */

/** @fn template <typename SubmitGlyph> void CE::Text::Legacy::AsciiFont::for_each_glyph(std::string_view text, SubmitGlyph&& submit) const
 * Synchronously invoke submit(index,x,y) for each visible baked glyph using the layout decoding/newline/tab policy. Index addresses a six-vertex quad; X/Y use local Y-up pixels. No text/callback is retained and the font is unchanged. Callback/allocation failures propagate; prior callback effects are not rolled back.
 * @tparam SubmitGlyph Callable accepting a glyph-slot index and two float offsets.
 */
