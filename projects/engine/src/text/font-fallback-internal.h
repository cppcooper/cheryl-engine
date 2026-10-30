#pragma once

#include <span>

namespace CE::Text::Detail {
    [[nodiscard]] std::span<const unsigned char> builtin_font_bytes() noexcept;
}
