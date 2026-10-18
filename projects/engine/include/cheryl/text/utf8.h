#pragma once

#include <cheryl/export.h>

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace CE::Text {
    /** One owned Unicode scalar with its original UTF-8 byte range. Logical source
     * order is distinct from grapheme, visual and shaping-cluster order. No source is retained.
     * @see [Encoding and source indices](cheryl-guide:assets/text-encoding)
     */
    struct Utf8Scalar {
        char32_t value{}; ///< Decoded scalar, or U+FFFD for a malformed maximal subpart.
        std::size_t byte_offset{}; ///< Zero-based offset in the original byte view.
        std::size_t byte_count{}; ///< Bytes consumed for this scalar; positive for every decoded record.
        bool replaced{}; ///< True for malformed input, false for an explicitly encoded U+FFFD.

        [[nodiscard]] bool operator==(const Utf8Scalar&) const = default;
    };

    // Decode at offset, or return no value at/beyond the end. Malformed input emits
    // U+FFFD for one maximal subpart; byte_count always advances within the view.
    // Truncated prefixes are errors at this view's end; this is not a streaming API.
    // The caller keeps input stable for the call and advances by returned byte_count.
    [[nodiscard]] CE_API std::optional<Utf8Scalar> decode_utf8_scalar(std::string_view text, std::size_t offset) noexcept;

    // Decode the complete view into owned records, including embedded NUL and BOM.
    // No normalization, segmentation or source retention. Allocation failure propagates.
    [[nodiscard]] CE_API std::vector<Utf8Scalar> decode_utf8(std::string_view text);
}

/** @fn bool CE::Text::Utf8Scalar::operator==(const Utf8Scalar&) const
 * Compare scalar, source byte range and malformed-input flag by value.
 */

/** @fn std::optional<CE::Text::Utf8Scalar> CE::Text::decode_utf8_scalar(std::string_view text, std::size_t offset) noexcept
 * Decode at a byte offset; return nullopt at or beyond the view end.
 * Malformed input emits U+FFFD for one maximal subpart. Truncated prefixes are errors at this view's end; this is not a streaming decoder. Retain stable input only for the call and advance by byte_count.
 * @param text Borrowed bytes; embedded NUL and BOM are decoded as scalars.
 * @param offset Zero-based byte offset, not a scalar index.
 * @return Owned scalar/source record with a positive byte_count within the view, or no result.
 * @see [UTF-8 decoding](cheryl-guide:assets/text-encoding)
 */

/** @fn std::vector<CE::Text::Utf8Scalar> CE::Text::decode_utf8(std::string_view text)
 * Decode the complete view into owned logical-order records, including NUL/BOM. No normalization, segmentation or source retention occurs. Malformed maximal subparts become marked replacements; allocation failure propagates.
 * @param text Bytes borrowed only during this call.
 * @return An empty vector for an empty view; otherwise independent scalar/source records.
 */
