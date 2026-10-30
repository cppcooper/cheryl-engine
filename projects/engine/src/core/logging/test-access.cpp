#if defined(CHERYL_LOGGING_TEST_HOOKS)

#include "../../../tests/logging-tests/backend-access.h"

#include "backend-log.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <backward.hpp>

#include <unordered_map>

namespace CE::Logging::Testing {
    namespace {
        std::shared_ptr<NativeLogger> wrap(const std::shared_ptr<spdlog::logger>& logger) {
            if (!logger)
                return {};
            static std::mutex mutex;
            static std::unordered_map<const void*, std::weak_ptr<NativeLogger>> wrappers;
            const std::lock_guard lock(mutex);
            auto& slot = wrappers[logger.get()];
            if (auto existing = slot.lock())
                return existing;
            auto result = std::make_shared<NativeLogger>(logger);
            slot = result;
            return result;
        }

        class FormatBridge final : public spdlog::formatter {
            const std::unique_ptr<Formatter> formatter_;

        public:
            explicit FormatBridge(std::unique_ptr<Formatter> formatter)
            : formatter_(std::move(formatter)) {}
            void format(const spdlog::details::log_msg& message, spdlog::memory_buf_t& destination) override {
                std::string output;
                formatter_->format(std::string_view{message.payload.data(), message.payload.size()}, output);
                destination.append(output.begin(), output.end());
            }
            std::unique_ptr<spdlog::formatter> clone() const override { return std::make_unique<FormatBridge>(formatter_->clone()); }
        };
    }

    struct Access {
        static std::shared_ptr<spdlog::logger> native(const NativeLogger& handle) {
            return std::static_pointer_cast<spdlog::logger>(handle.native_);
        }
        static std::shared_ptr<spdlog::sinks::sink> native(const Sink& handle) {
            return std::static_pointer_cast<spdlog::sinks::sink>(handle.native_);
        }
    };

    Formatter::~Formatter() = default;
    Sink::Sink(std::shared_ptr<void> native)
    : native_(std::move(native)) {}
    Level Sink::log_level() const {
        const auto sink = std::static_pointer_cast<spdlog::sinks::sink>(native_);
        if (const auto guarded = std::dynamic_pointer_cast<Detail::GuardedSink>(sink))
            return static_cast<Level>(guarded->delegate_level());
        return static_cast<Level>(sink->log_level());
    }
    void Sink::set_formatter(std::unique_ptr<Formatter> formatter) {
        std::static_pointer_cast<spdlog::sinks::sink>(native_)->set_formatter(std::make_unique<FormatBridge>(std::move(formatter)));
    }

    NativeLogger::NativeLogger(std::shared_ptr<void> native)
    : native_(std::move(native)) {}
    const void* NativeLogger::identity() const noexcept {
        return native_.get();
    }
    std::string NativeLogger::name() const {
        return Access::native(*this)->name();
    }
    Level NativeLogger::log_level() const {
        return static_cast<Level>(Access::native(*this)->log_level());
    }
    void NativeLogger::set_level(const Level level) {
        Access::native(*this)->set_level(static_cast<spdlog::level>(level));
    }
    void NativeLogger::set_pattern(const std::string& pattern) {
        Access::native(*this)->set_pattern(pattern);
    }
    void NativeLogger::flush() {
        Access::native(*this)->flush();
    }
    void NativeLogger::log(const Level level, const std::string_view message) {
        Access::native(*this)->log(static_cast<spdlog::level>(level), "{}", message);
    }
    std::shared_ptr<NativeLogger> NativeLogger::clone(const std::string& name) {
        return wrap(Access::native(*this)->clone(name));
    }
    std::vector<std::shared_ptr<Sink>> NativeLogger::sinks() const {
        std::vector<std::shared_ptr<Sink>> result;
        for (const auto& sink : Access::native(*this)->sinks())
            result.push_back(std::make_shared<Sink>(sink));
        return result;
    }
    void NativeLogger::restore_sinks(const std::vector<std::shared_ptr<Sink>>& graph) {
        std::vector<spdlog::sink_ptr> sinks;
        sinks.reserve(graph.size());
        for (const auto& sink : graph)
            sinks.push_back(Access::native(*sink));
        Access::native(*this)->sinks() = std::move(sinks);
    }
    void NativeLogger::append_null_sink() {
        Access::native(*this)->sinks().push_back(std::make_shared<spdlog::sinks::null_sink_mt>());
    }

    std::shared_ptr<NativeLogger> get(const std::string_view name) {
        return wrap(spdlog::get(std::string{name}));
    }
    std::shared_ptr<NativeLogger> default_logger() {
        return wrap(spdlog::default_logger());
    }
    void set_default_logger(const std::shared_ptr<NativeLogger>& logger) {
        spdlog::set_default_logger(logger ? Access::native(*logger) : nullptr);
    }
    std::shared_ptr<NativeLogger> file_logger(const std::string& name, const std::filesystem::path& file) {
        return wrap(std::make_shared<spdlog::logger>(name, std::make_shared<spdlog::sinks::basic_file_sink_mt>(file.string(), true)));
    }
    std::shared_ptr<void> thread_pool() {
        return spdlog::thread_pool();
    }
    std::shared_ptr<void> owned_pool() {
        return Detail::ThreadPoolInit::get().tp;
    }
    std::shared_ptr<void> make_pool() {
        return Detail::ThreadPoolInit{}.tp;
    }

    void console_probe(const std::string_view output, const std::string_view error) {
        spdlog::logger console{"console", std::make_shared<spdlog::sinks::stdout_color_sink_mt>()};
        spdlog::logger errors{"stderr", std::make_shared<spdlog::sinks::stderr_color_sink_mt>()};
        console.set_pattern("%v");
        errors.set_pattern("%v");
        console.info("{}", output);
        errors.error("{}", error);
    }

    std::size_t print_stack() {
        backward::Printer printer;
        backward::StackTrace stack;
        stack.load_here(6);
        printer.object = true;
        printer.address = true;
        printer.reverse = false;
        printer.snippet = false;
        printer.print(stack, stderr);
        return stack.size();
    }

    BackendStats fault_probe(const bool nonstandard, const bool flush) {
        class FailingSink final : public spdlog::sinks::sink {
            const bool nonstandard_;
            void fail() const {
                if (nonstandard_)
                    throw 42;
                throw std::runtime_error("isolated sink failure");
            }

        public:
            explicit FailingSink(const bool nonstandard)
            : nonstandard_(nonstandard) {}
            void log(const spdlog::details::log_msg&) override { fail(); }
            void flush() override { fail(); }
            void set_pattern(const std::string&) override {}
            void set_formatter(std::unique_ptr<spdlog::formatter>) override {}
        };
        const auto counters = std::make_shared<Detail::BackendCounters>();
        const auto state = std::make_shared<Detail::DestinationState>(counters, true);
        Detail::GuardedSink guard{std::make_shared<FailingSink>(nonstandard), state};
        if (flush)
            guard.flush();
        else
            guard.log(spdlog::details::log_msg{"isolated", spdlog::level::warn, "failure"});
        guard.flush();
        return {
            counters->failed_operations.load(), counters->callback_failures.load(), counters->suppressed_operations.load(),
            state->degraded()
        };
    }
}

#endif
