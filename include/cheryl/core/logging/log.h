#pragma once
#include "osink.h"
#include <templates/singleton.h>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <condition_variable>
#include <mutex>
#include <format>
#include <filesystem>
#include <utility>

namespace CE {
    extern std::string stack_trace(void* addr0 = nullptr);

    enum LogState {
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
        LogState state;
        StateRequirement requirement;
        const char* reason = nullptr;
    };

    class LogLifecycle final {
        mutable std::mutex mutex;
        std::condition_variable cv;
        LogState state = Closed;

    public:
        using Lock = std::unique_lock<std::mutex>;

        void require_state(const Lock& lock, const char* logger, const char* operation, std::initializer_list<StateRule> rules) const;
        void set_state(const Lock& lock, LogState new_state);
        [[nodiscard]] Lock lock() const;
        [[nodiscard]] LogState get_state(const Lock& lock) const;
        [[nodiscard]] bool is_open(const Lock& lock) const;
        [[nodiscard]] bool is_closed(const Lock& lock) const;
        [[nodiscard]] bool is_opening(const Lock& lock) const;
        [[nodiscard]] bool is_closing(const Lock& lock) const;
        [[nodiscard]] bool is_transitioning(const Lock& lock) const;

    private:
        void assert_locked(const Lock& lock) const;
    };

    template <const char*>
    class Log {
        template <typename T>
        using atomic_shared_ptr = std::atomic<std::shared_ptr<T>>;

    protected:
        uint16_t log_id;
        const spdlog::file_event_handlers event_handlers;
        atomic_shared_ptr<spdlog::async_logger> m_logger{nullptr};
        atomic_shared_ptr<spdlog::sinks::rotating_file_sink_mt> m_file{nullptr};
        atomic_shared_ptr<osink_mt> m_console{nullptr};

    private:
        void construct_log();
        LogLifecycle log_lifecycle;
        spdlog::level log_level = spdlog::level::off;
        spdlog::level file_level = spdlog::level::off;
        spdlog::level console_level = spdlog::level::off;

    protected:
        template <typename T>
        [[nodiscard]] std::shared_ptr<T> acquire_open_resource(const atomic_shared_ptr<T>& resource, const char* operation) const;
        void require_open_state(const LogLifecycle::Lock& lock, const char* operation) const;
        void require_closed_state(const LogLifecycle::Lock& lock, const char* operation) const;

    public:
        explicit Log(spdlog::file_event_handlers event_handlers = {});
        [[nodiscard]] std::filesystem::path get_file_path() const;
        [[nodiscard]] uint16_t get_log_id() const;
        void flush() const;
        void close(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());
        void reopen();
        void make_default() const;
        void set_level_logger(spdlog::level level) const;
        void set_level_filesink(spdlog::level level) const;
        void set_level_stdsink(spdlog::level level) const;

        // sets pattern for logger, propagates pattern to filesink and stdsink
        void set_pattern(const char* fmt) const {
            acquire_open_resource(m_logger, "set logger pattern for")->set_pattern(fmt);
        }

        void set_pattern_filesink(const char* fmt) const {
            acquire_open_resource(m_file, "set file pattern for")->set_pattern(fmt);
        }

        void set_pattern_stdsink(const char* fmt) const {
            acquire_open_resource(m_console, "set console pattern for")->set_pattern(fmt);
        }

        void strace(void* addr0 = nullptr) const {
            acquire_open_resource(m_logger, "write a stack trace to")->log(spdlog::level::trace, "%s", stack_trace(addr0));
        }

