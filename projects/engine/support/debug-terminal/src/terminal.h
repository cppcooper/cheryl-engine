#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace CE::TerminalDetail {
    inline constexpr char ready = 'R';
    inline constexpr char started = 'S';
    inline constexpr char normal_exit = 'N';

    struct Arguments {
        bool enabled = false;
        bool discovery = false;
        std::vector<char*> values;
    };

    [[nodiscard]] Arguments parse_arguments(int argc, char** argv, bool automatic);
    [[nodiscard]] std::vector<std::string> viewer_arguments(
        const std::string& emulator,
        const std::filesystem::path& viewer,
        const std::filesystem::path& socket,
        const std::filesystem::path& output,
        unsigned long owner
    );
    [[nodiscard]] bool desktop_available() noexcept;
    [[nodiscard]] bool discovery_environment() noexcept;
}
