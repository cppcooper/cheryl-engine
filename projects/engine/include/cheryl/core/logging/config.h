#pragma once
#include "compile-policy.h"
#include "backend-guard.h"
#include <internals/exceptions.h>

#include <cstddef>
#include <filesystem>
#include <string_view>

namespace CE::Logging {
    /** Runtime threshold defaults, independent of which severities were compiled. */
    enum class Profile { Developer, ///< Debug file/logger and info console; memory uses info.
        Support, ///< Debug file/logger and warn console; memory uses info.
        Release ///< Info file/logger and warn console; memory uses warn.
    };
    /** Per-Log behavior at shared queue capacity, for records and flush requests.
     * Block waits for room; DiscardNew counts/rejects the new item without evicting
     * accepted work. Neither acknowledges backend completion or durability.
     */
    enum class OverflowPolicy { Block, ///< Wait for shared queue capacity; does not wait for backend completion.
        DiscardNew ///< Discard/count the new item without evicting accepted records or flush requests.
    };

    /** Independently sampled values from the shared Cheryl pool, including flush
     * requests. Capacity discards span every Log using that pool and are not reset
     * by close/reopen. Queued items exclude work already taken by the worker.
     */
    struct QueueStats {
        std::size_t queued_items = 0; ///< Items still queued in the shared pool, excluding items already taken by its worker.
        std::size_t discarded_items = 0; ///< Cumulative shared-pool capacity discards, including flush requests across every logger.
    };

    /** Initial destination and per-Log queue configuration. Levels may change
     * while open; destination/rotation/overflow changes require a new owned Log
     * or the singleton's first initialization. Off suppresses output but does
     * not avoid opening the file. Configure before starting producers.
     */
    struct Config {
        std::filesystem::path directory{"logs"}; ///< Destination directory; normalized to absolute during construction, empty resolves to the current directory.
        std::size_t rotation_bytes = 10 * 1024 * 1024; ///< Positive file rotation size in bytes.
        std::size_t retained_files = 5; ///< Number of rotated files retained; at most 200000.
        bool rotate_on_open = true; ///< Whether opening rotates existing destination content.
        ::CE::Logging::Level logger_level = ::CE::Logging::Compile::profile == 2 ? ::CE::Logging::Level::info : ::CE::Logging::Level::debug; ///< Initial category gate; runtime changes do not alter this stored startup snapshot.
        ::CE::Logging::Level file_level = ::CE::Logging::Compile::profile == 2 ? ::CE::Logging::Level::info : ::CE::Logging::Level::debug; ///< Initial file gate, additionally subject to logger gate and compiled mask.
        ::CE::Logging::Level console_level = ::CE::Logging::Compile::profile == 0 ? ::CE::Logging::Level::info : ::CE::Logging::Level::warn; ///< Initial console gate, additionally subject to logger gate and compiled mask.
        ::CE::Logging::OverflowPolicy overflow_policy = ::CE::Logging::OverflowPolicy::Block; ///< Per-Log policy for shared queue saturation; neither mode acknowledges durable output.

        [[nodiscard]] static Config for_logger(
            const char* name,
            const ::CE::Logging::Profile profile = static_cast<::CE::Logging::Profile>(::CE::Logging::Compile::profile)
        ) {
            Config result;
            switch (profile) {
                case ::CE::Logging::Profile::Developer:
                    result.logger_level = result.file_level = ::CE::Logging::Level::debug;
                    result.console_level = ::CE::Logging::Level::info;
                    break;
                case ::CE::Logging::Profile::Support:
                    result.logger_level = result.file_level = ::CE::Logging::Level::debug;
                    result.console_level = ::CE::Logging::Level::warn;
                    break;
                case ::CE::Logging::Profile::Release:
                    result.logger_level = result.file_level = ::CE::Logging::Level::info;
                    result.console_level = ::CE::Logging::Level::warn;
                    break;
                default:
                    throw ::CE::Exceptions::invalid_args(CE_HERE, "Unknown logging profile");
            }
            if (name && std::string_view{name} == "memory")
                result.logger_level = result.file_level =
                    profile == ::CE::Logging::Profile::Release ? ::CE::Logging::Level::warn : ::CE::Logging::Level::info;
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
        const auto valid_level = [](const ::CE::Logging::Level level) {
            return level >= ::CE::Logging::Level::trace && level <= ::CE::Logging::Level::off;
        };
        if (!valid_level(config.logger_level) || !valid_level(config.file_level) || !valid_level(config.console_level))
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Unknown runtime logging level");
        if (config.rotation_bytes == 0 || config.retained_files > 200000)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Invalid rotating log limits");
        if (config.overflow_policy != ::CE::Logging::OverflowPolicy::Block &&
            config.overflow_policy != ::CE::Logging::OverflowPolicy::DiscardNew)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Unknown logging overflow policy");
        // Resolve relative paths once, before any file/registry side effects.
        // Reopening must not follow an unrelated change of working directory.
        config.directory =
            std::filesystem::absolute(config.directory.empty() ? std::filesystem::path{"."} : config.directory).lexically_normal();
        return config;
    }
}

/**
 * @fn CE::Logging::Config CE::Logging::Config::for_logger(const char* name, CE::Logging::Profile profile)
 * @brief Build profile defaults, including the quieter memory category.
 * @param name Borrowed category name used only to select defaults; nullptr is permitted here, but logger construction still requires a valid filename component.
 * @param profile Runtime default profile; cannot restore severities excluded at compile time.
 * @return Owned initial settings; creates no directory, file or logger.
 * @throws CE::Exceptions::invalid_args For an unknown profile.
 * Developer/Support use debug file/logger gates and info/warn console respectively; Release uses info file/logger and warn console. The memory category uses info, or warn in Release.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */
