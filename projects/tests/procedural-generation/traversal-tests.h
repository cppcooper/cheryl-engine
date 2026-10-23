#pragma once

#include <cstddef>
#include <string_view>

namespace ProcGenTests {
    struct Counts {
        std::size_t passed = 0;
        std::size_t failed = 0;
        std::size_t assertions = 0;
    };

    [[nodiscard]] Counts run_traversal_cases(std::string_view selection);
    [[nodiscard]] Counts run_configuration_cases(std::string_view selection);
}
