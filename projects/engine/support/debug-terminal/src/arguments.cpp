#include "arguments.h"

#include <cstdlib>
#include <stdexcept>
#include <string_view>

namespace CE::DebugTerminal::Detail {
    Arguments parse_arguments(const int argc, char** argv, const bool automatic) {
        if (argc <= 0 || !argv || !argv[0])
            throw std::invalid_argument("Terminal startup requires an executable and argument array");
        Arguments result{.enabled = automatic};
        result.values.reserve(static_cast<std::size_t>(argc) + 1);
        result.values.push_back(argv[0]);
        bool application_arguments = false;
        bool listing = false;
        for (int index = 1; index < argc; ++index) {
            if (!argv[index])
                throw std::invalid_argument("Terminal arguments must not contain null strings");
            const std::string_view value(argv[index]);
            if (!application_arguments && value == "--debug-terminal") {
                result.enabled = true;
            } else if (!application_arguments && value == "--no-debug-terminal") {
                result.enabled = false;
            } else {
                result.values.push_back(argv[index]);
                if (!application_arguments) {
                    if (value == "--gtest_list_tests")
                        listing = true;
                    else if (value.starts_with("--gtest_list_tests=")) {
                        const auto option = value.substr(19);
                        listing = option.empty() || (option.front() != '0' && option.front() != 'f' && option.front() != 'F');
                    }
                    result.help |= value == "--help" || value == "-h" || value == "--help-all" || value == "--gtest_help";
                    result.discovery |= result.help || value.starts_with("--gtest_internal_run_death_test=");
                    application_arguments = value == "--";
                }
            }
        }
        result.values.push_back(nullptr);
        result.discovery |= listing;
        result.enabled &= !result.discovery;
        return result;
    }

    bool discovery_environment() noexcept {
        const char* listing = std::getenv("GTEST_LIST_TESTS");
        return listing && *listing && std::string_view(listing) != "0";
    }
}
