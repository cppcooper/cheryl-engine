#include <gtest/gtest.h>

#include <core/logging.h>
#include <internals/compile-time-logging.hpp>
#include <internals/exceptions.h>
#include <spdlog/formatter.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <latch>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace CE;
using namespace std::chrono_literals;

namespace {
    namespace fs = std::filesystem;

    constexpr auto close_timeout = 2s;

    constexpr char file_barrier_name[] = "logging-file-barrier";
    constexpr char console_routing_name[] = "logging-console-routing";
    constexpr char destructor_close_name[] = "logging-destructor-close";
    constexpr char closed_state_name[] = "logging-closed-state";
    constexpr char reopen_levels_name[] = "logging-reopen-levels";
    constexpr char timed_close_name[] = "logging-timed-close";
    constexpr char opening_state_name[] = "logging-opening-state";
    constexpr char failed_reopen_name[] = "logging-failed-reopen";
    constexpr char default_restore_name[] = "logging-default-restore";
    constexpr char default_replaced_name[] = "logging-default-replaced";
    constexpr char alternate_default_name[] = "logging-alternate-default";
    constexpr char wrapper_name[] = "logging-wrapper";
    constexpr char filtered_args_name[] = "logging-filtered-args";
    constexpr char compiled_args_name[] = "logging-compiled-args";
    constexpr char lazy_failure_name[] = "logging-lazy-failure";
    constexpr char defaults_name[] = "logging-defaults";
    constexpr char config_name[] = "logging-config";
    constexpr char bad_config_name[] = "logging-bad-config";
    constexpr char bad_path_name[] = "logging/invalid-name";
    constexpr char queue_hold_name[] = "logging-queue-hold";
    constexpr char discard_name[] = "logging-discard";
    constexpr char blocking_name[] = "logging-blocking";
    constexpr char callback_failure_name[] = "logging-callback-failure";
    constexpr char startup_failure_name[] = "logging-startup-failure";
    constexpr char formatter_failure_name[] = "logging-formatter-failure";
    constexpr char console_failure_name[] = "logging-console-failure";
    constexpr std::size_t queue_capacity = 8192;

    struct LogDirectory {
        fs::path path = fs::temp_directory_path() /
                        ("cheryl-log-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

        LogDirectory() { fs::create_directories(path); }
        ~LogDirectory() {
            std::error_code error;
            fs::remove_all(path, error);
        }
    };

    struct CurrentDirectoryGuard {
        fs::path original = fs::current_path();

        ~CurrentDirectoryGuard() {
            std::error_code error;
            fs::current_path(original, error);
        }
    };

    void remove_current_log_file(const char* name) {
        std::error_code error;
        fs::remove(fs::path{"logs"} / std::format("{}.log", name), error);
    }

    std::string read_file(const fs::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            ADD_FAILURE() << "Failed to open log file: " << path;
            return {};
        }

        return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    }

    void expect_contains(const std::string& text, const std::string& fragment) {
        EXPECT_NE(text.find(fragment), std::string::npos) << "Expected log output to contain: " << fragment;
    }

    void expect_level(const std::string& text, const std::string& fragment, const ctlog::LogLevel level) {
        if (ctlog::enabled(level))
            expect_contains(text, fragment);
        else
            EXPECT_EQ(text.find(fragment), std::string::npos);
    }

    template <const char* name> class TestLog final : public Log<name> {
    public:
        explicit TestLog(spdlog::file_event_handlers event_handlers = {}, LogConfig config = LogConfig::for_logger(name))
        : Log<name>(std::move(event_handlers), std::move(config)) {}

        [[nodiscard]] std::shared_ptr<spdlog::logger> retain_logger() const { return this->m_logger.load(); }

        [[nodiscard]] spdlog::level logger_level() const { return this->m_logger.load()->log_level(); }

        [[nodiscard]] spdlog::level file_level() const { return this->m_file.load()->log_level(); }

        [[nodiscard]] spdlog::level console_level() const { return this->m_console.load()->log_level(); }
    };

    struct BackendGate {
        std::promise<void> entered;
        std::future<void> ready = entered.get_future();
        std::latch released{1};
        std::atomic<bool> unblocked = false;

        void unblock() noexcept {
            if (!unblocked.exchange(true))
                released.count_down();
        }
    };

    struct BackendRelease {
        std::shared_ptr<BackendGate> gate;

        ~BackendRelease() { gate->unblock(); }
    };

    class HeldSink final : public spdlog::sinks::sink {
        std::shared_ptr<BackendGate> gate;
        bool first = true;

    public:
        explicit HeldSink(std::shared_ptr<BackendGate> gate)
        : gate(std::move(gate)) {}

        void log(const spdlog::details::log_msg&) override {
            if (first) {
                first = false;
                gate->entered.set_value();
                gate->released.wait();
            }
        }

        void flush() override {}
        void set_pattern(const std::string&) override {}
        void set_formatter(std::unique_ptr<spdlog::formatter>) override {}
    };

    class FailingFormatter final : public spdlog::formatter {
        const bool nonstandard;

