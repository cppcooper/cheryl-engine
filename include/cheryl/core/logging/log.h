#pragma once
#include "osink.h"
#include <templates/singleton.h>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace CE::LogDetail {
    inline std::mutex default_logger_mutex;

    [[nodiscard]] uint16_t next_log_id() noexcept;

    enum class LogState {
        Opening,
        Open,
        Closing,
        Closed
    };

    enum class StateRequirement {
        Allow,
        BadRequest,
        FailedOperation
    };

    struct StateRule {
        LogState state = LogState::Closed;
        StateRequirement requirement = StateRequirement::Allow;
        const char* reason = nullptr;
    };

    struct ReopenState {
        spdlog::level logger;
        spdlog::level file;
        spdlog::level console;
        bool restore_default = false;
    };

    class LogStateController final {
        mutable std::mutex mutex;
        std::condition_variable cv;
        LogState state = LogState::Closed;

    public:
        using Lock = std::unique_lock<std::mutex>;

        void require_state(const Lock& lock, const char* logger, const char* operation, std::initializer_list<StateRule> rules) const;
        void set_state(const Lock& lock, LogState new_state);
        void complete_close();
        [[nodiscard]] Lock lock() const;
        [[nodiscard]] LogState get_state(const Lock& lock) const;
        [[nodiscard]] bool wait_until_closed(Lock& lock, std::chrono::milliseconds timeout);
        [[nodiscard]] bool is_open(const Lock& lock) const;
        [[nodiscard]] bool is_closed(const Lock& lock) const;
        [[nodiscard]] bool is_opening(const Lock& lock) const;
        [[nodiscard]] bool is_closing(const Lock& lock) const;
        [[nodiscard]] bool is_transitioning(const Lock& lock) const;

    private:
        void assert_locked(const Lock& lock) const;
    };
}

namespace CE {
    extern std::string stack_trace(void* addr0 = nullptr);

    template <const char*>
    class Log {
        template <typename T>
        using atomic_shared_ptr = std::atomic<std::shared_ptr<T>>;

    protected:
        uint16_t log_id = 0;
        const spdlog::file_event_handlers event_handlers;
        atomic_shared_ptr<spdlog::logger> m_logger{nullptr};
        atomic_shared_ptr<spdlog::sinks::rotating_file_sink_mt> m_file{nullptr};
        atomic_shared_ptr<osink_mt> m_console{nullptr};

    private:
        using LogStateController = LogDetail::LogStateController;
        using LogState = LogDetail::LogState;
        using StateRequirement = LogDetail::StateRequirement;
        using StateRule = LogDetail::StateRule;
        using ReopenState = LogDetail::ReopenState;

        void construct_log();
        void release_registry_ownership() const noexcept;
        std::shared_ptr<LogStateController> state_controller = std::make_shared<LogStateController>();
        std::shared_ptr<spdlog::logger> m_fallback_logger;
        std::optional<ReopenState> reopen_state;

    protected:
        template <typename T>
        [[nodiscard]] std::shared_ptr<T> acquire_open_resource(
            const LogStateController::Lock& lock,
            const atomic_shared_ptr<T>& resource,
            const char* operation
        ) const;
        void require_open_state(const LogStateController::Lock& lock, const char* operation) const;

    public:
        explicit Log(spdlog::file_event_handlers event_handlers = {});
        ~Log() noexcept;
        [[nodiscard]] std::filesystem::path get_file_path() const;
        [[nodiscard]] uint16_t get_log_id() const;
        void flush() const;
        void close(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());
        void reopen();
        void make_default() const;
        void set_level_logger(spdlog::level level) const;
        void set_level_filesink(spdlog::level level) const;
        void set_level_stdsink(spdlog::level level) const;

        // Sets the pattern for the logger and propagates it to its current sinks.
        void set_pattern(const char* fmt) const {
            const auto lock = state_controller->lock();
            acquire_open_resource(lock, m_logger, "set the pattern for")->set_pattern(fmt);
        }

        void set_pattern_filesink(const char* fmt) const {
            const auto lock = state_controller->lock();
            acquire_open_resource(lock, m_file, "set the file pattern for")->set_pattern(fmt);
        }

        void set_pattern_stdsink(const char* fmt) const {
            const auto lock = state_controller->lock();
            acquire_open_resource(lock, m_console, "set the console pattern for")->set_pattern(fmt);
        }

        void strace(void* addr0 = nullptr) const {
            if (const auto logger = m_logger.load(); logger && logger->should_log(spdlog::level::trace)) {
                logger->log(spdlog::level::trace, "{}", stack_trace(addr0));
            }
        }

