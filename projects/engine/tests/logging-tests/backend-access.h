#pragma once

#include <cheryl/core/logging/log.h>

#include <functional>
#include <vector>

namespace CE::Logging::Testing {
    // Test-only handles execute every native backend operation inside Engine.
    // The test runner never links or instantiates another spdlog implementation.
    class CE_TYPE_API Formatter {
    public:
        CE_API virtual ~Formatter();
        virtual void format(std::string_view payload, std::string& output) = 0;
        [[nodiscard]] virtual std::unique_ptr<Formatter> clone() const = 0;
    };

    class Sink {
        std::shared_ptr<void> native_;
        friend struct Access;

    public:
        CE_API explicit Sink(std::shared_ptr<void> native);
        [[nodiscard]] CE_API Level log_level() const;
        CE_API void set_formatter(std::unique_ptr<Formatter> formatter);
    };

    class NativeLogger {
        std::shared_ptr<void> native_;
        friend struct Access;

    public:
        CE_API explicit NativeLogger(std::shared_ptr<void> native);
        [[nodiscard]] CE_API const void* identity() const noexcept;
        [[nodiscard]] CE_API std::string name() const;
        [[nodiscard]] CE_API Level log_level() const;
        CE_API void set_level(Level level);
        CE_API void set_pattern(const std::string& pattern);
        CE_API void flush();
        CE_API void log(Level level, std::string_view message);
        [[nodiscard]] CE_API std::shared_ptr<NativeLogger> clone(const std::string& name);
        [[nodiscard]] CE_API std::vector<std::shared_ptr<Sink>> sinks() const;
        CE_API void restore_sinks(const std::vector<std::shared_ptr<Sink>>& graph);
        CE_API void append_null_sink();

        template <typename... Args> void warn(std::format_string<Args...> format, Args&&... arguments) {
            log(Level::warn, std::format(format, std::forward<Args>(arguments)...));
        }
    };

    [[nodiscard]] CE_API std::shared_ptr<NativeLogger> get(std::string_view name);
    [[nodiscard]] CE_API std::shared_ptr<NativeLogger> default_logger();
    CE_API void set_default_logger(const std::shared_ptr<NativeLogger>& logger);
    [[nodiscard]] CE_API std::shared_ptr<NativeLogger> file_logger(const std::string& name, const std::filesystem::path& file);
    [[nodiscard]] CE_API std::shared_ptr<void> thread_pool();
    [[nodiscard]] CE_API std::shared_ptr<void> owned_pool();
    [[nodiscard]] CE_API std::shared_ptr<void> make_pool();
    [[nodiscard]] CE_API BackendStats fault_probe(bool nonstandard, bool flush);
    CE_API void console_probe(std::string_view output, std::string_view error);
    [[nodiscard]] CE_API std::size_t print_stack();

    template <typename... Args> void warn(std::format_string<Args...> format, Args&&... arguments) {
        if (const auto logger = default_logger())
            logger->warn(format, std::forward<Args>(arguments)...);
    }
}
