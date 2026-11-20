#pragma once

#include <cheryl/tools/console/registry.h>
#include <optional>

namespace CE::Tools::Console {
    struct Token {
        std::string text;
        std::size_t offset = 0, length = 0;
        bool complete = true;
        std::optional<std::size_t> equals;
    };
    struct Parsed {
        std::vector<Token> tokens;
        bool trailing_space = false;
    };
    Checked<Parsed> parse(std::string_view line, bool partial = false);
    Checked<BoundRequest> bind_text(const Registry&, const Context&, std::string_view line);
    Checked<std::vector<std::string>> complete(const Registry&, const Context&, std::string_view line);
}