    public:
        explicit FailingFormatter(bool nonstandard)
        : nonstandard(nonstandard) {}

        void format(const spdlog::details::log_msg&, spdlog::memory_buf_t&) override {
            if (nonstandard)
                throw 42;
            throw std::runtime_error("injected formatter failure");
        }

        std::unique_ptr<spdlog::formatter> clone() const override { return std::make_unique<FailingFormatter>(nonstandard); }
    };

    LogConfig queue_config(const char* name, const fs::path& directory) {
        auto config = LogConfig::for_logger(name);
        config.directory = directory;
        config.rotate_on_open = false;
        config.logger_level = config.file_level = spdlog::level::warn;
        config.console_level = spdlog::level::off;
        return config;
    }

    void expect_sequence(const std::string& content, const int generation) {
        std::size_t cursor = 0;
        for (std::size_t i = 0; i < queue_capacity; ++i) {
            const auto marker = std::format("accepted[{}:{}]", generation, i);
            const auto position = content.find(marker, cursor);
            ASSERT_NE(position, std::string::npos) << "Missing or reordered marker: " << marker;
            cursor = position + marker.size();
        }
    }

    class DefaultLoggerGuard final {
        std::shared_ptr<spdlog::logger> original = spdlog::default_logger();
        bool restored = false;

    public:
        void restore() {
            if (restored) {
                return;
            }

            spdlog::set_default_logger(original);
            restored = true;
        }

        ~DefaultLoggerGuard() {
            try {
                restore();
            } catch (...) {
                // Never allow global test cleanup to terminate the test process.
            }
        }
    };
}

TEST(logging, close_completes_file) {
    remove_current_log_file(file_barrier_name);

    std::atomic<int> close_events = 0;
    spdlog::file_event_handlers handlers;
    handlers.after_close = [&close_events](const spdlog::filename_t&) { ++close_events; };

    TestLog<file_barrier_name> log{handlers};
    const auto path = log.get_file_path();

    // Configure deterministic file-only output, then exercise every ordinary severity path.
    log.set_pattern("%v");
    log.set_level_logger(spdlog::level::trace);
    log.set_level_filesink(spdlog::level::trace);
    log.set_level_stdsink(spdlog::level::off);
    log.trace("trace-message");
    log.debug("debug-message");
    log.info("info-message");
    log.warn("warn-message");
    log.error("error-message");
    log.critical("critical-message");

    // Successful close is the synchronization boundary: the sink close event has happened and
    // the file can be inspected immediately, without a queue poll or arbitrary sleep.
    EXPECT_EQ(close_events.load(), 0);
    EXPECT_NO_THROW(log.close());
    EXPECT_EQ(close_events.load(), 1);

    const auto content = read_file(path);
    expect_level(content, "trace-message", ctlog::TRACE_);
    expect_level(content, "debug-message", ctlog::DEBUG_);
    expect_level(content, "info-message", ctlog::INFO_);
    expect_level(content, "warn-message", ctlog::WARNING_);
    expect_level(content, "error-message", ctlog::ERROR_);
    expect_level(content, "critical-message", ctlog::FATAL_);

    // Closing an already closed logger is idempotent and must not destroy the sink twice.
    EXPECT_NO_THROW(log.close());
    EXPECT_EQ(close_events.load(), 1);
}

TEST(logging, destructor_closes_file) {
    remove_current_log_file(destructor_close_name);

    std::atomic<int> close_events = 0;
    spdlog::file_event_handlers handlers;
    handlers.after_close = [&close_events](const spdlog::filename_t&) { ++close_events; };

    DefaultLoggerGuard default_guard;
    fs::path path;
    {
        TestLog<destructor_close_name> log{handlers};
        path = log.get_file_path();
        log.set_pattern("%v");
        log.set_level_stdsink(spdlog::level::off);
        log.make_default();
        log.info("destructor-close-message");

        EXPECT_EQ(close_events.load(), 0);
        EXPECT_NE(spdlog::get(destructor_close_name), nullptr);
    }

    // Leaving scope invokes Log's destructor. Under ordinary ownership it must reach the same
    // completed file-close boundary as close(), then remove both real and fallback registrations.
    EXPECT_EQ(close_events.load(), 1);
    EXPECT_EQ(spdlog::get(destructor_close_name), nullptr);
    EXPECT_EQ(spdlog::get(std::format("{}-closed", destructor_close_name)), nullptr);
    EXPECT_EQ(spdlog::default_logger(), nullptr);
    expect_level(read_file(path), "destructor-close-message", ctlog::INFO_);

    default_guard.restore();
}

