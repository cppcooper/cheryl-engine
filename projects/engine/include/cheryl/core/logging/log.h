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

        [[nodiscard]] CE_API const Config& initial_configuration() const noexcept;
        [[nodiscard]] CE_API QueueStats shared_queue_stats() const;
        [[nodiscard]] CE_API BackendStats backend_stats() const noexcept;
        CE_API void report_diagnostics() const;
        [[nodiscard]] CE_API std::filesystem::path get_file_path() const;
        [[nodiscard]] CE_API std::uint16_t get_log_id() const;
        CE_API void flush() const;
        CE_API void close(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());
        CE_API void reopen();
        CE_API void make_default() const;
        CE_API void set_level_logger(Level level) const;
        CE_API void set_level_filesink(Level level) const;
        CE_API void set_level_stdsink(Level level) const;
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

/** Owned named logger with the LogHandle lifecycle and operation contracts.
     * @tparam name Category filename component with static character storage; normal construction owns independent backend state.
     * Keep an application ProcessLifetime until every caller and final closure finishes.
     */
    template <const char* name> class Log : public LogHandle {
        friend class Logger<name>;
        explicit Log(std::shared_ptr<Detail::LogState> state)
        : LogHandle(std::move(state)) {}

    public:
        explicit Log(FileEventHandlers handlers = {}, Config configuration = Config::for_logger(name))
        : LogHandle(name, std::move(handlers), std::move(configuration)) {}
    };
}