        template <typename... Args>
        void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if (auto logger = m_logger.load()) {
                logger->log(spdlog::level::trace, fmt, std::forward<Args>(args)...);
            }
        }

        template <typename... Args>
        void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if (auto logger = m_logger.load()) {
                logger->log(spdlog::level::debug, fmt, std::forward<Args>(args)...);
            }
        }

        template <typename... Args>
        void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if (auto logger = m_logger.load()) {
                logger->log(spdlog::level::info, fmt, std::forward<Args>(args)...);
            }
        }

        template <typename... Args>
        void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if (auto logger = m_logger.load()) {
                logger->log(spdlog::level::warn, fmt, std::forward<Args>(args)...);
            }
        }

        template <typename... Args>
        void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if (auto logger = m_logger.load()) {
                logger->log(spdlog::level::err, fmt, std::forward<Args>(args)...);
            }
        }

        template <typename... Args>
        void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            if (auto logger = m_logger.load()) {
                if (logger->should_log(spdlog::level::trace)) {
                    logger->log(spdlog::level::trace, "{}", stack_trace());
                }
                logger->log(spdlog::level::critical, fmt, std::forward<Args>(args)...);
            }
        }
    };
}

namespace spdlog::CE {
    struct TPInit : Singleton_CTS<TPInit> {
        std::shared_ptr<details::thread_pool> tp;

        TPInit() {
            init_thread_pool(8192, 3);
            tp = thread_pool();
        }
    };
}

// template definitions - methods
namespace CE {
    template <const char* name>
    void Log<name>::construct_log() {
        // Reserve the closed logger for construction and capture any state that must be
        // restored before releasing lifecycle synchronization.
        auto lock = state_controller->lock();
        state_controller->require_state(lock, name, "construct", {
            {LogState::Closed, StateRequirement::Allow},
            {LogState::Open, StateRequirement::BadRequest, "it is already open"},
            {LogState::Opening, StateRequirement::BadRequest, "it is already opening"},
            {LogState::Closing, StateRequirement::FailedOperation, "it is closing"}
        });

        const auto restore = reopen_state;
        state_controller->set_state(lock, LogState::Opening);
        lock.unlock();

        bool registered = false;
        try {
            // Build a complete replacement resource graph before publishing it to callers.
            // The file sink deleter becomes the close-completion signal once the lifecycle
            // later enters Closing; destruction during a failed opening is intentionally ignored.
            auto console = std::make_shared<osink_mt>();
            const auto controller = state_controller;
            auto file = std::shared_ptr<spdlog::sinks::rotating_file_sink_mt>(
                new spdlog::sinks::rotating_file_sink_mt(
                    std::format("logs/{}.log", name),
                    1024 * 1024 * 10, 5, true, event_handlers
                ),
                [controller](spdlog::sinks::rotating_file_sink_mt* sink) {
                    delete sink;
                    controller->complete_close();
                }
            );

            std::vector<spdlog::sink_ptr> sinks{console, file};
            auto logger = std::make_shared<spdlog::async_logger>(
                std::format("{}", name),
                sinks.begin(),
                sinks.end(),
                spdlog::CE::TPInit::get().tp,
                spdlog::async_overflow_policy::block
            );

            // Reapply the configuration captured by close() while the replacement resources
            // are still private, so callers never observe a partially restored logger.
            if (restore) {
                logger->set_level(restore->logger);
                file->set_level(restore->file);
                console->set_level(restore->console);
            }

            // Register first, then commit default ownership and member publication while the
            // lifecycle remains Opening. Open is exposed only after the full graph is available.
            register_logger(logger);
            registered = true;
            lock.lock();

            if (restore && restore->restore_default) {
                const std::lock_guard default_lock(LogDetail::default_logger_mutex);
                if (spdlog::default_logger() == m_fallback_logger) {
                    set_default_logger(logger);
                }
                spdlog::drop(m_fallback_logger->name());
            }

            m_file.store(std::move(file));
            m_console.store(std::move(console));
            m_logger.store(std::move(logger));
            reopen_state.reset();
            state_controller->set_state(lock, LogState::Open);
        }
        catch (...) {
            // Registration is externally visible, so roll it back before returning Opening to
            // Closed. Release the state lock first because dropping the logger may destroy the
            // file sink, whose deleter also enters the state controller.
            if (lock.owns_lock()) {
                lock.unlock();
            }
            if (registered) {
                spdlog::drop(name);
            }
            lock.lock();
            state_controller->set_state(lock, LogState::Closed);
            throw;
        }
    }

