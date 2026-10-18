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
/** Named lazy access to one Engine-owned category, with configuration before first use.
     * @tparam name Category with static character storage. Equal textual names share Engine registry state across libraries.
     * Keep ProcessLifetime through all callers. Ordinary writes do not resolve the host application's native default logger.
     */
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

/**
 * @fn template<const char* name> Log<name>& CE::Logging::Logger<name>::get()
 * @brief Lazily acquire the named category with profile defaults if not yet initialized.
 * @return Borrowed facade; initialization/open errors can propagate and backend reentry rejects.
 * Use initialize() before writers to guarantee explicit settings. Process logging shutdown is terminal.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> Log<name>& CE::Logging::Logger<name>::get(FileEventHandlers handlers, Config config)
 * @brief Acquire the lazy category, using supplied settings only for its first successful construction.
 * @param handlers Owned callbacks if this call creates the category; otherwise ignored.
 * @param config Startup configuration if this call creates the category; otherwise ignored.
 * @return Borrowed Engine-owned category; concurrent creation can wait. Prefer initialize() when repeated explicit configuration must reject.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> Log<name>& CE::Logging::Logger<name>::initialize(FileEventHandlers handlers, Config config)
 * @brief Explicitly initialize this category once before producers or lazy access.
 * @param handlers Owned callbacks with protected capture lifetime.
 * @param config Valid initial settings.
 * @return Borrowed named facade.
 * Repeated explicit initialization, including after prior lazy creation, rejects with bad_request. Opening failures propagate; protected capture/process lifetime remains the caller's responsibility.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> LogHandle* CE::Logging::Logger<name>::get_existing() noexcept
 * @brief Borrow an already published category without initialization or waiting.
 * @return Engine-owned handle, or nullptr when absent, being published, contended or unavailable.
 * Equal textual names find the same state across libraries. Protect the process lifetime while using the borrowed handle.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> CE::Logging::Logger<name>::Logger(const FileEventHandlers& event_handlers, Config config)
 * @brief Compatibility construction acquiring the lazy category.
 * @param event_handlers Settings used only if this performs first successful construction.
 * @param config Configuration used only if this performs first successful construction.
 * Use explicit initialize() before writers when configuration must not be silently ignored.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> void CE::Logging::Logger<name>::set_pattern(const char* fmt)
 * @brief Acquire the named category and forward set_pattern with the LogHandle contract.
 * @see CE::Logging::LogHandle::set_pattern
 * Unlike get_existing(), this can initialize a destination and propagate opening errors. Runtime thresholds do not restore compiled-out severities; lifecycle use requires caller/ProcessLifetime coordination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> std::filesystem::path CE::Logging::Logger<name>::get_file_path()
 * @brief Acquire the named category and forward get_file_path with the LogHandle contract.
 * @see CE::Logging::LogHandle::get_file_path
 * Unlike get_existing(), this can initialize a destination and propagate opening errors. Runtime thresholds do not restore compiled-out severities; lifecycle use requires caller/ProcessLifetime coordination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> const CE::Logging::Config& CE::Logging::Logger<name>::initial_configuration()
 * @brief Acquire the named category and forward initial_configuration with the LogHandle contract.
 * @see CE::Logging::LogHandle::initial_configuration
 * Unlike get_existing(), this can initialize a destination and propagate opening errors. Runtime thresholds do not restore compiled-out severities; lifecycle use requires caller/ProcessLifetime coordination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> CE::Logging::QueueStats CE::Logging::Logger<name>::shared_queue_stats()
 * @brief Acquire the named category and forward shared_queue_stats with the LogHandle contract.
 * @see CE::Logging::LogHandle::shared_queue_stats
 * Unlike get_existing(), this can initialize a destination and propagate opening errors. Runtime thresholds do not restore compiled-out severities; lifecycle use requires caller/ProcessLifetime coordination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> CE::Logging::BackendStats CE::Logging::Logger<name>::backend_stats()
 * @brief Acquire the named category and forward backend_stats with the LogHandle contract.
 * @see CE::Logging::LogHandle::backend_stats
 * Unlike get_existing(), this can initialize a destination and propagate opening errors. Runtime thresholds do not restore compiled-out severities; lifecycle use requires caller/ProcessLifetime coordination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> void CE::Logging::Logger<name>::report_diagnostics()
 * @brief Acquire the named category and forward report_diagnostics with the LogHandle contract.
 * @see CE::Logging::LogHandle::report_diagnostics
 * Unlike get_existing(), this can initialize a destination and propagate opening errors. Runtime thresholds do not restore compiled-out severities; lifecycle use requires caller/ProcessLifetime coordination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> void CE::Logging::Logger<name>::flush()
 * @brief Acquire the named category and forward flush with the LogHandle contract.
 * @see CE::Logging::LogHandle::flush
 * Unlike get_existing(), this can initialize a destination and propagate opening errors. Runtime thresholds do not restore compiled-out severities; lifecycle use requires caller/ProcessLifetime coordination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> void CE::Logging::Logger<name>::close(std::chrono::milliseconds timeout)
 * @brief Acquire the named category and forward close with the LogHandle contract.
 * @see CE::Logging::LogHandle::close
 * Unlike get_existing(), this can initialize a destination and propagate opening errors. Runtime thresholds do not restore compiled-out severities; lifecycle use requires caller/ProcessLifetime coordination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> void CE::Logging::Logger<name>::reopen()
 * @brief Acquire the named category and forward reopen with the LogHandle contract.
 * @see CE::Logging::LogHandle::reopen
 * Unlike get_existing(), this can initialize a destination and propagate opening errors. Runtime thresholds do not restore compiled-out severities; lifecycle use requires caller/ProcessLifetime coordination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> void CE::Logging::Logger<name>::make_default()
 * @brief Acquire the named category and forward make_default with the LogHandle contract.
 * @see CE::Logging::LogHandle::make_default
 * Unlike get_existing(), this can initialize a destination and propagate opening errors. Runtime thresholds do not restore compiled-out severities; lifecycle use requires caller/ProcessLifetime coordination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> void CE::Logging::Logger<name>::set_level_logger(CE::Logging::Level level)
 * @brief Acquire the named category and forward set_level_logger with the LogHandle contract.
 * @see CE::Logging::LogHandle::set_level_logger
 * Unlike get_existing(), this can initialize a destination and propagate opening errors. Runtime thresholds do not restore compiled-out severities; lifecycle use requires caller/ProcessLifetime coordination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> void CE::Logging::Logger<name>::set_level_filesink(CE::Logging::Level level)
 * @brief Acquire the named category and forward set_level_filesink with the LogHandle contract.
 * @see CE::Logging::LogHandle::set_level_filesink
 * Unlike get_existing(), this can initialize a destination and propagate opening errors. Runtime thresholds do not restore compiled-out severities; lifecycle use requires caller/ProcessLifetime coordination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> void CE::Logging::Logger<name>::set_level_stdsink(CE::Logging::Level level)
 * @brief Acquire the named category and forward set_level_stdsink with the LogHandle contract.
 * @see CE::Logging::LogHandle::set_level_stdsink
 * Unlike get_existing(), this can initialize a destination and propagate opening errors. Runtime thresholds do not restore compiled-out severities; lifecycle use requires caller/ProcessLifetime coordination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> bool CE::Logging::Logger<name>::should_log(CE::Logging::Level level)
 * @brief Acquire the named category and forward should_log with the LogHandle contract.
 * @see CE::Logging::LogHandle::should_log
 * Unlike get_existing(), this can initialize a destination and propagate opening errors. Runtime thresholds do not restore compiled-out severities; lifecycle use requires caller/ProcessLifetime coordination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> void CE::Logging::Logger<name>::strace(void* addr0)
 * @brief Acquire the named category and forward strace with the LogHandle contract.
 * @see CE::Logging::LogHandle::strace
 * Unlike get_existing(), this can initialize a destination and propagate opening errors. Runtime thresholds do not restore compiled-out severities; lifecycle use requires caller/ProcessLifetime coordination.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> template<CE::Logging::Compile::LogLevel severity, typename Write> void CE::Logging::Logger<name>::write_lazy(Write&& write) noexcept
 * @brief Evaluate diagnostic preparation only when compilation and destination gates admit it.
 * @tparam severity One compile-time severity bit.
 * @tparam Write Callable accepting Log<name>&.
 * @param write Callable invoked synchronously after admission; not retained when disabled.
 * Initialization, preparation and submission exceptions go to emergency reporting rather than changing the engine operation. This is ordinary logging: call outside locks/native callbacks; callback construction expressions still occur before entry.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> template<typename... Args> void CE::Logging::Logger<name>::trace(std::format_string<Args...> fmt, Args&&... args)
 * @brief Submit a named trace record through LogHandle's compiled/runtime gates.
 * @tparam Args Checked formatting argument types.
 * @param fmt Compile-time checked std::format string.
 * @param args Forwarded values; argument expressions are evaluated before this call.
 * @see CE::Logging::LogHandle::trace
 * Can initialize the category and propagate opening/formatting/submission failures; Block overflow can wait. Use write_lazy() for gated preparation and contained diagnostic failures.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> template<typename... Args> void CE::Logging::Logger<name>::debug(std::format_string<Args...> fmt, Args&&... args)
 * @brief Submit a named debug record through LogHandle's compiled/runtime gates.
 * @tparam Args Checked formatting argument types.
 * @param fmt Compile-time checked std::format string.
 * @param args Forwarded values; argument expressions are evaluated before this call.
 * @see CE::Logging::LogHandle::debug
 * Can initialize the category and propagate opening/formatting/submission failures; Block overflow can wait. Use write_lazy() for gated preparation and contained diagnostic failures.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> template<typename... Args> void CE::Logging::Logger<name>::info(std::format_string<Args...> fmt, Args&&... args)
 * @brief Submit a named info record through LogHandle's compiled/runtime gates.
 * @tparam Args Checked formatting argument types.
 * @param fmt Compile-time checked std::format string.
 * @param args Forwarded values; argument expressions are evaluated before this call.
 * @see CE::Logging::LogHandle::info
 * Can initialize the category and propagate opening/formatting/submission failures; Block overflow can wait. Use write_lazy() for gated preparation and contained diagnostic failures.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> template<typename... Args> void CE::Logging::Logger<name>::warn(std::format_string<Args...> fmt, Args&&... args)
 * @brief Submit a named warn record through LogHandle's compiled/runtime gates.
 * @tparam Args Checked formatting argument types.
 * @param fmt Compile-time checked std::format string.
 * @param args Forwarded values; argument expressions are evaluated before this call.
 * @see CE::Logging::LogHandle::warn
 * Can initialize the category and propagate opening/formatting/submission failures; Block overflow can wait. Use write_lazy() for gated preparation and contained diagnostic failures.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> template<typename... Args> void CE::Logging::Logger<name>::error(std::format_string<Args...> fmt, Args&&... args)
 * @brief Submit a named error record through LogHandle's compiled/runtime gates.
 * @tparam Args Checked formatting argument types.
 * @param fmt Compile-time checked std::format string.
 * @param args Forwarded values; argument expressions are evaluated before this call.
 * @see CE::Logging::LogHandle::error
 * Can initialize the category and propagate opening/formatting/submission failures; Block overflow can wait. Use write_lazy() for gated preparation and contained diagnostic failures.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn template<const char* name> template<typename... Args> void CE::Logging::Logger<name>::critical(std::format_string<Args...> fmt, Args&&... args)
 * @brief Submit a named critical record through LogHandle's compiled/runtime gates.
 * @tparam Args Checked formatting argument types.
 * @param fmt Compile-time checked std::format string.
 * @param args Forwarded values; argument expressions are evaluated before this call.
 * @see CE::Logging::LogHandle::critical
 * Can initialize the category and propagate opening/formatting/submission failures; Block overflow can wait. Use write_lazy() for gated preparation and contained diagnostic failures.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */
