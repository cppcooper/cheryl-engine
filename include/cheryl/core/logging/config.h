#pragma once
#include "compile-policy.h"
#include "backend-guard.h"
#include <internals/exceptions.h>

#include <cstddef>
#include <filesystem>
#include <string_view>

namespace CE {
    enum class LogProfile { Developer, Support, Release };
    /** Per-Log behavior at shared queue capacity, for records and flush requests.
     * Block waits for room; DiscardNew counts/rejects the new item without evicting
     * accepted work. Neither acknowledges backend completion or durability.
     */
    enum class LogOverflowPolicy { Block, DiscardNew };

    /** Independently sampled values from the shared Cheryl pool, including flush
     * requests. Capacity discards span every Log using that pool and are not reset
     * by close/reopen. Queued items exclude work already taken by the worker.
     */
    struct LogQueueStats {
        std::size_t queued_items = 0;
        std::size_t discarded_items = 0;
    };

    /** Initial destination and per-Log queue configuration. Levels may change
     * while open; destination/rotation/overflow changes require a new owned Log
     * or the singleton's first initialization. Off suppresses output but does
     * not avoid opening the file. Configure before starting producers.
     */
    struct LogConfig {
        std::filesystem::path directory{"logs"};
        std::size_t rotation_bytes = 10 * 1024 * 1024;
        std::size_t retained_files = 5;
        bool rotate_on_open = true;
        spdlog::level logger_level = ctlog::profile == 2 ? spdlog::level::info : spdlog::level::debug;
        spdlog::level file_level = ctlog::profile == 2 ? spdlog::level::info : spdlog::level::debug;
        spdlog::level console_level = ctlog::profile == 0 ? spdlog::level::info : spdlog::level::warn;
        LogOverflowPolicy overflow_policy = LogOverflowPolicy::Block;

        [[nodiscard]] static LogConfig for_logger(
            const char* name, const LogProfile profile = static_cast<LogProfile>(ctlog::profile)
        ) {
            LogConfig result;
            switch (profile) {
                case LogProfile::Developer:
                    result.logger_level = result.file_level = spdlog::level::debug;
                    result.console_level = spdlog::level::info;
                    break;
                case LogProfile::Support:
                    result.logger_level = result.file_level = spdlog::level::debug;
                    result.console_level = spdlog::level::warn;
                    break;
                case LogProfile::Release:
                    result.logger_level = result.file_level = spdlog::level::info;
                    result.console_level = spdlog::level::warn;
                    break;
                default:
                    throw Exceptions::invalid_args(CE_HERE, "Unknown logging profile");
            }
            if (name && std::string_view{name} == "memory")
                result.logger_level = result.file_level = profile == LogProfile::Release ? spdlog::level::warn : spdlog::level::info;
            return result;
        }
    };
}

namespace CE::LogDetail {
    [[nodiscard]] inline LogConfig prepare_configuration(LogConfig config, const char* name) {
        reject_backend_reentry("initialize a logger");
        const std::string_view logger_name = name ? name : "";
        if (logger_name.empty() || logger_name.find_first_of("/\\") != std::string_view::npos)
            throw Exceptions::invalid_args(CE_HERE, "A logger name must be a nonempty filename component");
        const auto valid_level = [](const spdlog::level level) { return level >= spdlog::level::trace && level <= spdlog::level::off; };
        if (!valid_level(config.logger_level) || !valid_level(config.file_level) || !valid_level(config.console_level))
            throw Exceptions::invalid_args(CE_HERE, "Unknown runtime logging level");
        if (config.rotation_bytes == 0 || config.retained_files > 200000)
            throw Exceptions::invalid_args(CE_HERE, "Invalid rotating log limits");
        if (config.overflow_policy != LogOverflowPolicy::Block && config.overflow_policy != LogOverflowPolicy::DiscardNew)
            throw Exceptions::invalid_args(CE_HERE, "Unknown logging overflow policy");
        // Resolve relative paths once, before any file/registry side effects.
        // Reopening must not follow an unrelated change of working directory.
        config.directory = std::filesystem::absolute(config.directory.empty() ? std::filesystem::path{"."} : config.directory)
                               .lexically_normal();
        return config;
    }
}
