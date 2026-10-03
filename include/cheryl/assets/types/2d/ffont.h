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

namespace CE::Assets {
    struct ResourceProvider;

    constexpr uint16_t num_chars_ffont = 256;
    using FFontData = std::tuple<std::array<float, num_chars_ffont>, std::shared_ptr<Geometry2D>, std::shared_ptr<Image>>;

    /** Deprecated legacy bitmap font with native-short widths and a separate
     * whitefont.png atlas. The original atlas is unavailable; use STBFont to
     * bake metrics and an atlas from a system or bundled font file.
     */
    struct [[deprecated("Use STBFont with a system or bundled font file.")]] FFont final : Font,
                                                                                            Singleton_CTS<FFont> {
    private:
        const std::array<float, num_chars_ffont> widths;

    public:
        // Configure once with initialize(load_ffont(widths_file, provider)) before
        // readers start. get() retrieves the published font without creating it.
        explicit FFont(const FFontData& data)
        : Font({std::get<1>(data), std::get<2>(data)}), widths(std::get<0>(data)) {}
        ~FFont() override = default;
        [[nodiscard]] std::vector<GlyphPlacement2D>
        layout(std::string_view text, FontLayoutOptions options = FontLayoutOptions{}) const override;
        /** Read 256 native shorts from a binary input file before any provider upload.
         * Incomplete reads fail; legacy trailing bytes are ignored. Metadata/endian
         * semantics remain unverified; see docs/resources/legacy-ffont.md for
         * the deprecation decision and preserved legacy behavior.
         */
        static FFontData load_ffont(const std::filesystem::path& path, ResourceProvider& provider);
    };
}
#endif