TEST(logging, console_routing) {
    remove_current_log_file(console_routing_name);
    TestLog<console_routing_name> log;

    // Suppress file output so this test isolates osink routing. close() then provides the async
    // completion barrier before either captured stream is inspected.
    log.set_pattern("%v");
    log.set_level_logger(spdlog::level::trace);
    log.set_level_filesink(spdlog::level::off);
    log.set_level_stdsink(spdlog::level::trace);

    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    log.info("stdout-info");
    log.warn("stdout-warn");
    log.error("stderr-error");
    log.close();
    std::cout.flush();
    std::cerr.flush();
    const auto stdout_text = testing::internal::GetCapturedStdout();
    const auto stderr_text = testing::internal::GetCapturedStderr();

    expect_level(stdout_text, "stdout-info", ctlog::INFO_);
    expect_level(stdout_text, "stdout-warn", ctlog::WARNING_);
    expect_level(stderr_text, "stderr-error", ctlog::ERROR_);
    EXPECT_EQ(stdout_text.find("stderr-error"), std::string::npos);
    EXPECT_EQ(stderr_text.find("stdout-info"), std::string::npos);
    EXPECT_EQ(stderr_text.find("stdout-warn"), std::string::npos);
}

TEST(logging, closed_state) {
    remove_current_log_file(closed_state_name);
    TestLog<closed_state_name> log;

    log.close();

    // Ordinary writes are intentionally lifecycle-insensitive: Closed routes them through the
    // null logger instead of throwing, including the stack-trace path used by critical().
    EXPECT_NO_THROW(log.trace("discarded-trace"));
    EXPECT_NO_THROW(log.debug("discarded-debug"));
    EXPECT_NO_THROW(log.info("discarded-info"));
    EXPECT_NO_THROW(log.warn("discarded-warn"));
    EXPECT_NO_THROW(log.error("discarded-error"));
    EXPECT_NO_THROW(log.critical("discarded-critical"));
    EXPECT_NO_THROW(log.strace());

    // Operations that require the real logger or sinks retain explicit Closed-state failures.
    EXPECT_THROW(log.flush(), Exceptions::bad_request);
    EXPECT_THROW(static_cast<void>(log.get_file_path()), Exceptions::bad_request);
    EXPECT_THROW(log.make_default(), Exceptions::bad_request);
    EXPECT_THROW(log.set_pattern("%v"), Exceptions::bad_request);
    EXPECT_THROW(log.set_pattern_filesink("%v"), Exceptions::bad_request);
    EXPECT_THROW(log.set_pattern_stdsink("%v"), Exceptions::bad_request);
    EXPECT_THROW(log.set_level_logger(spdlog::level::debug), Exceptions::bad_request);
    EXPECT_THROW(log.set_level_filesink(spdlog::level::debug), Exceptions::bad_request);
    EXPECT_THROW(log.set_level_stdsink(spdlog::level::debug), Exceptions::bad_request);
}

TEST(logging, levels_after_reopen) {
    remove_current_log_file(reopen_levels_name);
    TestLog<reopen_levels_name> log;

    // Distinct values, including the legitimate `off` level, catch accidental all-or-nothing
    // restoration and prove each resource carries its own reopen state.
    log.set_level_logger(spdlog::level::debug);
    log.set_level_filesink(spdlog::level::off);
    log.set_level_stdsink(spdlog::level::err);
    log.close();
    EXPECT_NO_THROW(log.reopen());

    EXPECT_EQ(log.logger_level(), spdlog::level::debug);
    EXPECT_EQ(log.file_level(), spdlog::level::off);
    EXPECT_EQ(log.console_level(), spdlog::level::err);

    // Reopening an already open logger is explicitly a no-op.
    EXPECT_NO_THROW(log.reopen());
}

