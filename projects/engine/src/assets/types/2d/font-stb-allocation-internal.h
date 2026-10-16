#pragma once

#include <cheryl/export.h>

#include <assets/types/2d/stbfont.h>
#include <memory_resource>

namespace CE::Text::Legacy::Detail {
    // Private allocation boundary for real stb rasterization. The public loader
    // uses new_delete_resource; acceptance tests can reject individual requests.
    [[nodiscard]] CE_API AsciiFontData load_font_with_resource(
        const std::filesystem::path& path,
        int font_size,
        CE::Rendering::Resources::Provider& provider,
        std::pmr::memory_resource& memory
    );
}
