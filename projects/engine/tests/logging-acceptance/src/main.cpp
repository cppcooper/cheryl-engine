#include <core/logging/logger.h>
#include <core/logging/log-names.h>
#include <core/logging/process-lifetime.h>
#include <core/subsystems/event-bus.h>
#include <internals/compile-time-logging.hpp>
#include <internals/failure-reporting.h>

#include "../../logging-tests/backend-access.h"

#include <chrono>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <fstream>
#include <future>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

using namespace std::chrono_literals;
namespace Backend = CE::Logging::Testing;

namespace {
    constexpr char owner_name[] = "isolated-owner";
    constexpr char target_name[] = "isolated-target";
    constexpr char static_name[] = "isolated-static";
    constexpr char game_name[] = "app-game";
    constexpr char process_name[] = "process-custom";
    constexpr char process_late_name[] = "process-late";

    void require(const bool condition, const char* message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    CE::Logging::Config configuration(const std::filesystem::path& directory, const bool discard = false) {
        CE::Logging::Config result;
        result.directory = directory;
        result.rotate_on_open = false;
        result.logger_level = result.file_level = CE::Logging::Level::warn;
        result.console_level = CE::Logging::Level::off;
        result.overflow_policy = discard ? CE::Logging::OverflowPolicy::DiscardNew : CE::Logging::OverflowPolicy::Block;
        return result;
    }

    struct RestoreDefault {
        const std::shared_ptr<Backend::NativeLogger> original = Backend::default_logger();

        ~RestoreDefault() {
            try {
                Backend::set_default_logger(original);
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
        CE::Logging::Logger<name>::warn("named-direct category={} phase={}", name, phase);
    }

    template <const char* name> void prepare_named(const std::filesystem::path& directory) {
        CE::Logging::Logger<name>::initialize(CE::Logging::FileEventHandlers{}, configuration(directory));
        CE::Logging::Logger<name>::set_pattern("[%n] [%l] %v");
    }

    template <const char* name> void acquire_named() {
        // First ordinary acquisition retains the normal default directory/profile.
        auto& log = CE::Logging::Logger<name>::get();
        require(log.initial_configuration().directory == std::filesystem::current_path() / "logs", "lazy category changed directory");
        log.set_level_logger(CE::Logging::Level::warn);
        log.set_level_filesink(CE::Logging::Level::warn);
        log.set_level_stdsink(CE::Logging::Level::off);
        log.set_pattern("[%n] [%l] %v");
    }

    void observe_events() {
        CE::Logging::Logger<CE::Logging::Categories::engine>::set_level_logger(CE::Logging::Level::debug);
        CE::Logging::Logger<CE::Logging::Categories::engine>::set_level_filesink(CE::Logging::Level::debug);
        CE::Events::EventBus bus;
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
        CE::Logging::Log<game_name> game{{}, configuration(directory)};
        game.set_pattern("[%n] [%l] %v");
        game.make_default();
        const auto* game_default = Backend::default_logger()->identity();
        Backend::warn("host-game-marker");

        prepare_named<CE::Logging::Categories::engine>(engine_directory);
        require(Backend::default_logger()->identity() == game_default, "explicit engine initialization replaced game default");
        bool repeated = false;
        try {
            CE::Logging::Logger<CE::Logging::Categories::engine>::initialize(
                CE::Logging::FileEventHandlers{}, configuration(engine_directory)
            );
        } catch (const CE::Exceptions::bad_request&) {
            repeated = true;
        }
        require(repeated, "repeated category initialization did not reject");
        acquire_named<CE::Logging::Categories::platform>();
        require(Backend::default_logger()->identity() == game_default, "lazy platform initialization replaced game default");
        prepare_named<CE::Logging::Categories::assets>(engine_directory);
        require(Backend::default_logger()->identity() == game_default, "asset initialization replaced game default");
        named_writes<CE::Logging::Categories::engine>("game-default");
        named_writes<CE::Logging::Categories::platform>("game-default");
        named_writes<CE::Logging::Categories::assets>("game-default");
        observe_events();
        require(Backend::default_logger()->identity() == game_default, "engine observer replaced game default");

        CE::Logging::Logger<CE::Logging::Categories::engine>::make_default();
        require(Backend::default_logger()->name() == CE::Logging::Categories::engine, "engine category did not become explicit default");
        auto native_host = Backend::file_logger("app-native", directory / "app-native.log");
        native_host->set_level(CE::Logging::Level::warn);
        native_host->set_pattern("[%n] [%l] %v");
        Backend::set_default_logger(native_host);
        Backend::warn("host-native-marker");
        prepare_named<CE::Logging::Categories::rendering>(engine_directory);
        require(Backend::default_logger() == native_host, "render initialization replaced native host default");
        acquire_named<CE::Logging::Categories::memory>();
        require(Backend::default_logger() == native_host, "lazy memory initialization replaced native host default");
        named_writes<CE::Logging::Categories::engine>("native-default");
        named_writes<CE::Logging::Categories::platform>("native-default");
        named_writes<CE::Logging::Categories::rendering>("native-default");
        named_writes<CE::Logging::Categories::assets>("native-default");
        named_writes<CE::Logging::Categories::memory>("native-default");
        observe_events();

        // Finish earlier accepted writes before narrowing the async destination gate.
        CE::Logging::Logger<CE::Logging::Categories::assets>::close(2s);
        CE::Logging::Logger<CE::Logging::Categories::assets>::reopen();
        CE::Logging::Logger<CE::Logging::Categories::assets>::set_pattern("[%n] [%l] %v");
        CE::Logging::Logger<CE::Logging::Categories::assets>::set_level_filesink(CE::Logging::Level::err);
        int prepared = 0;
        CE_LOG_WARN(CE::Logging::Categories::assets, "filtered-asset-formatted {}", ++prepared);
        UWARN(CE::Logging::Categories::assets) << "filtered-asset-stream " << ++prepared;
        CE::Logging::Logger<CE::Logging::Categories::assets>::warn("filtered-asset-direct");
        CE_LOG_ERROR(CE::Logging::Categories::assets, "admitted-asset-error");
        require(prepared == 0, "filtered named arguments were evaluated");
        named_writes<CE::Logging::Categories::rendering>("independent-filter");

        CE::Logging::Logger<CE::Logging::Categories::engine>::close(2s);
        require(Backend::default_logger() == native_host, "closing engine category replaced host default");
        named_writes<CE::Logging::Categories::engine>("closed-category");
        CE::Logging::Logger<CE::Logging::Categories::engine>::reopen();
        require(Backend::default_logger() == native_host, "reopening engine category replaced host default");
        CE::Logging::Logger<CE::Logging::Categories::engine>::set_pattern("[%n] [%l] %v");
        named_writes<CE::Logging::Categories::engine>("reopened-category");
        CE::Logging::Logger<CE::Logging::Categories::engine>::close(2s);
        CE::Logging::Logger<CE::Logging::Categories::platform>::close(2s);
        CE::Logging::Logger<CE::Logging::Categories::rendering>::close(2s);
        CE::Logging::Logger<CE::Logging::Categories::assets>::close(2s);
        CE::Logging::Logger<CE::Logging::Categories::memory>::close(2s);
        require(Backend::default_logger() == native_host, "category shutdown replaced host default");
        require(!std::filesystem::exists(engine_directory / "cheryl.log"), "engine routing initialized legacy log");
        native_host->flush();
        game.close(2s);
        std::printf(
            "named-routing compiled-warn=%d compiled-error=%d compiled-debug=%d\n",
            CE::Logging::Compile::enabled(CE::Logging::Compile::WARNING_), CE::Logging::Compile::enabled(CE::Logging::Compile::ERROR_),
            CE::Logging::Compile::enabled(CE::Logging::Compile::DEBUG_)
        );
    }

    class CallbackFormatter final : public Backend::Formatter {
        const std::function<void()> callback_;
        bool first_ = true;

    public:
        explicit CallbackFormatter(std::function<void()> callback)
        : callback_(std::move(callback)) {}

        void format(const std::string_view message, std::string& output) override {
            if (std::exchange(first_, false))
                callback_();
            output.append(message.begin(), message.end());
            output.push_back('\n');
        }

        std::unique_ptr<Backend::Formatter> clone() const override { return std::make_unique<CallbackFormatter>(callback_); }
    };

    void faults() {
        for (const bool nonstandard : {false, true}) {
            for (const bool flush : {false, true}) {
                const auto result = Backend::fault_probe(nonstandard, flush);
                require(result.file_degraded, "failed destination did not degrade");
                require(result.failed_operations == 1, "sink failure was not counted");
                require(result.suppressed_operations == 1, "degraded operation was not suppressed");
            }
        }
    }

    void rotation(const std::filesystem::path& directory) {
        bool fail = true;
        int opens = 0;
        CE::Logging::FileEventHandlers handlers;
        handlers.before_open = [&](const std::filesystem::path&) {
            if (++opens > 1 && fail)
                throw 42;
        };
        auto config = configuration(directory);
        config.rotation_bytes = 16;
        CE::Logging::Log<owner_name> log{handlers, config};
        {
            const auto native = Backend::get(owner_name);
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
        Backend::get(owner_name)->warn("recovered rotation record");
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
        CE::Logging::Log<owner_name> holder{{}, configuration(directory)};
        CE::Logging::Log<target_name> target{{}, configuration(directory, discard)};
        auto native_holder = Backend::get(owner_name);
        auto native_target = Backend::get(target_name);
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
        CE::Logging::Log<owner_name> log{{}, configuration(directory)};
        auto native = Backend::get(owner_name);
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
        auto target = std::make_unique<CE::Logging::Log<target_name>>(CE::Logging::FileEventHandlers{}, configuration(directory));
        CE::Logging::Log<owner_name> holder{{}, configuration(directory)};
        {
            auto native = Backend::get(owner_name);
            native->sinks()[1]->set_formatter(std::make_unique<CallbackFormatter>([&] { target.reset(); }));
            native->warn("fatal-callback-owner-release");
        }
        holder.close(2s);
        throw std::runtime_error("callback destruction did not terminate");
    }

    void process_lifetime(const std::filesystem::path& directory) {
        for (const auto* filename : {"process-custom.log", "engine.log", "isolated-static.log"})
            std::filesystem::remove(directory / filename);
        std::atomic<int> closes{0};
        std::unique_ptr<CE::Logging::Log<process_name>> custom;
        std::weak_ptr<void> pool;
        const auto host_pool = Backend::thread_pool();
        {
            const CE::Logging::ProcessLifetime process;
            CE::Logging::FileEventHandlers handlers;
            handlers.after_close = [&](const std::filesystem::path&) { closes.fetch_add(1); };
            custom = std::make_unique<CE::Logging::Log<process_name>>(handlers, configuration(directory));
            prepare_named<CE::Logging::Categories::engine>(directory);
            // A custom singleton is distinct from both a built-in name and a
            // caller-owned Log, and must release its pool before DLL detach too.
            prepare_named<static_name>(directory);
            pool = Backend::owned_pool();
            {
                const CE::Logging::ProcessLifetime nested;
                custom->warn("process-before-inner-close");
                custom->flush();
            }
            require(closes.load() == 0, "a nested guard closed a live custom logger");
            require(!pool.expired(), "a nested guard stopped the shared worker");
            custom->warn("process-final-custom-record");
            custom->flush();
            CE::Logging::Logger<CE::Logging::Categories::engine>::warn("process-final-engine-record");
            CE::Logging::Logger<static_name>::warn("process-final-singleton-record");
            // No explicit Log::close: the outer guard must drain accepted work.
        }
        require(pool.expired(), "the last guard left a Log or singleton owning the worker pool");
        require(closes.load() == 1, "the last guard did not close the caller-owned logger");
        require(Backend::thread_pool() == host_pool, "process shutdown replaced the host's spdlog pool");
        require(Backend::get(process_name) == nullptr, "process shutdown retained the native custom logger");
        require(custom->shared_queue_stats().queued_items == 0, "terminal diagnostics retained queued work");
        custom->report_diagnostics();
        const auto content = [](const std::filesystem::path& path) {
            std::ifstream file(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
        };
        if constexpr (CE::Logging::Compile::enabled(CE::Logging::Compile::WARNING_)) {
            require(
                content(directory / "process-custom.log").find("process-final-custom-record") != std::string::npos,
                "process shutdown lost a queued custom record or flush"
            );
            require(
                content(directory / "engine.log").find("process-final-engine-record") != std::string::npos,
                "process shutdown lost a queued built-in record"
            );
            require(
                content(directory / "isolated-static.log").find("process-final-singleton-record") != std::string::npos,
                "process shutdown lost a queued custom singleton record"
            );
        }
        bool rejected = false;
        try {
            custom->reopen();
        } catch (const CE::Exceptions::failed_operation&) {
            rejected = true;
        }
        require(rejected, "a logger reopened after terminal process shutdown");
        rejected = false;
        try {
            const CE::Logging::ProcessLifetime later;
        } catch (const CE::Exceptions::failed_operation&) {
            rejected = true;
        }
        require(rejected, "a later process guard restarted terminal logging");
        rejected = false;
        try {
            const CE::Logging::Log<process_late_name> later{{}, configuration(directory)};
        } catch (const CE::Exceptions::failed_operation&) {
            rejected = true;
        }
        require(rejected, "a new logger started after terminal process shutdown");
        custom.reset();
    }
}

int main(const int argc, const char* const* argv) {
    try {
        require(argc == 3, "expected scenario and output directory");
        const std::string_view scenario = argv[1];
        const std::filesystem::path directory = argv[2];
        if (scenario == "process-lifetime") {
            process_lifetime(directory);
            return 0;
        }
        const CE::Logging::ProcessLifetime logging;
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
            CE::Logging::Logger<static_name>::initialize(CE::Logging::FileEventHandlers{}, configuration(directory));
            Backend::get(static_name)->warn("static-final-record");
            // Return normally: the process guard must drain the singleton record.
        } else
            throw std::runtime_error("unknown logging acceptance scenario");
        return 0;
    } catch (...) {
        CE::Diagnostics::report_failure("isolated logging acceptance", std::current_exception());
        return 1;
    }
}