TEST(logging, timed_close) {
    remove_current_log_file(timed_close_name);
    TestLog<timed_close_name> log;
    log.set_level_logger(spdlog::level::off);

    // Retaining the real logger keeps its sinks alive after Cheryl drops its own references. This
    // gives the timeout and Closing-state paths a deterministic lifetime dependency.
    auto retained_logger = log.retain_logger();

    constexpr std::size_t writer_count = 4;
    std::latch writers_ready{writer_count};
    std::atomic<bool> begin_writing = false;
    std::atomic<bool> stop_writing = false;
    std::atomic<bool> writer_failed = false;
    std::vector<std::thread> writers;
    writers.reserve(writer_count);

    for (std::size_t i = 0; i < writer_count; ++i) {
        writers.emplace_back([&] {
            writers_ready.count_down();
            while (!begin_writing.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            try {
                while (!stop_writing.load(std::memory_order_acquire)) {
                    log.info("concurrent-write");
                }
            } catch (...) {
                writer_failed.store(true, std::memory_order_release);
            }
        });
    }

    writers_ready.wait();
    begin_writing.store(true, std::memory_order_release);

    // The retained real logger prevents completion, so close must time out without corrupting the
    // lifecycle. Writers continue across the atomic handoff from the real logger to the fallback.
    EXPECT_THROW(log.close(5ms), Exceptions::failed_operation);
    stop_writing.store(true, std::memory_order_release);
    for (auto& writer : writers) {
        writer.join();
    }
    EXPECT_FALSE(writer_failed.load(std::memory_order_acquire));

    EXPECT_NO_THROW(log.warn("discarded-while-closing"));
    EXPECT_THROW(log.flush(), Exceptions::failed_operation);
    EXPECT_THROW(log.reopen(), Exceptions::bad_request);

    // A later close call observes the existing Closing transition rather than starting another
    // teardown. It also times out while the external owner remains alive.
    EXPECT_THROW(log.close(5ms), Exceptions::failed_operation);

    retained_logger.reset();
    EXPECT_NO_THROW(log.close(close_timeout));
}

TEST(logging, opening_state) {
    remove_current_log_file(opening_state_name);

    std::atomic<bool> block_open = false;
    std::atomic<bool> signaled_open = false;
    std::promise<void> open_entered_promise;
    auto open_entered = open_entered_promise.get_future();
    std::promise<void> release_open_promise;
    const auto release_open = release_open_promise.get_future().share();

    spdlog::file_event_handlers handlers;
    handlers.before_open = [&](const spdlog::filename_t&) {
        if (!block_open.load(std::memory_order_acquire)) {
            return;
        }

        if (!signaled_open.exchange(true, std::memory_order_acq_rel)) {
            open_entered_promise.set_value();
        }
        release_open.wait();
    };

    TestLog<opening_state_name> log{handlers};
    log.close();
    block_open.store(true, std::memory_order_release);

    // Hold reconstruction inside the file-open callback so the public API can be exercised while
    // the lifecycle is observably Opening without relying on scheduler timing.
    auto reopening = std::async(std::launch::async, [&] { log.reopen(); });
    open_entered.wait();

    EXPECT_THROW(log.reopen(), Exceptions::bad_request);
    EXPECT_THROW(log.close(5ms), Exceptions::bad_request);
    EXPECT_THROW(log.flush(), Exceptions::failed_operation);
    EXPECT_NO_THROW(log.info("discarded-while-opening"));

    release_open_promise.set_value();
    EXPECT_NO_THROW(reopening.get());
}

TEST(logging, failed_reopen) {
    remove_current_log_file(failed_reopen_name);

    std::atomic<bool> fail_next_open = false;
    spdlog::file_event_handlers handlers;
    handlers.before_open = [&](const spdlog::filename_t&) {
        if (fail_next_open.exchange(false, std::memory_order_acq_rel)) {
            throw std::runtime_error("injected logging open failure");
        }
    };

    TestLog<failed_reopen_name> log{handlers};
    log.close();
    fail_next_open.store(true, std::memory_order_release);

    // Opening failure must roll back to a coherent Closed state with no registered real logger.
    EXPECT_THROW(log.reopen(), std::runtime_error);
    EXPECT_EQ(spdlog::get(failed_reopen_name), nullptr);
    EXPECT_THROW(log.flush(), Exceptions::bad_request);
    EXPECT_NO_THROW(log.info("discarded-after-open-failure"));

    // The same object remains recoverable; no separate Failed state is required for this path.
    EXPECT_NO_THROW(log.reopen());
    EXPECT_NE(spdlog::get(failed_reopen_name), nullptr);
}

TEST(logging, close_failures) {
    LogDirectory directory;
    std::atomic<int> before = 0;
    std::atomic<int> after = 0;
    spdlog::file_event_handlers handlers;
    handlers.before_close = [&](const spdlog::filename_t&, std::FILE*) {
        ++before;
        throw 42;
    };
    handlers.after_close = [&](const spdlog::filename_t&) {
        ++after;
        throw std::runtime_error("injected after_close failure");
    };
    TestLog<callback_failure_name> log{handlers, queue_config(callback_failure_name, directory.path)};
    testing::internal::CaptureStderr();
    EXPECT_NO_THROW(log.close(close_timeout));
    const auto output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(before.load(), 1);
    EXPECT_EQ(after.load(), 1);
    EXPECT_EQ(log.backend_stats().callback_failures, 2u);
    EXPECT_TRUE(log.backend_stats().file_degraded);
    expect_contains(output, "before_close callback");
    expect_contains(output, "after_close callback");
}

TEST(logging, startup_failure) {
    LogDirectory directory;
    bool fail = true;
    int closed = 0;
    spdlog::file_event_handlers handlers;
    handlers.after_open = [&](const spdlog::filename_t&, std::FILE*) {
        if (fail)
            throw std::runtime_error("original startup failure");
    };
    handlers.before_close = [&](const spdlog::filename_t&, std::FILE*) {
        if (fail)
            throw 42;
    };
    handlers.after_close = [&](const spdlog::filename_t&) { ++closed; };
    const auto config = queue_config(startup_failure_name, directory.path);
    testing::internal::CaptureStderr();
    try {
        TestLog<startup_failure_name> log{handlers, config};
        ADD_FAILURE() << "Expected startup failure";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "original startup failure");
    }
    const auto output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(closed, 1);
    EXPECT_EQ(spdlog::get(startup_failure_name), nullptr);
    expect_contains(output, "before_close callback");
    fail = false;
    TestLog<startup_failure_name> retry{handlers, config};
    EXPECT_NO_THROW(retry.close(close_timeout));
    EXPECT_EQ(closed, 2);
}

