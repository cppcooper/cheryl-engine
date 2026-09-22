#pragma once
#include "osink.h"
#include <templates/singleton.h>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <mutex>
#include <format>
#include <filesystem>
#include <utility>

namespace CE {
    extern std::string stack_trace(void* addr0 = nullptr);

    template<const char* name>
    class Log {
    protected:
        uint16_t log_id;
        const spdlog::file_event_handlers event_handlers;
        std::shared_ptr<spdlog::async_logger> m_logger;
        std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> m_file;
        std::shared_ptr<osink_mt> m_console;
    private:
        void construct_log();
        spdlog::level log_level = spdlog::level::off;
        spdlog::level file_level = spdlog::level::off;
        spdlog::level console_level = spdlog::level::off;
    public:
        explicit Log(spdlog::file_event_handlers  event_handlers = {});
        [[nodiscard]] std::filesystem::path get_file_path() const;
        [[nodiscard]] uint16_t get_log_id() const;
        void flush() const;
        void close();
        void reopen();
        void make_default() const;
        void set_level_logger(spdlog::level level) const;
        void set_level_filesink(spdlog::level level) const;
        void set_level_stdsink(spdlog::level level) const;

        void set_pattern(const char* fmt) const {
            m_file->set_pattern(fmt);
            m_logger->set_pattern(fmt);
            m_console->set_pattern(fmt);
        }

        void strace(void* addr0 = nullptr) const {
            m_logger->log(spdlog::level::trace, "%s", stack_trace(addr0));
        }

        template<typename... Args>
        void trace(spdlog::format_string_t<Args...> fmt, Args&& ...args) {
            m_logger->log(spdlog::level::trace, fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void debug(spdlog::format_string_t<Args...> fmt, Args&& ...args) {
            m_logger->log(spdlog::level::debug, fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void info(spdlog::format_string_t<Args...> fmt, Args&& ...args) {
            m_logger->log(spdlog::level::info, fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void warn(spdlog::format_string_t<Args...> fmt, Args&& ...args) {
            m_logger->log(spdlog::level::warn, fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void error(spdlog::format_string_t<Args...> fmt, Args&& ...args) {
            m_logger->log(spdlog::level::err, fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void critical(spdlog::format_string_t<Args...> fmt, Args&& ...args) {
            strace();
            m_logger->log(spdlog::level::critical, fmt, std::forward<Args>(args)...);
        }
    };
}

namespace spdlog::CE {
    struct TPInit : Singleton_CTS<TPInit> {
        std::shared_ptr<details::thread_pool> tp;
        TPInit() {
            init_thread_pool(1024 * 1024 * 10, 3);
            tp = thread_pool();
        }
    };
}

// template definitions - methods
namespace CE {
    extern uint16_t log_counter;
    template<const char* name>
    void Log<name>::construct_log() {
        if (m_logger || m_console || m_file) return;
        static std::once_flag tp_flag;
        // todo: unlink the call from type(s) - e.g. extern the flag? simple tp_init singleton
        m_console = std::make_shared<osink_mt>();
        m_file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(std::format("logs/{}.log", name), 1024 * 1024 * 10, 5, true, event_handlers);
        //todo: set pattern/format w/e to prepend the log name to the log lines
        std::vector<spdlog::sink_ptr> sinks{ m_console, m_file };
        m_logger = std::make_shared<spdlog::async_logger>(
                std::format("{}", name), sinks.begin(), sinks.end(),
                spdlog::CE::TPInit::get().tp, spdlog::async_overflow_policy::block
        );
        spdlog::register_logger(m_logger);
    }

    template<const char* name>
    Log<name>::Log(spdlog::file_event_handlers  event_handlers)
            : event_handlers(std::move(event_handlers)) {
        construct_log();
        log_id = ++log_counter;
    }

    template<const char* name>
    std::filesystem::path Log<name>::get_file_path() const {
        if (!m_file) {
            assert(false);
            return {};
        }
        return std::filesystem::absolute(m_file->filename());
    }

    template<const char* name>
    uint16_t Log<name>::get_log_id() const {
        return log_id;
    }

    template<const char* name>
    void Log<name>::flush() const {
        m_file->flush();
        m_console->flush();
        m_logger->flush();
    }

    template<const char* name>
    void Log<name>::close() {
        spdlog::drop(name);
        console_level = m_console->log_level();
        file_level = m_file->log_level();
        log_level = m_logger->log_level();
        // todo: this while loop might stall a program.. better solution required.. maybe not even necessary in any way
        while (spdlog::thread_pool()->queue_size()) { }
        m_logger.reset();
        m_file.reset();
        m_console.reset();
    }

    template<const char* name>
    void Log<name>::reopen() {
        if (m_logger == nullptr) {
            construct_log();
        }
        if (log_level != spdlog::level::off && file_level != spdlog::level::off && console_level != spdlog::level::off) {
            m_logger->set_level(log_level);
            m_console->set_level(console_level);
            m_file->set_level(file_level);
        }
    }

    template<const char* name>
    void Log<name>::make_default() const {
        spdlog::info(std::format("Changing default logger from {} to {}", spdlog::default_logger()->name(), m_logger->name()));
        spdlog::set_default_logger(m_logger);
        spdlog::info("Default logger set.");
    }

    template<const char* name>
    void Log<name>::set_level_logger(spdlog::level level) const {
        m_logger->set_level(level);
    }

    template<const char* name>
    void Log<name>::set_level_filesink(spdlog::level level) const {
        m_file->set_level(level);
    }

    template<const char* name>
    void Log<name>::set_level_stdsink(spdlog::level level) const {
        m_console->set_level(level);
    }
}
