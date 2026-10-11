#pragma once

#include <vector>

namespace CE::DebugTerminal::Detail {
    struct Arguments {
        bool enabled = false;
        bool discovery = false;
        bool help = false;
        std::vector<char*> values;
    };

    [[nodiscard]] Arguments parse_arguments(int argc, char** argv, bool automatic);
    [[nodiscard]] bool discovery_environment() noexcept;
}