TEST(logging, degraded_reopen) {
    LogDirectory directory;
    TestLog<formatter_failure_name> log{{}, queue_config(formatter_failure_name, directory.path)};
    const auto path = log.get_file_path();
    testing::internal::CaptureStderr();
    {
        auto native = log.retain_logger();
        native->sinks()[1]->set_formatter(std::make_unique<FailingFormatter>(true));
        // Native writes exercise the backend even in the explicitly disabled profile.
        native->warn("failed-record");
        native->warn("queued-after-failure");
    }
    EXPECT_NO_THROW(log.close(close_timeout));
    const auto output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(log.backend_stats().failed_operations, 1u);
    EXPECT_GE(log.backend_stats().suppressed_operations, 1u);
    EXPECT_TRUE(log.backend_stats().file_degraded);
    expect_contains(output, "logging file write");
    expect_contains(output, "non-standard exception");

    EXPECT_NO_THROW(log.reopen());
    EXPECT_FALSE(log.backend_stats().file_degraded);
    log.set_pattern("%v");
    { log.retain_logger()->warn("recovered-record"); }
    log.close(close_timeout);
    EXPECT_EQ(log.backend_stats().failed_operations, 1u);
    expect_contains(read_file(path), "recovered-record");
}

TEST(logging, console_failure) {
    LogDirectory directory;
    auto config = queue_config(console_failure_name, directory.path);
    config.console_level = spdlog::level::warn;
    TestLog<console_failure_name> log{{}, config};
    const auto path = log.get_file_path();
    testing::internal::CaptureStderr();
    {
        auto native = log.retain_logger();
        native->sinks()[0]->set_formatter(std::make_unique<FailingFormatter>(false));
        native->warn("file-survives-console-failure");
    }
    log.close(close_timeout);
    const auto output = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(log.backend_stats().console_degraded);
    EXPECT_FALSE(log.backend_stats().file_degraded);
    EXPECT_EQ(log.backend_stats().failed_operations, 1u);
    expect_contains(output, "logging console write");
    expect_contains(read_file(path), "file-survives-console-failure");
}

TEST(logging, default_after_reopen) {
    remove_current_log_file(default_restore_name);
    TestLog<default_restore_name> log;
    DefaultLoggerGuard default_guard;

    log.make_default();
    EXPECT_EQ(spdlog::default_logger()->name(), default_restore_name);

    // Closing the preferred default substitutes its null fallback. Reopening restores the real
    // logger only because nobody replaced that fallback in the meantime.
    log.close();
    EXPECT_EQ(spdlog::default_logger()->name(), std::format("{}-closed", default_restore_name));
    EXPECT_EQ(spdlog::get(default_restore_name), nullptr);

    log.reopen();
    EXPECT_EQ(spdlog::default_logger()->name(), default_restore_name);
    EXPECT_EQ(spdlog::get(std::format("{}-closed", default_restore_name)), nullptr);

    default_guard.restore();
}

TEST(logging, replaced_default_after_reopen) {
    remove_current_log_file(default_replaced_name);
    remove_current_log_file(alternate_default_name);
    TestLog<default_replaced_name> preferred;
    TestLog<alternate_default_name> replacement;
    DefaultLoggerGuard default_guard;

    preferred.make_default();
    preferred.close();
    replacement.make_default();
    EXPECT_EQ(spdlog::default_logger()->name(), alternate_default_name);

    // The old logger remembers that it used to be default, but that preference is conditional:
    // another explicit choice made while it was closed must win.
    preferred.reopen();
    EXPECT_EQ(spdlog::default_logger()->name(), alternate_default_name);
    EXPECT_EQ(spdlog::get(std::format("{}-closed", default_replaced_name)), nullptr);

    default_guard.restore();
}

TEST(logging, wrapper_timeout_and_writes) {
    remove_current_log_file(wrapper_name);

    // Touch the facade first so its underlying singleton is constructed, then retain the real
    // spdlog logger externally to force Logger<name>::close(timeout) down the timeout path.
    Logger<wrapper_name>::set_level_logger(spdlog::level::off);
    auto retained_logger = spdlog::get(wrapper_name);
    EXPECT_NE(retained_logger, nullptr);
    EXPECT_THROW(Logger<wrapper_name>::close(5ms), Exceptions::failed_operation);

    // Both direct wrapper calls and LogLineStream destructor-based macros must remain non-throwing
    // while Closing because they now target the null fallback.
    EXPECT_NO_THROW(Logger<wrapper_name>::warn("discarded-wrapper-write"));
    EXPECT_NO_THROW({ UWARN(wrapper_name) << "discarded-compile-time-write"; });

    retained_logger.reset();
    EXPECT_NO_THROW(Logger<wrapper_name>::close(close_timeout));

    // Closed has the same write contract, and the wrapper retains its legacy zero-argument close.
    EXPECT_NO_THROW({ UERROR(wrapper_name) << "discarded-closed-compile-time-write"; });
    EXPECT_NO_THROW(Logger<wrapper_name>::reopen());
    EXPECT_NO_THROW(Logger<wrapper_name>::close());
}

