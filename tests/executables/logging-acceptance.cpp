#include <core/logging/logger.h>
#include <internals/failure-reporting.h>

#include <chrono>
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
