#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace CE::Text {
    // An owned scalar and its original byte range. replaced distinguishes malformed
    // input from an explicitly encoded U+FFFD. Records carry no source reference.
    // Scalar order is logical source order, not a grapheme or shaping-cluster index.
    struct Utf8Scalar {
        char32_t value{};
        std::size_t byte_offset{};
        std::size_t byte_count{};
        bool replaced{};

        [[nodiscard]] bool operator==(const Utf8Scalar&) const = default;
    };

    // Decode at offset, or return no value at/beyond the end. Malformed input emits
    // U+FFFD for one maximal subpart; byte_count always advances within the view.
    // Truncated prefixes are errors at this view's end; this is not a streaming API.
    // The caller keeps input stable for the call and advances by returned byte_count.
    [[nodiscard]] std::optional<Utf8Scalar> decode_utf8_scalar(std::string_view text, std::size_t offset) noexcept;

    // Decode the complete view into owned records, including embedded NUL and BOM.
    // No normalization, segmentation or source retention. Allocation failure propagates.
    [[nodiscard]] std::vector<Utf8Scalar> decode_utf8(std::string_view text);
}