TEST(logging, filtered_args) {
    auto& log = Logger<filtered_args_name>::get();
    const auto path = log.get_file_path();
    log.set_pattern("%v");
    log.set_level_logger(spdlog::level::trace);
    log.set_level_filesink(spdlog::level::off);
    log.set_level_stdsink(spdlog::level::off);
    int arguments = 0;
    int alternatives = 0;
    EXPECT_FALSE(log.should_log(spdlog::level::warn));
    CE_LOG_WARN(filtered_args_name, "formatted-{}", ++arguments);
    UWARN(filtered_args_name) << "streamed-" << ++arguments;
    if (false)
        UWARN(filtered_args_name) << ++arguments;
    else
        ++alternatives;
    if (true)
        UWARN(filtered_args_name) << ++arguments;
    else
        ++alternatives;
    EXPECT_EQ(arguments, 0);
    EXPECT_EQ(alternatives, 1);

    // Enabling just the file must admit records even with the console off.
    log.set_level_filesink(spdlog::level::trace);
    CE_LOG_WARN(filtered_args_name, "formatted-{}", ++arguments);
    UWARN(filtered_args_name) << "streamed-" << ++arguments;
    EXPECT_EQ(arguments, ctlog::enabled(ctlog::WARNING_) ? 2 : 0);
    log.close();
    const auto content = read_file(path);
    expect_level(content, "formatted-1", ctlog::WARNING_);
    expect_level(content, "streamed-2", ctlog::WARNING_);
}

TEST(logging, compiled_args) {
    auto& log = Logger<compiled_args_name>::get();
    log.set_level_logger(spdlog::level::trace);
    log.set_level_filesink(spdlog::level::trace);
    log.set_level_stdsink(spdlog::level::off);
    int arguments = 0;
    CE_LOG_TRACE(compiled_args_name, "{}", ++arguments);
    UTRACE(compiled_args_name) << ++arguments;
    EXPECT_EQ(arguments, ctlog::enabled(ctlog::TRACE_) ? 2 : 0);
    log.close();
}

TEST(logging, lazy_failure) {
    auto& log = Logger<lazy_failure_name>::get();
    log.set_level_logger(spdlog::level::trace);
    log.set_level_filesink(spdlog::level::trace);
    log.set_level_stdsink(spdlog::level::off);
    int arguments = 0;
    const auto prepare = [&]() -> int {
        ++arguments;
        throw std::runtime_error("injected lazy argument failure");
    };
    testing::internal::CaptureStderr();
    EXPECT_NO_THROW(CE_LOG_WARN(lazy_failure_name, "{}", prepare()));
    const auto output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(arguments, ctlog::enabled(ctlog::WARNING_) ? 1 : 0);
    if (ctlog::enabled(ctlog::WARNING_)) {
        expect_contains(output, "logging write");
        expect_contains(output, "injected lazy argument failure");
    } else {
        EXPECT_TRUE(output.empty());
    }
    log.close();
}

TEST(logging, stream_failure) {
    testing::internal::CaptureStderr();
    EXPECT_NO_THROW({
        ctlog::LogLineStream([](const std::string&) { throw std::runtime_error("injected stream failure"); }) << "message";
    });
    const auto output = testing::internal::GetCapturedStderr();
    expect_contains(output, "stream log emission");
    expect_contains(output, "injected stream failure");
}

TEST(logging, partial_stream) {
    bool emitted = false;
    EXPECT_THROW(
        {
            ctlog::LogLineStream stream([&](const std::string&) { emitted = true; });
            stream << "partial";
            throw std::runtime_error("injected argument failure");
        },
        std::runtime_error
    );
    EXPECT_FALSE(emitted);
}

TEST(logging, runtime_defaults) {
    TestLog<defaults_name> log;
    const auto expected = ctlog::profile == 2 ? spdlog::level::info : spdlog::level::debug;
    EXPECT_EQ(log.logger_level(), expected);
    EXPECT_EQ(log.file_level(), expected);
    EXPECT_EQ(log.console_level(), ctlog::profile == 0 ? spdlog::level::info : spdlog::level::warn);
    EXPECT_TRUE(log.initial_configuration().directory.is_absolute());
    EXPECT_EQ(log.initial_configuration().rotation_bytes, 10u * 1024u * 1024u);
    EXPECT_EQ(log.initial_configuration().retained_files, 5u);
    EXPECT_EQ(log.initial_configuration().overflow_policy, LogOverflowPolicy::Block);
    log.close();

    const auto developer = LogConfig::for_logger("memory", LogProfile::Developer);
    const auto support = LogConfig::for_logger("memory", LogProfile::Support);
    const auto release = LogConfig::for_logger("memory", LogProfile::Release);
    EXPECT_EQ(developer.logger_level, spdlog::level::info);
    EXPECT_EQ(developer.file_level, spdlog::level::info);
    EXPECT_EQ(developer.console_level, spdlog::level::info);
    EXPECT_EQ(developer.overflow_policy, LogOverflowPolicy::Block);
    EXPECT_EQ(support.logger_level, spdlog::level::info);
    EXPECT_EQ(support.file_level, spdlog::level::info);
    EXPECT_EQ(support.console_level, spdlog::level::warn);
    EXPECT_EQ(support.overflow_policy, LogOverflowPolicy::Block);
    EXPECT_EQ(release.logger_level, spdlog::level::warn);
    EXPECT_EQ(release.file_level, spdlog::level::warn);
    EXPECT_EQ(release.console_level, spdlog::level::warn);
    EXPECT_EQ(release.overflow_policy, LogOverflowPolicy::Block);
}

