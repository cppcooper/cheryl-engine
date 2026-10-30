#pragma once

#include <cheryl/export.h>
#ifndef FFONT_H
#define FFONT_H

#include <assets/types/2d/font.h>
#include <templates/singleton.h>

#include <filesystem>
#include <tuple>
#include <array>
#include <memory>
#include <vector>

namespace CE::Rendering::Resources {
    struct Provider;
}

namespace CE::Text::Legacy {
    struct CE_TYPE_API BitmapFont;
    /** Number of fixed legacy atlas slots, including the alternate bank. */
    constexpr uint16_t bitmap_character_count = 256;
    /** Owned width table plus retained geometry/atlas handles for BitmapFont construction. */
    using BitmapFontData = std::tuple<
        std::array<float, bitmap_character_count>,
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D>,
        std::shared_ptr<CE::Rendering::Resources::Image>>;

    /** Deprecated legacy bitmap font with native-short widths and a separate
     * whitefont.png atlas. Recovered inputs remain usable by this binary loader;
     * prefer CE::Text::Legacy::AsciiFont to bake metrics and an atlas from a supplied font file.
     */
    struct [[deprecated("Use CE::Text::Legacy::AsciiFont with a system or bundled font file.")]] BitmapFont final : Font,
                                                                                                CE::State::Singleton<BitmapFont> {
    private:
        const std::array<float, bitmap_character_count> widths;

    public:
        // Configure once with initialize(load_ffont(widths_file, provider)) before
        // readers start. get() retrieves the published font without creating it.
        explicit BitmapFont(const BitmapFontData& data)
        : Font({std::get<1>(data), std::get<2>(data)}), widths(std::get<0>(data)) {}
        ~BitmapFont() override = default;
        [[nodiscard]] CE_API std::vector<GlyphPlacement>
        layout(std::string_view text, LayoutOptions options = LayoutOptions{}) const override;
        CE_API static BitmapFontData load_ffont(const std::filesystem::path& path, CE::Rendering::Resources::Provider& provider);
    };
}

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
namespace CE::State {
    template <> CE_API auto Singleton<CE::Text::Legacy::BitmapFont>::storage() -> Storage&;
}
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif

/** @fn CE::Text::Legacy::BitmapFont::BitmapFont(const BitmapFontData& data)
 * Copy the width table and retain geometry/atlas. Configure the singleton once with initialize(load_ffont(...)) before readers start; get() only retrieves the published instance.
 */

/** @fn CE::Text::Legacy::BitmapFont::~BitmapFont()
 * Release this legacy font's logical resource handles; retained draw packets keep their own handles.
 */

/** @fn std::vector<CE::Text::Legacy::GlyphPlacement> CE::Text::Legacy::BitmapFont::layout(std::string_view text, LayoutOptions options) const
 * Interpret bytes as printable ASCII, replacing other bytes with ?. Newline resets X and moves Y by -1/128; CR is ignored. Widths are divided by 128. alternate_bank adds128 to glyph indices; spaces advance without draws. This deprecated path does not decode UTF-8 or shape text; allocation failures propagate.
 */

/** @fn CE::Text::Legacy::BitmapFontData CE::Text::Legacy::BitmapFont::load_ffont(const std::filesystem::path& path, CE::Rendering::Resources::Provider& provider)
 * Read256 native shorts before uploading geometry. Incomplete reads fail; trailing bytes are ignored. Requires whitefont.png already present in the image manager. Metadata/endian semantics remain unverified. Uses the provider loading owner/current context; file/provider/allocation failures propagate and partial native resources retire normally.
 * @see [Legacy format limits](cheryl-guide:resources/legacy-ffont)
 */

/** @fn template <> auto CE::State::Singleton<CE::Text::Legacy::BitmapFont>::storage() -> Storage&
 * Return the process-local singleton storage for this deprecated font specialization. Use initialize/get rather than modifying storage directly.
 */