        template <typename... Args>
        void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            acquire_open_resource(m_logger, "write a trace message to")->log(spdlog::level::trace, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            acquire_open_resource(m_logger, "write a debug message to")->log(spdlog::level::debug, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            acquire_open_resource(m_logger, "write an info message to")->log(spdlog::level::info, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            acquire_open_resource(m_logger, "write a warn message to")->log(spdlog::level::warn, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            acquire_open_resource(m_logger, "write an error message to")->log(spdlog::level::err, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) {
            strace();
            acquire_open_resource(m_logger, "write a critical message to")->log(
                spdlog::level::critical, fmt, std::forward<Args>(args)...);
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
    extern uint16_t log_counter;

    template <const char* name>
    void Log<name>::construct_log() {
        [&] {
            const auto lock = log_lifecycle.lock();
            require_closed_state(lock, "construct");
            log_lifecycle.set_state(lock, Opening);
        }();
        // Share console and rotating-file sinks behind one async logger; the thread pool
        // handles queued writes after producers return from the log call.
        auto console = std::make_shared<osink_mt>();
        auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            std::format("logs/{}.log", name),
            1024 * 1024 * 10, 5, true, event_handlers
        );

        std::vector<spdlog::sink_ptr> sinks{console, file};
        auto logger = std::make_shared<spdlog::async_logger>(
            std::format("{}", name),
            sinks.begin(),
            sinks.end(),
            spdlog::CE::TPInit::get().tp,
            spdlog::async_overflow_policy::block
        );

        register_logger(logger);
        m_file.store(std::move(file));
        m_console.store(std::move(console));
        m_logger.store(std::move(logger));
    }

    template <const char* name>
    Log<name>::Log(spdlog::file_event_handlers event_handlers)
        : event_handlers(std::move(event_handlers)) {
        construct_log();
        log_id = ++log_counter;
    }

    template <const char* name>
    std::filesystem::path Log<name>::get_file_path() const {
        return std::filesystem::absolute(acquire_open_resource(m_file, "get the file path for")->filename());
    }

    template <const char* name>
    uint16_t Log<name>::get_log_id() const {
        return log_id;
    }

    template <const char* name>
    void Log<name>::flush() const {
        acquire_open_resource(m_logger, "flush")->flush();
    }

    template <const char* name>
    void Log<name>::close(std::chrono::milliseconds timeout) {
        auto lock = log_lifecycle.lock();

        if (log_lifecycle.is_closed(lock)) {
            return;
        }

        if (log_lifecycle.get_state(lock) == Opening) {
            throw Exceptions::failed_operation(
                CE_HERE,
                std::format("Cannot close logger '{}' while it is opening.", name).c_str()
            );
        }

        if (log_lifecycle.get_state(lock) == Closing) {
            if (timeout == std::chrono::milliseconds::zero()) {
                log_lifecycle.wait_until_closed(lock);
                return;
            }

            if (!log_lifecycle.wait_until_closed_for(lock, timeout)) {
                throw Exceptions::failed_operation(
                    CE_HERE,
                    std::format("Timed out while closing logger '{}'.", name).c_str()
                );
            }

            return;
        }

        auto logger = m_logger.load();
        auto file = m_file.load();
        auto console = m_console.load();

        log_level = logger->log_level();
        file_level = file->log_level();
        console_level = console->log_level();

        log_lifecycle.set_state(lock, Closing);

        m_logger.store(null_logger);
        m_file.store(nullptr);
        m_console.store(nullptr);

        lock.unlock();

        spdlog::drop(name);

        logger.reset();
        file.reset();
        console.reset();

        lock.lock();

        if (timeout == std::chrono::milliseconds::zero()) {
            log_lifecycle.wait_until_closed(lock);
            return;
        }

        if (!log_lifecycle.wait_until_closed_for(lock, timeout)) {
            throw Exceptions::failed_operation(
                CE_HERE,
                std::format("Timed out while closing logger '{}'.", name).c_str()
            );
        }
    }

    template <const char* name>
    void Log<name>::reopen() {
        if (m_logger.load() == nullptr) {
            construct_log();
        }
        if (log_level != spdlog::level::off && file_level != spdlog::level::off && console_level != spdlog::level::off) {
            m_logger.load()->set_level(log_level);
            m_console->set_level(console_level);
            m_file->set_level(file_level);
        }
    }

    template <const char* name>
    void Log<name>::make_default() const {
        spdlog::info(std::format("Changing default logger from {} to {}",
            spdlog::default_logger()->name(), m_logger.load()->name()));
        set_default_logger(m_logger.load());
        spdlog::info("Default logger set.");
    }

    template <const char* name>
    void Log<name>::set_level_logger(spdlog::level level) const {
        m_logger.load()->set_level(level);
    }

    template <const char* name>
    void Log<name>::set_level_filesink(spdlog::level level) const {
        m_file->set_level(level);
    }

    template <const char* name>
    void Log<name>::set_level_stdsink(spdlog::level level) const {
        m_console->set_level(level);
    }

    template <const char* name>
    template <typename T>
    std::shared_ptr<T> Log<name>::acquire_open_resource(const std::atomic<std::shared_ptr<T>>& resource, const char* operation) const {
        const auto lock = log_lifecycle.lock();
        log_lifecycle.require_state(lock, name, operation, {
            {Open,    StateRequirement::Allow},
            {Closed,  StateRequirement::BadRequest, "it is closed"},
            {Closing, StateRequirement::FailedOperation, "it is closing"},
            {Opening, StateRequirement::FailedOperation, "it is opening"}
        });

        if (auto ptr = resource.load()) {
            return ptr;
        }
        throw Exceptions::failed_operation(CE_HERE,
            std::format("The '{}' logger is open but the requested resource is nullptr.", name));
    }

    // LogLifecycle
    //////////////////

    inline void LogLifecycle::require_state(
        const Lock& lock,
        const char* logger,
        const char* operation,
        std::initializer_list<StateRule> rules
    ) const {
        assert_locked(lock);
        const auto current = state;

        for (const auto& [state, requirement, reason] : rules) {
            if (current != state) {
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
        throw Exceptions::failed_operation(CE_HERE,
            std::format("The '{}' logger has no lifecycle rule for operation '{}'.", logger, operation));
    }

    inline void LogLifecycle::set_state(const Lock& lock, LogState new_state) {
        assert_locked(lock);
        state = new_state;
    }

    inline LogLifecycle::Lock LogLifecycle::lock() const {
        return Lock{mutex};
    }

    inline LogState LogLifecycle::get_state(const Lock& lock) const {
        assert_locked(lock);
        return state;
    }

    inline bool LogLifecycle::is_open(const Lock& lock) const {
        return get_state(lock) == Open;
    }

    inline bool LogLifecycle::is_closed(const Lock& lock) const {
        return get_state(lock) == Closed;
    }

    inline bool LogLifecycle::is_opening(const Lock& lock) const {
        return get_state(lock) == Opening;
    }

    inline bool LogLifecycle::is_closing(const Lock& lock) const {
        return get_state(lock) == Closing;
    }

    inline bool LogLifecycle::is_transitioning(const Lock& lock) const {
        const auto current = get_state(lock);
        return current == Opening || current == Closing;
    }

    inline void LogLifecycle::assert_locked(const Lock& lock) const {
        assert(lock.owns_lock());
        assert(lock.mutex() == &mutex);
    }
}
