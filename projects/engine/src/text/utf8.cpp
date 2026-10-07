#include <text/utf8.h>

#include <cstdint>

namespace CE::Text {
    std::optional<Utf8Scalar> decode_utf8_scalar(const std::string_view text, const std::size_t offset) noexcept {
        if (offset >= text.size())
            return std::nullopt;
        const auto remaining = text.size() - offset;
        const auto byte = [&](const std::size_t index) { return static_cast<unsigned char>(text[offset + index]); };
        const auto first = byte(0);
        if (first <= 0x7f)
            return Utf8Scalar{static_cast<char32_t>(first), offset, 1, false};

        std::size_t width;
        std::uint32_t value;
        unsigned char second_minimum = 0x80;
        unsigned char second_maximum = 0xbf;
        if (first >= 0xc2 && first <= 0xdf) {
            width = 2;
            value = static_cast<std::uint32_t>(first) & 0x1f;
        } else if (first >= 0xe0 && first <= 0xef) {
            width = 3;
            value = static_cast<std::uint32_t>(first) & 0x0f;
            if (first == 0xe0)
                second_minimum = 0xa0;
            else if (first == 0xed)
                second_maximum = 0x9f;
        } else if (first >= 0xf0 && first <= 0xf4) {
            width = 4;
            value = static_cast<std::uint32_t>(first) & 0x07;
            if (first == 0xf0)
                second_minimum = 0x90;
            else if (first == 0xf4)
                second_maximum = 0x8f;
        } else {
            return Utf8Scalar{U'\ufffd', offset, 1, true};
        }

        // Keep only the prefix that could start a well-formed sequence. A rejected
        // successor belongs to the next decode, including any following ASCII byte.
        std::size_t consumed = 1;
        while (consumed < width) {
            if (consumed >= remaining)
                return Utf8Scalar{U'\ufffd', offset, consumed, true};
            const auto next = byte(consumed);
            const auto minimum = consumed == 1 ? second_minimum : 0x80;
            const auto maximum = consumed == 1 ? second_maximum : 0xbf;
            if (next < minimum || next > maximum)
                return Utf8Scalar{U'\ufffd', offset, consumed, true};
            value = (value << 6) | static_cast<std::uint32_t>(next & 0x3f);
            ++consumed;
        }
        return Utf8Scalar{static_cast<char32_t>(value), offset, consumed, false};
    }

    std::vector<Utf8Scalar> decode_utf8(const std::string_view text) {
        std::vector<Utf8Scalar> result;
        std::size_t offset = 0;
        while (const auto scalar = decode_utf8_scalar(text, offset)) {
            result.push_back(*scalar);
            offset += scalar->byte_count;
        }
        return result;
    }
}