TEST(logging, config_reopen) {
    const LogDirectory directory;
    const auto relative = directory.path.lexically_relative(fs::current_path());
    if (relative.empty())
        GTEST_SKIP() << "The temporary directory must share the working directory's filesystem root";
    const auto expected_path = directory.path / "logs" / std::format("{}.log", config_name);
    fs::create_directories(expected_path.parent_path());
    std::ofstream(expected_path) << "existing-marker\n";
    auto config = LogConfig::for_logger(config_name);
    config.directory = relative / "logs";
    config.rotation_bytes = 1024;
    config.retained_files = 2;
    config.rotate_on_open = false;
    config.logger_level = spdlog::level::trace;
    config.file_level = spdlog::level::info;
    config.console_level = spdlog::level::off;
    TestLog<config_name> log{spdlog::file_event_handlers{}, config};
    EXPECT_EQ(log.get_file_path(), expected_path);
    log.set_pattern("%v");
    log.info("configured-message");
    log.set_level_filesink(spdlog::level::err);
    log.close();
    {
        // This process-wide change is scoped after producers stop and the file
        // closes. Reopening must continue using its original absolute directory.
        const CurrentDirectoryGuard guard;
        fs::current_path(directory.path);
        log.reopen();
        EXPECT_EQ(log.get_file_path(), expected_path);
        EXPECT_EQ(log.logger_level(), spdlog::level::trace);
        EXPECT_EQ(log.file_level(), spdlog::level::err);
        EXPECT_EQ(log.console_level(), spdlog::level::off);
        log.close();
    }
    const auto output = read_file(expected_path);
    expect_contains(output, "existing-marker");
    expect_level(output, "configured-message", ctlog::INFO_);
}

TEST(logging, invalid_config) {
    const LogDirectory directory;
    int opens = 0;
    spdlog::file_event_handlers handlers;
    handlers.before_open = [&](const spdlog::filename_t&) { ++opens; };
    auto config = LogConfig::for_logger(bad_config_name);
    config.directory = directory.path / "unopened";
    config.rotation_bytes = 0;
    EXPECT_THROW((TestLog<bad_config_name>{handlers, config}), Exceptions::invalid_args);
    config.rotation_bytes = 1024;
    config.retained_files = 200001;
    EXPECT_THROW((TestLog<bad_config_name>{handlers, config}), Exceptions::invalid_args);
    config.retained_files = 0; // Zero backup retention is supported by the rotating sink.
    config.file_level = static_cast<spdlog::level>(-1);
    EXPECT_THROW((TestLog<bad_config_name>{handlers, config}), Exceptions::invalid_args);
    config.file_level = spdlog::level::info;
    config.overflow_policy = static_cast<LogOverflowPolicy>(-1);
    EXPECT_THROW((TestLog<bad_config_name>{handlers, config}), Exceptions::invalid_args);
    config.overflow_policy = LogOverflowPolicy::Block;
    EXPECT_THROW((TestLog<bad_path_name>{handlers, config}), Exceptions::invalid_args);
    EXPECT_EQ(opens, 0);
    EXPECT_FALSE(fs::exists(config.directory));
    EXPECT_EQ(spdlog::get(bad_config_name), nullptr);
    EXPECT_EQ(spdlog::get(bad_path_name), nullptr);
}

TEST(logging, host_pool) {
    // Construct directly even when the compatibility singleton was already used.
    // The old constructor replaced this global pool on every construction.
    const auto original = spdlog::thread_pool();
    {
        const spdlog::CE::TPInit owned;
        ASSERT_NE(owned.tp, nullptr);
        EXPECT_NE(owned.tp, original);
        EXPECT_EQ(spdlog::thread_pool(), original);
    }
    EXPECT_EQ(spdlog::thread_pool(), original);
}

