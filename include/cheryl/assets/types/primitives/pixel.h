#pragma once

#include <cstdint>

namespace CE::Assets {
    struct PixelPoint {
        std::uint32_t x{};
        std::uint32_t y{};
    };

    struct PixelSize {
        std::uint32_t width{};
        std::uint32_t height{};
    };

    struct PixelRect {
        std::uint64_t x{};
        std::uint64_t y{};
        std::uint32_t width{};
        std::uint32_t height{};
    };
}
