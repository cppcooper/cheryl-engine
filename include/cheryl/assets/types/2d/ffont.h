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

    struct FFont final : Font,
                         Singleton_CTS<FFont> {
    private:
        const std::array<float, num_chars_ffont> widths;

    public:
        // call FFont::get(load_ffont(widths_file)) for construction
        explicit FFont(
            const FFontData& data
        )
        : Font({std::get<1>(data), std::get<2>(data)}), widths(std::get<0>(data)) {}
        ~FFont() override = default;
        [[nodiscard]] std::vector<GlyphPlacement2D> layout(
            std::string_view text,
            FontLayoutOptions options = FontLayoutOptions{}
        ) const override;
        static FFontData load_ffont(
            const std::filesystem::path& path,
            ResourceProvider& provider
        );
    };
}
#endif
