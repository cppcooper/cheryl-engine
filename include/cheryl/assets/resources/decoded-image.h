#pragma once

#include "image.h"

#include <filesystem>
#include <vector>

namespace CE::Assets {
    // Owned top-to-bottom, four-channel pixels, independent of any graphics context.
    struct DecodedImage {
        PixelSize size;
        std::vector<unsigned char> rgba;
    };
    [[nodiscard]] DecodedImage decode_image(const std::filesystem::path& file);
}
