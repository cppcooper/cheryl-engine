#pragma once

#include "gate.h"

#include <expected>
#include <string>
#include <string_view>

namespace GateConsole {
    inline constexpr std::size_t maximum_save_bytes = 64 * 1024;

    std::string encode(const PairedState& state);
    std::expected<PairedState, std::string> decode(std::string_view bytes);
    // Decode/resume into fresh owned values; on rejection the live pair and all
    // previously issued selections retain their authority.
    std::expected<void, std::string> restore_bytes(Encounter& encounter, std::string_view bytes);
}
