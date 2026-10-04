#pragma once

#include <assets/types/2d/stbfont.h>
#include <memory_resource>

namespace CE::Assets::FontDetail {
    // Private allocation boundary for real stb rasterization. The public loader
    // uses new_delete_resource; acceptance tests can reject individual requests.
    [[nodiscard]] STBFontData load_font_with_resource(
        const std::filesystem::path& path,
        int font_size,
        ResourceProvider& provider,
        std::pmr::memory_resource& memory
    );
}