TEST(logging, discard_queue) {
    if constexpr (!ctlog::enabled(ctlog::WARNING_))
        GTEST_SKIP() << "Queue submission requires a compiled severity";

    const LogDirectory directory;
    TestLog<queue_hold_name> holder{spdlog::file_event_handlers{}, queue_config(queue_hold_name, directory.path)};
    auto config = queue_config(discard_name, directory.path);
    config.overflow_policy = LogOverflowPolicy::DiscardNew;
    TestLog<discard_name> log{spdlog::file_event_handlers{}, config};
    const auto path = log.get_file_path();

    for (int generation = 0; generation < 2; ++generation) {
        holder.set_pattern("%v");
        log.set_pattern("%v");
        const auto gate = std::make_shared<BackendGate>();
        auto native = holder.retain_logger();
        // No producer has used this generation's logger yet. Attach the test sink
        // while its sink graph is quiescent, before starting any writes.
        native->sinks().push_back(std::make_shared<HeldSink>(gate));
        std::promise<void> completed;
        auto completion = completed.get_future();
        std::jthread producer;
        const BackendRelease release{gate};
        holder.warn("hold-worker");
        ASSERT_EQ(gate->ready.wait_for(close_timeout), std::future_status::ready);
        ASSERT_EQ(log.shared_queue_stats().queued_items, 0u);
        const auto discarded = log.shared_queue_stats().discarded_items;

        for (std::size_t i = 0; i < queue_capacity; ++i)
            log.warn("accepted[{}:{}]", generation, i);
        ASSERT_EQ(log.shared_queue_stats().queued_items, queue_capacity);
        ASSERT_EQ(log.shared_queue_stats().discarded_items, discarded);
        // Submit on a joined producer so a mistaken Block mapping can fail the
        // bounded observation, release the backend, and unwind without hanging.
        producer = std::jthread([&] {
            try {
                log.warn("discarded[{}]", generation);
                log.flush(); // Flush submission follows this Log's policy and is lost too.
                completed.set_value();
            } catch (...) {
                completed.set_exception(std::current_exception());
            }
        });
        ASSERT_EQ(completion.wait_for(close_timeout), std::future_status::ready);
        EXPECT_NO_THROW(completion.get());
        producer.join();
        EXPECT_EQ(log.shared_queue_stats().queued_items, queue_capacity);
        EXPECT_EQ(log.shared_queue_stats().discarded_items, discarded + 2);
        EXPECT_EQ(holder.shared_queue_stats().discarded_items, discarded + 2);

        gate->unblock();
        native.reset();
        EXPECT_NO_THROW(log.close(close_timeout));
        EXPECT_NO_THROW(holder.close(close_timeout));
        EXPECT_EQ(log.shared_queue_stats().discarded_items, discarded + 2);
        EXPECT_EQ(log.shared_queue_stats().queued_items, 0u);
        const auto content = read_file(path);
        expect_sequence(content, generation);
        EXPECT_EQ(content.find(std::format("discarded[{}]", generation)), std::string::npos);
        if (generation == 0) {
            log.reopen();
            holder.reopen();
            EXPECT_EQ(log.initial_configuration().overflow_policy, LogOverflowPolicy::DiscardNew);
            EXPECT_EQ(log.shared_queue_stats().discarded_items, discarded + 2);
        }
    }
}

TEST(logging, blocking_queue) {
    if constexpr (!ctlog::enabled(ctlog::WARNING_))
        GTEST_SKIP() << "Queue submission requires a compiled severity";

    const LogDirectory directory;
    auto config = queue_config(queue_hold_name, directory.path);
    config.overflow_policy = LogOverflowPolicy::DiscardNew;
    TestLog<queue_hold_name> holder{spdlog::file_event_handlers{}, config};
    TestLog<blocking_name> log{spdlog::file_event_handlers{}, queue_config(blocking_name, directory.path)};
    const auto path = log.get_file_path();
    const auto holder_path = holder.get_file_path();
    holder.set_pattern("%v");
    log.set_pattern("%v");
    const auto gate = std::make_shared<BackendGate>();
    auto native = holder.retain_logger();
    native->sinks().push_back(std::make_shared<HeldSink>(gate));
    std::promise<void> started;
    auto starting = started.get_future();
    std::promise<void> completed;
    auto completion = completed.get_future();
    std::jthread producer;
    // Declared after the producer so fatal assertions release the backend before
    // jthread destruction joins a producer that may be waiting for queue capacity.
    const BackendRelease release{gate};
    holder.warn("hold-worker");
    ASSERT_EQ(gate->ready.wait_for(close_timeout), std::future_status::ready);
    ASSERT_EQ(log.shared_queue_stats().queued_items, 0u);
    const auto discarded = log.shared_queue_stats().discarded_items;
    for (std::size_t i = 0; i < queue_capacity; ++i)
        holder.warn("accepted[0:{}]", i);
    ASSERT_EQ(log.shared_queue_stats().queued_items, queue_capacity);
    ASSERT_EQ(log.shared_queue_stats().discarded_items, discarded);

    producer = std::jthread([&] {
        started.set_value();
        try {
            log.warn("blocking-marker");
            completed.set_value();
        } catch (...) {
            completed.set_exception(std::current_exception());
        }
    });
    ASSERT_EQ(starting.wait_for(close_timeout), std::future_status::ready);
    EXPECT_EQ(completion.wait_for(50ms), std::future_status::timeout);
    gate->unblock();
    ASSERT_EQ(completion.wait_for(close_timeout), std::future_status::ready);
    EXPECT_NO_THROW(completion.get());
    producer.join();
    native.reset();
    EXPECT_NO_THROW(log.close(close_timeout));
    EXPECT_NO_THROW(holder.close(close_timeout));
    EXPECT_EQ(log.shared_queue_stats().discarded_items, discarded);
    expect_contains(read_file(path), "blocking-marker");
    expect_sequence(read_file(holder_path), 0);
}
