#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <format>

namespace CE::Memory {

    /** Formats an owned, one-decimal binary-unit size string.
     * @param bytes Unsigned byte count; this does not query an allocation or residency.
     * @return Decimal magnitude followed by bytes/KiB/MiB and later binary suffixes.
     * Exact powers of 1024 start the next unit. Unit selection precedes rounding,
     * so a value near a boundary can display 1024.0 in the preceding unit.
     * Zero formats as "0.0 bytes"; formatting/allocation failures propagate.
     * @see [Utility contracts](cheryl-guide:runtime/utility-contracts)
     */
    inline std::string format_bytes(const std::size_t bytes) {
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

}