/**
 * @fn CE::Logging::LogHandle::LogHandle(const char* name, FileEventHandlers handlers, Config config)
 * @brief Construct owned backend state for a valid category and initial configuration.
 * @param name Nonempty filename component with no slash/backslash.
 * @param handlers Owned callbacks with protected capture lifetime through closure.
 * @param config Valid rotation/level/overflow settings; relative directory resolves once before publication.
 * Opening/validation/callback failures propagate. Requires a live process logging lifetime; no backend callback may reenter construction.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn CE::Logging::LogHandle::LogHandle(std::shared_ptr<Detail::LogState> state)
 * @brief Retain already constructed Engine logging state for an internal derived facade.
 * @param state Valid non-null backend state; this protected path does not construct another native logger.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn CE::Logging::LogHandle::~LogHandle()
 * @brief Release facade ownership; final backend ownership closes/drains its accepted output.
 * Stop callers before destruction. Retained queue/backend state and process logging lifetime govern actual native cleanup.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn const CE::Logging::Config& CE::Logging::LogHandle::initial_configuration() const noexcept
 * @brief Borrow immutable startup settings with the normalized absolute directory.
 * @return Reference valid during facade/backend lifetime. Live level changes affect current generations and survive close/reopen, but do not mutate this snapshot.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn CE::Logging::QueueStats CE::Logging::LogHandle::shared_queue_stats() const
 * @brief Sample independently maintained shared logging-pool counters.
 * @return Owned counts across all categories, including discarded flush requests and excluding worker-taken items. Available while closed; reopen does not reset totals.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn CE::Logging::BackendStats CE::Logging::LogHandle::backend_stats() const noexcept
 * @brief Sample cumulative failures and the last published generation's degradation.
 * @return Owned independently sampled counters; completed close/reopen clears degradation, not cumulative history.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn void CE::Logging::LogHandle::report_diagnostics() const
 * @brief Emit bounded coalesced stdio observations outside lifecycle locks.
 * Available while closed and emitted by completed close. This observes failures without redirecting the original exception or proving output durability.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn std::filesystem::path CE::Logging::LogHandle::get_file_path() const
 * @brief Copy the configured absolute log-file path; does not reopen or refresh a closed destination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn std::uint16_t CE::Logging::LogHandle::get_log_id() const
 * @brief Read the backend's process-local compact log identifier; not a persistent globally unique key.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn void CE::Logging::LogHandle::flush() const
 * @brief Enqueue a flush using this log's overflow policy.
 * May wait for shared queue capacity or discard/count the request. It does not acknowledge backend completion or physical durability; backend/lifecycle reentry rejects.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn void CE::Logging::LogHandle::close(std::chrono::milliseconds timeout)
 * @brief Stop accepting writes and wait for accepted file ownership to close.
 * @param timeout Zero waits indefinitely; a finite timeout bounds this wait.
 * Stop producers first. Timeout throws failed_operation and leaves Closing intact for a later close; completed close does not promise physical durability. Backend callback reentry or invalid lifecycle transitions reject.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn void CE::Logging::LogHandle::reopen()
 * @brief Reopen a completed Closed log using retained settings and live level configuration.
 * An already Open log is unchanged; Opening/Closing rejects. Native/open-callback errors propagate without publishing a completed generation. Process logging shutdown is terminal and forbids reopen.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn void CE::Logging::LogHandle::make_default() const
 * @brief Select this open logger in Engine's private native registry.
 * Does not replace an application's independently configured native default logger. Closed/transitional state or backend reentry rejects.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn bool CE::Logging::LogHandle::should_log(Level level) const noexcept
 * @brief Probe compilation, logger and healthy destination gates before expensive argument preparation.
 * @param level Runtime severity to inspect.
 * @return Best-effort admission at this instant; false while closing/closed or recursively inside backend activity.
 * A level change/close can race a later write. Ordinary call arguments are evaluated before entering a severity method; use lazy submission to avoid that preparation.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn void CE::Logging::LogHandle::strace(void* address) const
 * @brief Submit a stack trace through the configured trace gate.
 * @param address Optional stack-trace origin; nullptr requests the current trace.
 * Compilation/destination gates can suppress it; it is ordinary logging, not an emergency/noexcept callback facility.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn CE::Logging::LogHandle::LogHandle(const LogHandle&)
 * @brief Copy/move facade ownership is disabled; use a named Logger or explicitly owned Log instead.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn LogHandle& CE::Logging::LogHandle::operator=(const LogHandle&)
 * @brief Copy/move facade ownership is disabled; use a named Logger or explicitly owned Log instead.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn CE::Logging::LogHandle::LogHandle(LogHandle&&)
 * @brief Copy/move facade ownership is disabled; use a named Logger or explicitly owned Log instead.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn LogHandle& CE::Logging::LogHandle::operator=(LogHandle&&)
 * @brief Copy/move facade ownership is disabled; use a named Logger or explicitly owned Log instead.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn void CE::Logging::LogHandle::set_level_logger(Level level) const
 * @brief Change the live category gate on an open log.
 * @param level Valid runtime threshold; off suppresses output without closing destinations.
 * The threshold survives close/reopen but cannot enable compiled-out severities. Closed/transitional state and backend reentry reject.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn void CE::Logging::LogHandle::set_level_filesink(Level level) const
 * @brief Change the live file destination gate on an open log.
 * @param level Valid runtime threshold; off suppresses output without closing destinations.
 * The threshold survives close/reopen but cannot enable compiled-out severities. Closed/transitional state and backend reentry reject.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn void CE::Logging::LogHandle::set_level_stdsink(Level level) const
 * @brief Change the live console destination gate on an open log.
 * @param level Valid runtime threshold; off suppresses output without closing destinations.
 * The threshold survives close/reopen but cannot enable compiled-out severities. Closed/transitional state and backend reentry reject.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn void CE::Logging::LogHandle::set_pattern(const char* format) const
 * @brief Set formatting for both destinations on the open log.
 * @param format Borrowed valid native pattern string consumed by the operation, not a std::format message.
 * Closed/transitional state or backend reentry rejects; destination pattern parsing errors propagate.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn void CE::Logging::LogHandle::set_pattern_filesink(const char* format) const
 * @brief Set formatting for the file destination on the open log.
 * @param format Borrowed valid native pattern string consumed by the operation, not a std::format message.
 * Closed/transitional state or backend reentry rejects; destination pattern parsing errors propagate.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn void CE::Logging::LogHandle::set_pattern_stdsink(const char* format) const
 * @brief Set formatting for the console destination on the open log.
 * @param format Borrowed valid native pattern string consumed by the operation, not a std::format message.
 * Closed/transitional state or backend reentry rejects; destination pattern parsing errors propagate.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<typename... Args> void CE::Logging::LogHandle::trace(std::format_string<Args...> format, Args&&... arguments) const
 * @brief Format and submit an ordinary trace record only when compiled and admitted.
 * @tparam Args Types accepted by the checked std::format string.
 * @param format Compile-time checked message format.
 * @param arguments Forwarded formatting inputs; expressions are evaluated before this call.
 * Formatting/submission can allocate or throw and Block overflow can wait. Closing/closed/degraded destinations suppress output. Do not call from locks, native/noexcept callbacks or the logging backend; lazy diagnostics handle preparation failures separately.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<typename... Args> void CE::Logging::LogHandle::debug(std::format_string<Args...> format, Args&&... arguments) const
 * @brief Format and submit an ordinary debug record only when compiled and admitted.
 * @tparam Args Types accepted by the checked std::format string.
 * @param format Compile-time checked message format.
 * @param arguments Forwarded formatting inputs; expressions are evaluated before this call.
 * Formatting/submission can allocate or throw and Block overflow can wait. Closing/closed/degraded destinations suppress output. Do not call from locks, native/noexcept callbacks or the logging backend; lazy diagnostics handle preparation failures separately.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<typename... Args> void CE::Logging::LogHandle::info(std::format_string<Args...> format, Args&&... arguments) const
 * @brief Format and submit an ordinary info record only when compiled and admitted.
 * @tparam Args Types accepted by the checked std::format string.
 * @param format Compile-time checked message format.
 * @param arguments Forwarded formatting inputs; expressions are evaluated before this call.
 * Formatting/submission can allocate or throw and Block overflow can wait. Closing/closed/degraded destinations suppress output. Do not call from locks, native/noexcept callbacks or the logging backend; lazy diagnostics handle preparation failures separately.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<typename... Args> void CE::Logging::LogHandle::warn(std::format_string<Args...> format, Args&&... arguments) const
 * @brief Format and submit an ordinary warning record only when compiled and admitted.
 * @tparam Args Types accepted by the checked std::format string.
 * @param format Compile-time checked message format.
 * @param arguments Forwarded formatting inputs; expressions are evaluated before this call.
 * Formatting/submission can allocate or throw and Block overflow can wait. Closing/closed/degraded destinations suppress output. Do not call from locks, native/noexcept callbacks or the logging backend; lazy diagnostics handle preparation failures separately.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<typename... Args> void CE::Logging::LogHandle::error(std::format_string<Args...> format, Args&&... arguments) const
 * @brief Format and submit an ordinary error record only when compiled and admitted.
 * @tparam Args Types accepted by the checked std::format string.
 * @param format Compile-time checked message format.
 * @param arguments Forwarded formatting inputs; expressions are evaluated before this call.
 * Formatting/submission can allocate or throw and Block overflow can wait. Closing/closed/degraded destinations suppress output. Do not call from locks, native/noexcept callbacks or the logging backend; lazy diagnostics handle preparation failures separately.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<typename... Args> void CE::Logging::LogHandle::critical(std::format_string<Args...> format, Args&&... arguments) const
 * @brief Format and submit an ordinary critical record only when compiled and admitted.
 * @tparam Args Types accepted by the checked std::format string.
 * @param format Compile-time checked message format.
 * @param arguments Forwarded formatting inputs; expressions are evaluated before this call.
 * Formatting/submission can allocate or throw and Block overflow can wait. Closing/closed/degraded destinations suppress output. Critical submission also includes its trace behavior. Do not call from locks, native/noexcept callbacks or the logging backend; lazy diagnostics handle preparation failures separately.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> CE::Logging::Log<name>::Log(FileEventHandlers handlers, Config configuration)
 * @brief Construct an independently owned named log using LogHandle's validated opening path.
 * @param handlers Owned file callbacks retained through final closure.
 * @param configuration Initial rotation, destination and runtime levels.
 * Unlike Logger<name>, another Log with the same template name is not a singleton access path.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/** @fn std::string CE::Diagnostics::stack_trace(void* address) noexcept
 * @brief Capture a best-effort owned stack trace without submitting ordinary logging.
 * @param address Optional origin; nullptr captures the current stack.
 * @return Owned trace text; failures are contained by the diagnostics implementation.
 * @see [Failure reporting](cheryl-guide:runtime/failure-reporting)
 */
