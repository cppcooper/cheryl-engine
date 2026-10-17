#pragma once

#include <cheryl/export.h>

#include "image.h"

#include <filesystem>
#include <span>
#include <vector>

namespace CE::Assets::Images {
    // Owned top-to-bottom, four-channel pixels, independent of any graphics context.
    struct Decoded {
        CE::Rendering::Geometry::PixelSize size;
        std::vector<unsigned char> rgba;
    };
    // CPU-only file decode; throws on unreadable/unsupported data or storage overflow.
    // Returns owned pixels without publishing a resource/cache entry.
    [[nodiscard]] CE_API Decoded decode(const std::filesystem::path& file);
    // CPU-only memory decode with the same owned RGBA representation as file decoding.
    [[nodiscard]] CE_API Decoded decode(std::span<const unsigned char> bytes);
}
