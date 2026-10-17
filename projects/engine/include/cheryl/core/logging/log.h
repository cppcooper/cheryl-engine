#pragma once

#include <cheryl/export.h>
#include "compile-policy.h"
#include "config.h"
#include "backend-guard.h"
#include "log-names.h"
#include <internals/failure-reporting.h>

#include <chrono>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace CE::Diagnostics {
    [[nodiscard]] CE_API std::string stack_trace(void* address = nullptr) noexcept;
}

namespace CE::Logging::Detail {
    struct LogState;
    [[nodiscard]] CE_API std::shared_ptr<LogState>
    acquire_category(const char* name, FileEventHandlers handlers, Config config, bool initialize);
}

namespace CE::Logging {
    class LogHandle;
}

namespace CE::Logging::Detail {
    [[nodiscard]] CE_API LogHandle* existing_category(const char* name) noexcept;
}

namespace CE::Logging {
    template <const char* name> class Logger;

    /** An owned logger facade. Engine holds its queue, destinations, registry and
     * native implementation. Formatting occurs only after the destination probe.
     */
    class LogHandle {
        std::shared_ptr<Detail::LogState> state_;

        CE_API void submit(Level level, std::string_view message, bool trace) const;

        template <Compile::LogLevel severity, typename... Args> void write(std::format_string<Args...> format, Args&&... arguments) const {
            if constexpr (Compile::enabled(severity)) {
                constexpr auto level = Compile::runtime_level(severity);
                if (should_log(level))
                    submit(level, std::format(format, std::forward<Args>(arguments)...), severity == Compile::FATAL_);
            }
        }

    protected:
        CE_API LogHandle(const char* name, FileEventHandlers handlers, Config config);
        CE_API explicit LogHandle(std::shared_ptr<Detail::LogState> state);

    public:
        CE_API ~LogHandle();
        LogHandle(const LogHandle&) = delete;
        LogHandle& operator=(const LogHandle&) = delete;
        LogHandle(LogHandle&&) = delete;
        LogHandle& operator=(LogHandle&&) = delete;

        /** Immutable startup settings with an absolute directory. Live level
         * changes affect the current generation and survive close/reopen.
         */
        [[nodiscard]] CE_API const Config& initial_configuration() const noexcept;
        /** Independently sampled counts across every category using this pool,
         * including discarded flush requests. Available while closed and not reset
         * by reopen; queued work excludes items already taken by the worker.
         */
        [[nodiscard]] CE_API QueueStats shared_queue_stats() const;
        /** Cumulative failures across generations; degradation describes the last
         * published generation and clears after completed close/reopen.
         */
        [[nodiscard]] CE_API BackendStats backend_stats() const noexcept;
        /** Coalesced bounded stdio reports outside lifecycle locks; available
         * while closed and also emitted by completed close.
         */
        CE_API void report_diagnostics() const;
        [[nodiscard]] CE_API std::filesystem::path get_file_path() const;
        [[nodiscard]] CE_API std::uint16_t get_log_id() const;
        /** Queue a flush using this logger's overflow policy. This may wait for
         * capacity or discard the request; it does not acknowledge backend completion.
         */
        CE_API void flush() const;
        /** Stop producers first. Completed close drains accepted file ownership,
         * without promising physical durability. A zero timeout waits indefinitely;
         * timeout leaves Closing intact for a later close. File callbacks cannot
         * reenter lifecycle operations or wait for the logging worker.
         */
        CE_API void close(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());
        CE_API void reopen();
        /** Select this destination in Engine's private logging registry. This
         * does not replace an application's independently configured native logger.
         */
        CE_API void make_default() const;
        CE_API void set_level_logger(Level level) const;
        CE_API void set_level_filesink(Level level) const;
        CE_API void set_level_stdsink(Level level) const;
        /** Best-effort probe of compiled severities, logger and destination gates.
         * Probe before evaluating expensive arguments; a level change or close can
         * race a later write. Writes while closing/closed are suppressed.
         */
        [[nodiscard]] CE_API bool should_log(Level level) const noexcept;
        CE_API void set_pattern(const char* format) const;
        CE_API void set_pattern_filesink(const char* format) const;
        CE_API void set_pattern_stdsink(const char* format) const;
        CE_API void strace(void* address = nullptr) const;
        template <typename... Args> void trace(std::format_string<Args...> format, Args&&... arguments) const {
            write<Compile::TRACE_>(format, std::forward<Args>(arguments)...);
        }

        template <typename... Args> void debug(std::format_string<Args...> format, Args&&... arguments) const {
            write<Compile::DEBUG_>(format, std::forward<Args>(arguments)...);
        }

        template <typename... Args> void info(std::format_string<Args...> format, Args&&... arguments) const {
            write<Compile::INFO_>(format, std::forward<Args>(arguments)...);
        }

        template <typename... Args> void warn(std::format_string<Args...> format, Args&&... arguments) const {
            write<Compile::WARNING_>(format, std::forward<Args>(arguments)...);
        }

        template <typename... Args> void error(std::format_string<Args...> format, Args&&... arguments) const {
            write<Compile::ERROR_>(format, std::forward<Args>(arguments)...);
        }

        template <typename... Args> void critical(std::format_string<Args...> format, Args&&... arguments) const {
            write<Compile::FATAL_>(format, std::forward<Args>(arguments)...);
        }
    };

    template <const char* name> class Log : public LogHandle {
        friend class Logger<name>;
        explicit Log(std::shared_ptr<Detail::LogState> state)
        : LogHandle(std::move(state)) {}

    public:
        explicit Log(FileEventHandlers handlers = {}, Config configuration = Config::for_logger(name))
        : LogHandle(name, std::move(handlers), std::move(configuration)) {}
    };
}
