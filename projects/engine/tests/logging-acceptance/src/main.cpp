#include <core/logging/logger.h>
#include <core/logging/log-names.h>
#include <core/subsystems/event-bus.h>
#include <internals/compile-time-logging.hpp>
#include <internals/failure-reporting.h>

#include <spdlog/sinks/basic_file_sink.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

using namespace std::chrono_literals;

namespace {
    constexpr char owner_name[] = "isolated-owner";
    constexpr char target_name[] = "isolated-target";
    constexpr char static_name[] = "isolated-static";
    constexpr char game_name[] = "app-game";

    void require(const bool condition, const char* message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    CE::LogConfig configuration(const std::filesystem::path& directory, const bool discard = false) {
        CE::LogConfig result;
        result.directory = directory;
        result.rotate_on_open = false;
        result.logger_level = result.file_level = spdlog::level::warn;
        result.console_level = spdlog::level::off;
        result.overflow_policy = discard ? CE::LogOverflowPolicy::DiscardNew : CE::LogOverflowPolicy::Block;
        return result;
    }

    struct RestoreDefault {
        const std::shared_ptr<spdlog::logger> original = spdlog::default_logger();

        ~RestoreDefault() {
            try {
                spdlog::set_default_logger(original);
            } catch (...) {
                CE::Diagnostics::report_failure("isolated default restoration", std::current_exception());
            }
        }
    };

    struct RestoreDirectory {
        const std::filesystem::path original = std::filesystem::current_path();

        ~RestoreDirectory() {
            std::error_code ignored;
            std::filesystem::current_path(original, ignored);
        }
    };

    template <const char* name> void named_writes(const std::string_view phase) {
        CE_LOG_WARN(name, "named-formatted category={} phase={}", name, phase);
        UWARN(name) << "named-stream category=" << name << " phase=" << phase;
        CE::Logger<name>::warn("named-direct category={} phase={}", name, phase);
    }

    template <const char* name> void prepare_named(const std::filesystem::path& directory) {
        CE::Logger<name>::initialize(spdlog::file_event_handlers{}, configuration(directory));
        CE::Logger<name>::set_pattern("[%n] [%l] %v");
    }

    template <const char* name> void acquire_named() {
        // First ordinary acquisition retains the normal default directory/profile.
        auto& log = CE::Logger<name>::get();
        require(log.initial_configuration().directory == std::filesystem::current_path() / "logs", "lazy category changed directory");
        log.set_level_logger(spdlog::level::warn);
        log.set_level_filesink(spdlog::level::warn);
        log.set_level_stdsink(spdlog::level::off);
        log.set_pattern("[%n] [%l] %v");
    }

    void observe_events() {
        CE::Logger<CE::enginelog>::set_level_logger(spdlog::level::debug);
        CE::Logger<CE::enginelog>::set_level_filesink(spdlog::level::debug);
        CE::SubSystems::EventBus bus;
        int calls = 0;
        const auto registration = bus.register_listener("isolated-secret-event", [&](std::any) { ++calls; });
        bus.dispatch("isolated-secret-event", "isolated-secret-payload");
        require(calls == 1, "representative engine event did not run");
        bus.report_diagnostics();
        require(bus.unregister_and_wait(registration), "representative listener did not unregister");
    }

    void named_routing(const std::filesystem::path& supplied_directory) {
        RestoreDirectory restore_directory;
        RestoreDefault restore_default;
        const auto directory = std::filesystem::absolute(supplied_directory);
        std::filesystem::current_path(directory);
        const auto engine_directory = directory / "logs";
        CE::Log<game_name> game{{}, configuration(directory)};
        game.set_pattern("[%n] [%l] %v");
        game.make_default();
        const auto* game_default = spdlog::default_logger().get();
        spdlog::warn("host-game-marker");

        prepare_named<CE::enginelog>(engine_directory);
        require(spdlog::default_logger().get() == game_default, "explicit engine initialization replaced game default");
        bool repeated = false;
        try {
            CE::Logger<CE::enginelog>::initialize(spdlog::file_event_handlers{}, configuration(engine_directory));
        } catch (const CE::Exceptions::bad_request&) {
            repeated = true;
        }
        require(repeated, "repeated category initialization did not reject");
        acquire_named<CE::platformlog>();
        require(spdlog::default_logger().get() == game_default, "lazy platform initialization replaced game default");
        prepare_named<CE::assetlog>(engine_directory);
        require(spdlog::default_logger().get() == game_default, "asset initialization replaced game default");
        named_writes<CE::enginelog>("game-default");
        named_writes<CE::platformlog>("game-default");
        named_writes<CE::assetlog>("game-default");
        observe_events();
        require(spdlog::default_logger().get() == game_default, "engine observer replaced game default");

        CE::Logger<CE::enginelog>::make_default();
        require(spdlog::default_logger()->name() == CE::enginelog, "engine category did not become explicit default");
        auto native_host = std::make_shared<spdlog::logger>(
            "app-native", std::make_shared<spdlog::sinks::basic_file_sink_mt>((directory / "app-native.log").string(), true)
        );
        native_host->set_level(spdlog::level::warn);
        native_host->set_pattern("[%n] [%l] %v");
        spdlog::set_default_logger(native_host);
        spdlog::warn("host-native-marker");
        prepare_named<CE::renderlog>(engine_directory);
        require(spdlog::default_logger() == native_host, "render initialization replaced native host default");
        acquire_named<CE::memlog>();
        require(spdlog::default_logger() == native_host, "lazy memory initialization replaced native host default");
        named_writes<CE::enginelog>("native-default");
        named_writes<CE::platformlog>("native-default");
        named_writes<CE::renderlog>("native-default");
        named_writes<CE::assetlog>("native-default");
        named_writes<CE::memlog>("native-default");
        observe_events();

        // Finish earlier accepted writes before narrowing the async destination gate.
        CE::Logger<CE::assetlog>::close(2s);
        CE::Logger<CE::assetlog>::reopen();
        CE::Logger<CE::assetlog>::set_pattern("[%n] [%l] %v");
        CE::Logger<CE::assetlog>::set_level_filesink(spdlog::level::err);
        int prepared = 0;
        CE_LOG_WARN(CE::assetlog, "filtered-asset-formatted {}", ++prepared);
        UWARN(CE::assetlog) << "filtered-asset-stream " << ++prepared;
        CE::Logger<CE::assetlog>::warn("filtered-asset-direct");
        CE_LOG_ERROR(CE::assetlog, "admitted-asset-error");
        require(prepared == 0, "filtered named arguments were evaluated");
        named_writes<CE::renderlog>("independent-filter");

        CE::Logger<CE::enginelog>::close(2s);
        require(spdlog::default_logger() == native_host, "closing engine category replaced host default");
        named_writes<CE::enginelog>("closed-category");
        CE::Logger<CE::enginelog>::reopen();
        require(spdlog::default_logger() == native_host, "reopening engine category replaced host default");
        CE::Logger<CE::enginelog>::set_pattern("[%n] [%l] %v");
        named_writes<CE::enginelog>("reopened-category");
        CE::Logger<CE::enginelog>::close(2s);
        CE::Logger<CE::platformlog>::close(2s);
        CE::Logger<CE::renderlog>::close(2s);
        CE::Logger<CE::assetlog>::close(2s);
        CE::Logger<CE::memlog>::close(2s);
        require(spdlog::default_logger() == native_host, "category shutdown replaced host default");
        require(!std::filesystem::exists(engine_directory / "cheryl.log"), "engine routing initialized legacy log");
        native_host->flush();
        game.close(2s);
        std::printf(
            "named-routing compiled-warn=%d compiled-error=%d compiled-debug=%d\n", ctlog::enabled(ctlog::WARNING_),
            ctlog::enabled(ctlog::ERROR_), ctlog::enabled(ctlog::DEBUG_)
        );
    }

    class CallbackFormatter final : public spdlog::formatter {
        const std::function<void()> callback_;
        bool first_ = true;

    public:
        explicit CallbackFormatter(std::function<void()> callback)
        : callback_(std::move(callback)) {}

        void format(const spdlog::details::log_msg& message, spdlog::memory_buf_t& output) override {
            if (std::exchange(first_, false))
                callback_();
            output.append(message.payload.begin(), message.payload.end());
            output.push_back('\n');
        }

        std::unique_ptr<spdlog::formatter> clone() const override { return std::make_unique<CallbackFormatter>(callback_); }
    };

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

    void faults() {
        for (const bool nonstandard : {false, true}) {
            for (const bool flush : {false, true}) {
                auto counters = std::make_shared<CE::LogDetail::BackendCounters>();
                auto state = std::make_shared<CE::LogDetail::DestinationState>(counters, true);
                CE::LogDetail::GuardedSink guard{std::make_shared<FailingSink>(nonstandard), state};
                if (flush)
                    guard.flush();
                else
                    guard.log(spdlog::details::log_msg{"isolated", spdlog::level::warn, "failure"});
                require(state->degraded(), "failed destination did not degrade");
                require(counters->failed_operations == 1, "sink failure was not counted");
                guard.flush(); // A degraded destination must never be entered again.
                require(counters->suppressed_operations == 1, "degraded operation was not suppressed");
            }
        }
    }

    void rotation(const std::filesystem::path& directory) {
        bool fail = true;
        int opens = 0;
        spdlog::file_event_handlers handlers;
        handlers.before_open = [&](const spdlog::filename_t&) {
            if (++opens > 1 && fail)
                throw 42;
        };
        auto config = configuration(directory);
        config.rotation_bytes = 16;
        CE::Log<owner_name> log{handlers, config};
        {
            const auto native = spdlog::get(owner_name);
            native->warn("first large rotation record");
            native->warn("second large rotation record");
            native->flush(); // Must skip a file left closed by the failed rotation.
        }
        log.close(2s);
        require(log.backend_stats().failed_operations == 1, "rotation failure was not contained");
        require(log.backend_stats().callback_failures == 1, "rotation callback failure was not counted");
        require(log.backend_stats().file_degraded, "failed rotation did not degrade the generation");
        fail = false;
        log.reopen();
        require(!log.backend_stats().file_degraded, "reopen did not replace degraded destinations");
        spdlog::get(owner_name)->warn("recovered rotation record");
        log.close(2s);
    }

    struct Release {
        std::promise<void>& promise;
        bool released = false;

        void release() {
            if (!std::exchange(released, true))
                promise.set_value();
        }
        ~Release() { release(); }
    };

    void full_reentry(const std::filesystem::path& directory, const bool discard) {
        CE::Log<owner_name> holder{{}, configuration(directory)};
        CE::Log<target_name> target{{}, configuration(directory, discard)};
        auto native_holder = spdlog::get(owner_name);
        auto native_target = spdlog::get(target_name);
        std::promise<void> entered;
        auto ready = entered.get_future();
        std::promise<void> released;
        auto release = released.get_future();
        int rejected = 0;
        native_holder->sinks()[1]->set_formatter(std::make_unique<CallbackFormatter>([&] {
            entered.set_value();
            release.wait();
            native_target->warn("recursive-full-queue");
            try {
                target.close(1ms);
            } catch (const CE::Exceptions::bad_request&) {
                ++rejected;
            }
            try {
                native_target->flush();
            } catch (const CE::Exceptions::bad_request&) {
                ++rejected;
            }
        }));
        // Release precedes logger cleanup even when an observation fails.
        Release unblock{released};
        native_holder->warn("held-backend-record");
        require(ready.wait_for(2s) == std::future_status::ready, "backend did not enter formatter");
        const auto baseline = target.shared_queue_stats().discarded_items;
        for (std::size_t index = 0; index < 8192; ++index)
            native_target->warn("accepted[{}]", index);
        require(target.shared_queue_stats().queued_items == 8192, "shared queue did not fill");
        if (discard) {
            native_target->warn("discarded-marker");
            native_target->flush();
            require(target.shared_queue_stats().discarded_items == baseline + 2, "record/flush loss was not counted");
        }
        unblock.release();
        native_holder.reset();
        holder.close(2s);
        native_target.reset();
        target.close(2s);
        require(rejected == 2, "backend lifecycle reentry was not rejected");
        require(holder.backend_stats().recursive_submissions == 1, "native recursive write was not suppressed");
    }

    void retained(const std::filesystem::path& directory) {
        CE::Log<owner_name> log{{}, configuration(directory)};
        auto native = spdlog::get(owner_name);
        auto clone = native->clone("isolated-clone");
        clone->warn("retained-clone-record");
        native.reset();
        bool timed_out = false;
        try {
            log.close(1ms);
        } catch (const CE::Exceptions::failed_operation&) {
            timed_out = true;
        }
        require(timed_out, "retained clone did not preserve the close boundary");
        clone.reset();
        log.close(2s);
    }

    void fatal_destruction(const std::filesystem::path& directory) {
        std::set_terminate([] { std::_Exit(86); });
        auto target = std::make_unique<CE::Log<target_name>>(spdlog::file_event_handlers{}, configuration(directory));
        CE::Log<owner_name> holder{{}, configuration(directory)};
        {
            auto native = spdlog::get(owner_name);
            native->sinks()[1]->set_formatter(std::make_unique<CallbackFormatter>([&] { target.reset(); }));
            native->warn("fatal-callback-owner-release");
        }
        holder.close(2s);
        throw std::runtime_error("callback destruction did not terminate");
    }
}

int main(const int argc, const char* const* argv) {
    try {
        require(argc == 3, "expected scenario and output directory");
        const std::string_view scenario = argv[1];
        const std::filesystem::path directory = argv[2];
        if (scenario == "faults")
            faults();
        else if (scenario == "rotation")
            rotation(directory);
        else if (scenario == "full-reentry")
            full_reentry(directory, false);
        else if (scenario == "discard")
            full_reentry(directory, true);
        else if (scenario == "retained")
            retained(directory);
        else if (scenario == "named-routing")
            named_routing(directory);
        else if (scenario == "fatal-destruction")
            fatal_destruction(directory);
        else if (scenario == "static") {
            CE::Logger<static_name>::initialize(spdlog::file_event_handlers{}, configuration(directory));
            spdlog::get(static_name)->warn("static-final-record");
            // Return normally: the singleton/pool teardown must drain this record.
        } else
            throw std::runtime_error("unknown logging acceptance scenario");
        return 0;
    } catch (...) {
        CE::Diagnostics::report_failure("isolated logging acceptance", std::current_exception());
        return 1;
    }
}
