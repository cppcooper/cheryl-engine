#pragma once

#include <internals/exceptions.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace CE::Assets::FontDetail {
    template <typename Allocator = std::allocator<unsigned char>> struct BakedAlphaAtlas {
        std::vector<unsigned char, Allocator> pixels;
        int size;
    };

    // Private retry boundary. The production baker supplies stb's status; a
    // fixture can reject a real scoped vector allocation without global hooks.
    template <typename Baker, typename Allocator = std::allocator<unsigned char>>
    [[nodiscard]] BakedAlphaAtlas<Allocator> bake_font_atlas(
        const std::filesystem::path& path,
        Baker&& bake,
        const Allocator& allocator = Allocator{}
    ) {
        BakedAlphaAtlas<Allocator> atlas{std::vector<unsigned char, Allocator>(allocator), 256};
        while (true) {
            // Retry the whole printable range at double resolution; a partial
            // bake cannot supply stable glyph indices for drawing.
            atlas.pixels.assign(static_cast<std::size_t>(atlas.size) * atlas.size, 0);
            if (bake(std::span<unsigned char>{atlas.pixels}, atlas.size) > 0)
                return atlas;
            if (atlas.size == 4096)
                throw Exceptions::runtime_exception(CE_HERE, "Font glyphs do not fit in an atlas: '" + path.string() + "'");
            atlas.size *= 2;
        }
    }
}