    template <const char* name>
    Log<name>::Log(spdlog::file_event_handlers event_handlers)
    : event_handlers(std::move(event_handlers)),
      m_fallback_logger(std::make_shared<spdlog::logger>(
          std::format("{}-closed", name), std::make_shared<spdlog::sinks::null_sink_mt>())) {
        m_fallback_logger->set_level(spdlog::level::off);
        construct_log();
        log_id = LogDetail::next_log_id();
    }

    template <const char* name>
    Log<name>::~Log() noexcept {
        // Ordinary destruction attempts the same strong close boundary exposed by close(): under
        // normal ownership, the file sink is destroyed before shutdown completes. External spdlog
        // owners can extend that lifetime, so destruction waits at most one minute.
        try {
            close(std::chrono::seconds{60});
        }
        catch (...) {
            // Destructors cannot propagate lifecycle failures. A timeout leaves Closing intact,
            // and the sink-held state controller remains alive until the final external owner exits.
        }

        release_registry_ownership();
    }

    template <const char* name>
    void Log<name>::release_registry_ownership() const noexcept {
        try {
            const std::lock_guard default_lock(LogDetail::default_logger_mutex);

            // Remove only registrations that still refer to this Log's objects. Pointer identity
            // avoids disturbing another logger that later reused either textual name.
            if (const auto logger = m_logger.load(); logger && logger != m_fallback_logger && spdlog::get(name) == logger) {
                spdlog::drop(name);
            }
            if (spdlog::get(m_fallback_logger->name()) == m_fallback_logger) {
                spdlog::drop(m_fallback_logger->name());
            }
        }
        catch (...) {
            // Registry cleanup is best-effort during noexcept destruction.
        }
    }

    template <const char* name>
    std::filesystem::path Log<name>::get_file_path() const {
        const auto lock = state_controller->lock();
        return std::filesystem::absolute(acquire_open_resource(lock, m_file, "get the file path for")->filename());
    }

    template <const char* name>
    uint16_t Log<name>::get_log_id() const {
        return log_id;
    }

    template <const char* name>
    void Log<name>::flush() const {
        const auto lock = state_controller->lock();
        acquire_open_resource(lock, m_logger, "flush")->flush();
    }

    template <const char* name>
    void Log<name>::close(std::chrono::milliseconds timeout) {
        auto lock = state_controller->lock();
        state_controller->require_state(lock, name, "close", {
            {LogState::Open, StateRequirement::Allow},
            {LogState::Closing, StateRequirement::Allow},
            {LogState::Closed, StateRequirement::Allow},
            {LogState::Opening,
             StateRequirement::BadRequest,
             "it is still opening; review the caller's lifecycle assumptions or synchronization"
            }
        });

        if (state_controller->is_closed(lock)) {
            return;
        }

        if (state_controller->is_open(lock)) {
            auto logger = acquire_open_resource(lock, m_logger, "close");
            auto file = acquire_open_resource(lock, m_file, "close");
            auto console = acquire_open_resource(lock, m_console, "close");

            {
                const std::lock_guard default_lock(LogDetail::default_logger_mutex);
                const bool restore_default = spdlog::default_logger() == logger;
                reopen_state = ReopenState{
                    logger->log_level(),
                    file->log_level(),
                    console->log_level(),
                    restore_default
                };
                if (restore_default) {
                    set_default_logger(m_fallback_logger);
                }
            }

            m_logger.store(m_fallback_logger);
            m_file.store(nullptr);
            m_console.store(nullptr);
            state_controller->set_state(lock, LogState::Closing);
            lock.unlock();

            spdlog::drop(name);
            logger.reset();
            file.reset();
            console.reset();

            lock.lock();
        }

        if (!state_controller->wait_until_closed(lock, timeout)) {
            const auto message = std::format("Timed out while closing the '{}' logger.", name);
            throw Exceptions::failed_operation(CE_HERE, message);
        }
    }

    template <const char* name>
    void Log<name>::reopen() {
        auto lock = state_controller->lock();
        state_controller->require_state(lock, name, "reopen", {
            {LogState::Closed, StateRequirement::Allow},
            {LogState::Open, StateRequirement::Allow},
            {LogState::Closing,
             StateRequirement::BadRequest,
             "it is still closing; review the caller's lifecycle assumptions or synchronization"
            },
            {LogState::Opening,
             StateRequirement::BadRequest,
             "it is already opening; review the caller's lifecycle assumptions or synchronization"
            }
        });

        if (state_controller->is_open(lock)) {
            return;
        }

        lock.unlock();
        construct_log();
    }

