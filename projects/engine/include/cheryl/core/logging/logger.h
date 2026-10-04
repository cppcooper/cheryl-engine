#pragma once
#include "log.h"
#include <templates/singleton.h>

#include <chrono>
#include <utility>

namespace CE {
    // Named access lazily creates Log<name>; writes never resolve the host's default
    // logger. Configure with initialize(handlers, config) on the logging owner
    // before any writer/lazy access;
    // repeated explicit initialization rejects. This compatibility constructor's
    // handlers/config apply only if it performs the first successful construction.
    template <const char* name> class Logger : public Singleton_CTS<Log<name>> {
    protected:
        explicit Logger(const spdlog::file_event_handlers& event_handlers = {}, LogConfig config = LogConfig::for_logger(name)) {
            get(event_handlers, std::move(config));
        }

    public:
        template <typename... Args> static Log<name>& get(Args&&... args) {
            LogDetail::reject_backend_reentry("acquire a logger");
            // Initialize the registry before singleton storage so it outlives
            // facade teardown, rather than first using it inside Log construction.
            // This establishes registry lifetime only; it does not select a write destination.
            (void)spdlog::default_logger();
            return Singleton_CTS<Log<name>>::get(std::forward<Args>(args)...);
        }

        template <typename... Args> static Log<name>& initialize(Args&&... args) {
            LogDetail::reject_backend_reentry("initialize a logger");
            (void)spdlog::default_logger();
            return Singleton_CTS<Log<name>>::initialize(std::forward<Args>(args)...);
        }

        static void set_pattern(const char* fmt) { get().set_pattern(fmt); }

        [[nodiscard]] static std::filesystem::path get_file_path() { return get().get_file_path(); }

        [[nodiscard]] static const LogConfig& initial_configuration() { return get().initial_configuration(); }

        [[nodiscard]] static LogQueueStats shared_queue_stats() { return get().shared_queue_stats(); }

        [[nodiscard]] static LogBackendStats backend_stats() { return get().backend_stats(); }

        static void report_diagnostics() { get().report_diagnostics(); }

        static void flush() { get().flush(); }

        static void close(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero()) {
            get().close(timeout);
        }

        static void reopen() { get().reopen(); }

        static void make_default() { get().make_default(); }

        static void set_level_logger(spdlog::level level) { get().set_level_logger(level); }

        static void set_level_filesink(spdlog::level level) { get().set_level_filesink(level); }

        static void set_level_stdsink(spdlog::level level) { get().set_level_stdsink(level); }

        [[nodiscard]] static bool should_log(const spdlog::level level) {
            if (LogDetail::suppress_backend_emission())
                return false;
            return get().should_log(level);
        }

        /** Evaluate diagnostics only when compiled and admitted by a destination.
         * Initialization, argument preparation, and submission failures report
         * through the emergency path instead of changing the engine operation.
         * This is ordinary logging: call outside locks and native/noexcept paths.
         */
        template <ctlog::LogLevel severity, typename Write> static void write_lazy(Write&& write) noexcept {
            if constexpr (ctlog::enabled(severity)) {
                if (LogDetail::suppress_backend_emission())
                    return;
                try {
                    auto& log = get();
                    if (log.should_log(ctlog::runtime_level(severity)))
                        std::forward<Write>(write)(log);
                } catch (...) {
                    Diagnostics::report_failure("logging write", std::current_exception());
                }
            }
        }

        template <typename... Args> static void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if constexpr (ctlog::enabled(ctlog::TRACE_)) {
                if (LogDetail::suppress_backend_emission())
                    return;
                get().trace(fmt, std::forward<Args>(args)...);
            }
        }

        template <typename... Args> static void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if constexpr (ctlog::enabled(ctlog::DEBUG_)) {
                if (LogDetail::suppress_backend_emission())
                    return;
                get().debug(fmt, std::forward<Args>(args)...);
            }
        }

        template <typename... Args> static void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if constexpr (ctlog::enabled(ctlog::INFO_)) {
                if (LogDetail::suppress_backend_emission())
                    return;
                get().info(fmt, std::forward<Args>(args)...);
            }
        }

        template <typename... Args> static void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if constexpr (ctlog::enabled(ctlog::WARNING_)) {
                if (LogDetail::suppress_backend_emission())
                    return;
                get().warn(fmt, std::forward<Args>(args)...);
            }
        }

        template <typename... Args> static void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if constexpr (ctlog::enabled(ctlog::ERROR_)) {
                if (LogDetail::suppress_backend_emission())
                    return;
                get().error(fmt, std::forward<Args>(args)...);
            }
        }

        template <typename... Args> static void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if constexpr (ctlog::enabled(ctlog::FATAL_)) {
                if (LogDetail::suppress_backend_emission())
                    return;
                get().critical(fmt, std::forward<Args>(args)...);
            }
        }

        static void strace(void* addr0 = nullptr) {
            if constexpr (ctlog::enabled(ctlog::TRACE_)) {
                if (LogDetail::suppress_backend_emission())
                    return;
                get().strace(addr0);
            }
        }
    };
}
