#pragma once
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
    constexpr uint16_t bitmap_character_count = 256;
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
        [[nodiscard]] std::vector<GlyphPlacement> layout(std::string_view text, LayoutOptions options = LayoutOptions{}) const override;
        /** Read 256 native shorts from a binary input file before any provider upload.
         * Incomplete reads fail; legacy trailing bytes are ignored. Metadata/endian
         * semantics remain unverified; see docs/resources/legacy-ffont.md for
         * the deprecation decision and preserved legacy behavior.
         */
        static BitmapFontData load_ffont(const std::filesystem::path& path, CE::Rendering::Resources::Provider& provider);
    };
}

#endif