    template <const char* name>
    void Log<name>::make_default() const {
        const auto lock = state_controller->lock();
        const auto logger = acquire_open_resource(lock, m_logger, "make default");
        const std::lock_guard default_lock(LogDetail::default_logger_mutex);
        set_default_logger(logger);
    }

    template <const char* name>
    void Log<name>::set_level_logger(spdlog::level level) const {
        const auto lock = state_controller->lock();
        acquire_open_resource(lock, m_logger, "set the log level for")->set_level(level);
    }

    template <const char* name>
    void Log<name>::set_level_filesink(spdlog::level level) const {
        const auto lock = state_controller->lock();
        acquire_open_resource(lock, m_file, "set the file log level for")->set_level(level);
    }

    template <const char* name>
    void Log<name>::set_level_stdsink(spdlog::level level) const {
        const auto lock = state_controller->lock();
        acquire_open_resource(lock, m_console, "set the console log level for")->set_level(level);
    }

    template <const char* name>
    template <typename T>
    std::shared_ptr<T> Log<name>::acquire_open_resource(
        const LogStateController::Lock& lock,
        const std::atomic<std::shared_ptr<T>>& resource,
        const char* operation
    ) const {
        require_open_state(lock, operation);

        if (auto ptr = resource.load()) {
            return ptr;
        }
        const auto message = std::format("The '{}' logger is open but the requested resource is nullptr.", name);
        throw Exceptions::failed_operation(CE_HERE, message);
    }

    template <const char* name>
    void Log<name>::require_open_state(const LogStateController::Lock& lock, const char* operation) const {
        state_controller->require_state(lock, name, operation, {
            {LogState::Open, StateRequirement::Allow},
            {LogState::Closed, StateRequirement::BadRequest, "it is closed"},
            {LogState::Closing, StateRequirement::FailedOperation, "it is closing"},
            {LogState::Opening, StateRequirement::FailedOperation, "it is opening"}
        });
    }
}

// LogStateController
//////////////////
namespace CE::LogDetail {
    inline void LogStateController::require_state(
        const Lock& lock,
        const char* logger,
        const char* operation,
        std::initializer_list<StateRule> rules
    ) const {
        assert_locked(lock);
        const auto current = state;

        for (const auto& [rule_state, requirement, reason] : rules) {
            if (current != rule_state) {
                continue;
            }
            if (requirement == StateRequirement::Allow) {
                return;
            }

            const auto message = std::format("Cannot {} the '{}' logger because {}.", operation, logger, reason);
            if (requirement == StateRequirement::BadRequest) {
                throw Exceptions::bad_request(CE_HERE, message);
            }
            throw Exceptions::failed_operation(CE_HERE, message);
        }

        const auto message = std::format("The '{}' logger has no lifecycle rule for operation '{}'.", logger, operation);
        throw Exceptions::failed_operation(CE_HERE, message);
    }

    inline void LogStateController::set_state(const Lock& lock, LogState new_state) {
        assert_locked(lock);
        state = new_state;
    }

    inline void LogStateController::complete_close() {
        auto lock = this->lock();
        if (state != LogState::Closing) {
            return;
        }

        state = LogState::Closed;
        lock.unlock();
        cv.notify_all();
    }

    inline LogStateController::Lock LogStateController::lock() const {
        return Lock{mutex};
    }

    inline LogState LogStateController::get_state(const Lock& lock) const {
        assert_locked(lock);
        return state;
    }

    inline bool LogStateController::wait_until_closed(Lock& lock, std::chrono::milliseconds timeout) {
        assert_locked(lock);
        if (timeout == std::chrono::milliseconds::zero()) {
            cv.wait(lock, [this] {
                return state == LogState::Closed;
            });
            return true;
        }

        return cv.wait_for(lock, timeout, [this] {
            return state == LogState::Closed;
        });
    }

    inline bool LogStateController::is_open(const Lock& lock) const {
        return get_state(lock) == LogState::Open;
    }

    inline bool LogStateController::is_closed(const Lock& lock) const {
        return get_state(lock) == LogState::Closed;
    }

    inline bool LogStateController::is_opening(const Lock& lock) const {
        return get_state(lock) == LogState::Opening;
    }

    inline bool LogStateController::is_closing(const Lock& lock) const {
        return get_state(lock) == LogState::Closing;
    }

    inline bool LogStateController::is_transitioning(const Lock& lock) const {
        const auto current = get_state(lock);
        return current == LogState::Opening || current == LogState::Closing;
    }

    inline void LogStateController::assert_locked(const Lock& lock) const {
        assert(lock.owns_lock());
        assert(lock.mutex() == &mutex);
    }
}
