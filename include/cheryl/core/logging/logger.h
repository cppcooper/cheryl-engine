#pragma once
#include "log.h"
#include <templates/singleton.h>

#include <chrono>
#include <utility>

namespace CE {
    // Default access lazily creates Log<name>. Configure with initialize(handlers,
    // config) on the logging owner before any writer/default access;
    // repeated explicit initialization rejects. This compatibility constructor's
    // handlers/config apply only if it performs the first successful construction.
    template <const char* name> class Logger : public Singleton_CTS<Log<name>> {
    protected:
        explicit Logger(const spdlog::file_event_handlers& event_handlers = {}, LogConfig config = LogConfig::for_logger(name)) {
            Singleton_CTS<Log<name>>::get(event_handlers, std::move(config));
        }

    public:
        static void set_pattern(const char* fmt) { Singleton_CTS<Log<name>>::get().set_pattern(fmt); }

        [[nodiscard]] static std::filesystem::path get_file_path() { return Singleton_CTS<Log<name>>::get().get_file_path(); }

        [[nodiscard]] static const LogConfig& initial_configuration() { return Singleton_CTS<Log<name>>::get().initial_configuration(); }

        [[nodiscard]] static LogQueueStats shared_queue_stats() { return Singleton_CTS<Log<name>>::get().shared_queue_stats(); }

        [[nodiscard]] static LogBackendStats backend_stats() { return Singleton_CTS<Log<name>>::get().backend_stats(); }

        static void flush() { Singleton_CTS<Log<name>>::get().flush(); }

        static void close(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero()) {
            Singleton_CTS<Log<name>>::get().close(timeout);
        }

        static void reopen() { Singleton_CTS<Log<name>>::get().reopen(); }

        static void make_default() { Singleton_CTS<Log<name>>::get().make_default(); }

        static void set_level_logger(spdlog::level level) { Singleton_CTS<Log<name>>::get().set_level_logger(level); }

        static void set_level_filesink(spdlog::level level) { Singleton_CTS<Log<name>>::get().set_level_filesink(level); }

        static void set_level_stdsink(spdlog::level level) { Singleton_CTS<Log<name>>::get().set_level_stdsink(level); }

        [[nodiscard]] static bool should_log(const spdlog::level level) { return Singleton_CTS<Log<name>>::get().should_log(level); }

        /** Evaluate diagnostics only when compiled and admitted by a destination.
         * Initialization, argument preparation, and submission failures report
         * through the emergency path instead of changing the engine operation.
         * This is ordinary logging: call outside locks and native/noexcept paths.
         */
        template <ctlog::LogLevel severity, typename Write> static void write_lazy(Write&& write) noexcept {
            if constexpr (ctlog::enabled(severity)) {
                try {
                    auto& log = Singleton_CTS<Log<name>>::get();
                    if (log.should_log(ctlog::runtime_level(severity)))
                        std::forward<Write>(write)(log);
                } catch (...) {
                    Diagnostics::report_failure("logging write", std::current_exception());
                }
            }
        }

        template <typename... Args> static void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if constexpr (ctlog::enabled(ctlog::TRACE_))
                Singleton_CTS<Log<name>>::get().trace(fmt, std::forward<Args>(args)...);
        }

        template <typename... Args> static void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if constexpr (ctlog::enabled(ctlog::DEBUG_))
                Singleton_CTS<Log<name>>::get().debug(fmt, std::forward<Args>(args)...);
        }

        template <typename... Args> static void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if constexpr (ctlog::enabled(ctlog::INFO_))
                Singleton_CTS<Log<name>>::get().info(fmt, std::forward<Args>(args)...);
        }

        template <typename... Args> static void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if constexpr (ctlog::enabled(ctlog::WARNING_))
                Singleton_CTS<Log<name>>::get().warn(fmt, std::forward<Args>(args)...);
        }

        template <typename... Args> static void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if constexpr (ctlog::enabled(ctlog::ERROR_))
                Singleton_CTS<Log<name>>::get().error(fmt, std::forward<Args>(args)...);
        }

        template <typename... Args> static void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if constexpr (ctlog::enabled(ctlog::FATAL_))
                Singleton_CTS<Log<name>>::get().critical(fmt, std::forward<Args>(args)...);
        }

        static void strace(void* addr0 = nullptr) {
            if constexpr (ctlog::enabled(ctlog::TRACE_))
                Singleton_CTS<Log<name>>::get().strace(addr0);
        }
    };
}
