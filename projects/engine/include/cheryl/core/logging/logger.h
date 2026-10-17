#pragma once
#include "log.h"
#include <atomic>

#include <chrono>
#include <utility>

namespace CE::Logging {
    // Named access lazily creates Log<name>; writes never resolve the host's default
    // logger. Configure with initialize(handlers, config) on the logging owner
    // before any writer/lazy access;
    // repeated explicit initialization rejects. This compatibility constructor's
    // handlers/config apply only if it performs the first successful construction.
    template <const char* name> class Logger {
        inline static std::atomic<Log<name>*> published_{nullptr};
        struct Facade {
            Log<name> value;
            explicit Facade(std::shared_ptr<Detail::LogState> state)
            : value(std::move(state)) {}
            ~Facade() { published_.store(nullptr, std::memory_order_release); }
        };
    protected:
        explicit Logger(
            const ::CE::Logging::FileEventHandlers& event_handlers = {},
            ::CE::Logging::Config config = ::CE::Logging::Config::for_logger(name)
        ) {
            get(event_handlers, std::move(config));
        }

    public:
        static ::CE::Logging::Log<name>& get() {
            ::CE::Logging::Detail::reject_backend_reentry("acquire a logger");
            if (auto* existing = published_.load(std::memory_order_acquire))
                return *existing;
            return get({}, ::CE::Logging::Config::for_logger(name));
        }

        static ::CE::Logging::Log<name>&
        get(::CE::Logging::FileEventHandlers handlers, ::CE::Logging::Config config = ::CE::Logging::Config::for_logger(name)) {
            ::CE::Logging::Detail::reject_backend_reentry("acquire a logger");
            static Facade facade{Detail::acquire_category(name, std::move(handlers), std::move(config), false)};
            published_.store(&facade.value, std::memory_order_release);
            return facade.value;
        }

        static ::CE::Logging::Log<name>&
        initialize(::CE::Logging::FileEventHandlers handlers = {}, ::CE::Logging::Config config = ::CE::Logging::Config::for_logger(name)) {
            ::CE::Logging::Detail::reject_backend_reentry("initialize a logger");
            static_cast<void>(Detail::acquire_category(name, std::move(handlers), std::move(config), true));
            return get();
        }

        /** Borrow the Engine-owned category handle without constructing or waiting
         * for a logger. Null also means construction/publication is in progress.
         * The same textual category resolves to the same handle across libraries.
         */
        [[nodiscard]] static LogHandle* get_existing() noexcept { return Detail::existing_category(name); }

        static void set_pattern(const char* fmt) { get().set_pattern(fmt); }

        [[nodiscard]] static std::filesystem::path get_file_path() { return get().get_file_path(); }

        [[nodiscard]] static const ::CE::Logging::Config& initial_configuration() { return get().initial_configuration(); }

        [[nodiscard]] static ::CE::Logging::QueueStats shared_queue_stats() { return get().shared_queue_stats(); }

        [[nodiscard]] static ::CE::Logging::BackendStats backend_stats() { return get().backend_stats(); }

        static void report_diagnostics() { get().report_diagnostics(); }

        static void flush() { get().flush(); }

        static void close(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero()) { get().close(timeout); }

        static void reopen() { get().reopen(); }

        static void make_default() { get().make_default(); }

        static void set_level_logger(::CE::Logging::Level level) { get().set_level_logger(level); }

        static void set_level_filesink(::CE::Logging::Level level) { get().set_level_filesink(level); }

        static void set_level_stdsink(::CE::Logging::Level level) { get().set_level_stdsink(level); }

        [[nodiscard]] static bool should_log(const ::CE::Logging::Level level) {
            if (::CE::Logging::Detail::suppress_backend_emission())
                return false;
            return get().should_log(level);
        }

        /** Evaluate diagnostics only when compiled and admitted by a destination.
         * Initialization, argument preparation, and submission failures report
         * through the emergency path instead of changing the engine operation.
         * This is ordinary logging: call outside locks and native/noexcept paths.
         */
        template <::CE::Logging::Compile::LogLevel severity, typename Write> static void write_lazy(Write&& write) noexcept {
            if constexpr (::CE::Logging::Compile::enabled(severity)) {
                if (::CE::Logging::Detail::suppress_backend_emission())
                    return;
                try {
                    auto& log = get();
                    if (log.should_log(::CE::Logging::Compile::runtime_level(severity)))
                        std::forward<Write>(write)(log);
                } catch (...) {
                    ::CE::Diagnostics::report_failure("logging write", std::current_exception());
                }
            }
        }

        template <typename... Args> static void trace(std::format_string<Args...> fmt, Args&&... args) {
            if constexpr (::CE::Logging::Compile::enabled(::CE::Logging::Compile::TRACE_)) {
                if (::CE::Logging::Detail::suppress_backend_emission())
                    return;
                get().trace(fmt, std::forward<Args>(args)...);
            }
        }

        template <typename... Args> static void debug(std::format_string<Args...> fmt, Args&&... args) {
            if constexpr (::CE::Logging::Compile::enabled(::CE::Logging::Compile::DEBUG_)) {
                if (::CE::Logging::Detail::suppress_backend_emission())
                    return;
                get().debug(fmt, std::forward<Args>(args)...);
            }
        }

        template <typename... Args> static void info(std::format_string<Args...> fmt, Args&&... args) {
            if constexpr (::CE::Logging::Compile::enabled(::CE::Logging::Compile::INFO_)) {
                if (::CE::Logging::Detail::suppress_backend_emission())
                    return;
                get().info(fmt, std::forward<Args>(args)...);
            }
        }

        template <typename... Args> static void warn(std::format_string<Args...> fmt, Args&&... args) {
            if constexpr (::CE::Logging::Compile::enabled(::CE::Logging::Compile::WARNING_)) {
                if (::CE::Logging::Detail::suppress_backend_emission())
                    return;
                get().warn(fmt, std::forward<Args>(args)...);
            }
        }

        template <typename... Args> static void error(std::format_string<Args...> fmt, Args&&... args) {
            if constexpr (::CE::Logging::Compile::enabled(::CE::Logging::Compile::ERROR_)) {
                if (::CE::Logging::Detail::suppress_backend_emission())
                    return;
                get().error(fmt, std::forward<Args>(args)...);
            }
        }

        template <typename... Args> static void critical(std::format_string<Args...> fmt, Args&&... args) {
            if constexpr (::CE::Logging::Compile::enabled(::CE::Logging::Compile::FATAL_)) {
                if (::CE::Logging::Detail::suppress_backend_emission())
                    return;
                get().critical(fmt, std::forward<Args>(args)...);
            }
        }

        static void strace(void* addr0 = nullptr) {
            if constexpr (::CE::Logging::Compile::enabled(::CE::Logging::Compile::TRACE_)) {
                if (::CE::Logging::Detail::suppress_backend_emission())
                    return;
                get().strace(addr0);
            }
        }
    };
}
