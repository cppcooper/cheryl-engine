#pragma once

#include <core/engine/worker-configuration.h>

#include <filesystem>
#include <istream>
#include <string_view>

namespace CE::Workers::Json {
    // Version 1.0 settings; no SDK types or discovery, threads, group creation or
    // affinity changes. Runtime availability is resolved by CE::Workers::Pool/make_group.
    [[nodiscard]] CE::Workers::Configuration parse_configuration(std::string_view document, const std::filesystem::path& source = {});
    [[nodiscard]] CE::Workers::Configuration load_configuration(std::istream& input, const std::filesystem::path& source = {});
    [[nodiscard]] CE::Workers::Configuration load_configuration(const std::filesystem::path& source);
}
