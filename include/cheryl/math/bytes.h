#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <format>

// Choose the binary unit using the integer byte count, then round to one decimal.
// Exact powers of 1024 begin the next unit; rounding near a boundary may display
// 1024.0 in the preceding unit. Zero is "0.0 bytes"; suffixes extend through YiB.
inline std::string human_readable(const std::size_t bytes) {
    constexpr std::array suffixes{" bytes", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"};
    double XiB = static_cast<double>(bytes);
    auto whole = bytes;
    std::size_t counter = 0;
    while (whole >= 1024 && counter + 1 < suffixes.size()) {
        whole /= 1024;
        XiB /= 1024;
        counter++;
    }
    return std::format("{:3.1f}{}", XiB, suffixes[counter]);
}
