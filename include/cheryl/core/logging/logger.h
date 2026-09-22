#pragma once
#include "log.h"
#include <templates/singleton.h>
#include <string>

namespace CE {
    template<const char* name>
    class Logger : public Singleton_CTS<Log<name>> {
    protected:
        explicit Logger(const spdlog::file_event_handlers& event_handlers = {}) {
            Singleton_CTS<Log<name>>::get(event_handlers);
        }

    public:
        static void set_pattern(const char* fmt) { Singleton_CTS<Log<name>>::get().set_pattern(fmt); }
        [[nodiscard]]
        static std::filesystem::path get_file_path() { return Singleton_CTS<Log<name>>::get().get_file_path(); };

        static void flush() { Singleton_CTS<Log<name>>::get().flush(); }

        static void close() { Singleton_CTS<Log<name>>::get().close(); }

        static void reopen() { Singleton_CTS<Log<name>>::get().reopen(); }

        static void make_default() { Singleton_CTS<Log<name>>::get().make_default(); }

        static void set_level_logger(spdlog::level level) { Singleton_CTS<Log<name>>::get().set_level_logger(level); }

        static void set_level_filesink(spdlog::level level) { Singleton_CTS<Log<name>>::get().set_level_filesink(level); }

        static void set_level_stdsink(spdlog::level level) { Singleton_CTS<Log<name>>::get().set_level_stdsink(level); }

        template<typename... Args>
        static void trace(spdlog::format_string_t<Args...> fmt, Args&& ...args) {
            Singleton_CTS<Log<name>>::get().trace(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void debug(spdlog::format_string_t<Args...> fmt, Args&& ...args) {
            Singleton_CTS<Log<name>>::get().debug(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void info(spdlog::format_string_t<Args...> fmt, Args&& ...args) {
            Singleton_CTS<Log<name>>::get().info(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void warn(spdlog::format_string_t<Args...> fmt, Args&& ...args) {
            Singleton_CTS<Log<name>>::get().warn(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void error(spdlog::format_string_t<Args...> fmt, Args&& ...args) {
            Singleton_CTS<Log<name>>::get().error(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void critical(spdlog::format_string_t<Args...> fmt, Args&& ...args) {
            Singleton_CTS<Log<name>>::get().critical(fmt, std::forward<Args>(args)...);
        }

        static void strace(void* addr0 = nullptr) {
            Singleton_CTS<Log<name>>::get().strace(addr0);
        }
    };
}


