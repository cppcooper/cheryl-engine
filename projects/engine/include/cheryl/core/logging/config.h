#pragma once
#include "compile-policy.h"
#include "backend-guard.h"
#include <internals/exceptions.h>

#include <cstddef>
#include <filesystem>
#include <string_view>

namespace CE::Logging {
    enum class Profile { Developer, Support, Release };
    /** Per-Log behavior at shared queue capacity, for records and flush requests.
     * Block waits for room; DiscardNew counts/rejects the new item without evicting
     * accepted work. Neither acknowledges backend completion or durability.
     */
    enum class OverflowPolicy { Block, DiscardNew };

    /** Independently sampled values from the shared Cheryl pool, including flush
     * requests. Capacity discards span every Log using that pool and are not reset
     * by close/reopen. Queued items exclude work already taken by the worker.
     */
    struct QueueStats {
        std::size_t queued_items = 0;
        std::size_t discarded_items = 0;
    };

    /** Initial destination and per-Log queue configuration. Levels may change
     * while open; destination/rotation/overflow changes require a new owned Log
     * or the singleton's first initialization. Off suppresses output but does
     * not avoid opening the file. Configure before starting producers.
     */
    struct Config {
        std::filesystem::path directory{"logs"};
        std::size_t rotation_bytes = 10 * 1024 * 1024;
        std::size_t retained_files = 5;
        bool rotate_on_open = true;
        spdlog::level logger_level = ::CE::Logging::Compile::profile == 2 ? spdlog::level::info : spdlog::level::debug;
        spdlog::level file_level = ::CE::Logging::Compile::profile == 2 ? spdlog::level::info : spdlog::level::debug;
        spdlog::level console_level = ::CE::Logging::Compile::profile == 0 ? spdlog::level::info : spdlog::level::warn;
        ::CE::Logging::OverflowPolicy overflow_policy = ::CE::Logging::OverflowPolicy::Block;

        [[nodiscard]] static Config for_logger(
            const char* name,
            const ::CE::Logging::Profile profile = static_cast<::CE::Logging::Profile>(::CE::Logging::Compile::profile)
        ) {
            Config result;
            switch (profile) {
                case ::CE::Logging::Profile::Developer:
                    result.logger_level = result.file_level = spdlog::level::debug;
                    result.console_level = spdlog::level::info;
                    break;
                case ::CE::Logging::Profile::Support:
                    result.logger_level = result.file_level = spdlog::level::debug;
                    result.console_level = spdlog::level::warn;
                    break;
                case ::CE::Logging::Profile::Release:
                    result.logger_level = result.file_level = spdlog::level::info;
                    result.console_level = spdlog::level::warn;
                    break;
                default:
                    throw ::CE::Exceptions::invalid_args(CE_HERE, "Unknown logging profile");
            }
            if (name && std::string_view{name} == "memory")
                result.logger_level = result.file_level =
                    profile == ::CE::Logging::Profile::Release ? spdlog::level::warn : spdlog::level::info;
            return result;
        }
    };
}

namespace CE::Logging::Detail {
    [[nodiscard]] inline ::CE::Logging::Config prepare_configuration(::CE::Logging::Config config, const char* name) {
        reject_backend_reentry("initialize a logger");
        const std::string_view logger_name = name ? name : "";
        if (logger_name.empty() || logger_name.find_first_of("/\\") != std::string_view::npos)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "A logger name must be a nonempty filename component");
        const auto valid_level = [](const spdlog::level level) { return level >= spdlog::level::trace && level <= spdlog::level::off; };
        if (!valid_level(config.logger_level) || !valid_level(config.file_level) || !valid_level(config.console_level))
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Unknown runtime logging level");
        if (config.rotation_bytes == 0 || config.retained_files > 200000)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Invalid rotating log limits");
        if (config.overflow_policy != ::CE::Logging::OverflowPolicy::Block &&
            config.overflow_policy != ::CE::Logging::OverflowPolicy::DiscardNew)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Unknown logging overflow policy");
        // Resolve relative paths once, before any file/registry side effects.
        // Reopening must not follow an unrelated change of working directory.
        config.directory = std::filesystem::absolute(config.directory.empty() ? std::filesystem::path{"."} : config.directory)
                               .lexically_normal();
        return config;
    }
}
