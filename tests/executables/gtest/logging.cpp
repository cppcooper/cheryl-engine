#include <gtest/gtest.h>

#include <core/logging.h>
#include <internals/compile-time-logging.hpp>
#include <internals/exceptions.h>

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
#include <vector>

using namespace CE;
using namespace std::chrono_literals;

namespace {
    namespace fs = std::filesystem;

    constexpr auto close_timeout = 2s;

    constexpr char file_barrier_name[] = "logging-file-barrier";
    constexpr char console_routing_name[] = "logging-console-routing";
    constexpr char closed_state_name[] = "logging-closed-state";
    constexpr char reopen_levels_name[] = "logging-reopen-levels";
    constexpr char timed_close_name[] = "logging-timed-close";
    constexpr char opening_state_name[] = "logging-opening-state";
    constexpr char failed_reopen_name[] = "logging-failed-reopen";
    constexpr char default_restore_name[] = "logging-default-restore";
    constexpr char default_replaced_name[] = "logging-default-replaced";
    constexpr char alternate_default_name[] = "logging-alternate-default";
    constexpr char wrapper_name[] = "logging-wrapper";

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

        return {
            std::istreambuf_iterator<char>{file},
            std::istreambuf_iterator<char>{}
        };
    }

    void expect_contains(const std::string& text, const std::string& fragment) {
        EXPECT_NE(text.find(fragment), std::string::npos)
            << "Expected log output to contain: " << fragment;
    }

    template <const char* name>
    class TestLog final : public Log<name> {
    public:
        explicit TestLog(spdlog::file_event_handlers event_handlers = {})
        : Log<name>(std::move(event_handlers)) {
        }

        ~TestLog() {
            try {
                this->close(close_timeout);
            } catch (...) {
                // Test cleanup must not mask the assertion or exception that ended the test.
            }
        }

        [[nodiscard]] std::shared_ptr<spdlog::logger> retain_logger() const {
            return this->m_logger.load();
        }

        [[nodiscard]] spdlog::level logger_level() const {
            return this->m_logger.load()->log_level();
        }

        [[nodiscard]] spdlog::level file_level() const {
            return this->m_file.load()->log_level();
        }

        [[nodiscard]] spdlog::level console_level() const {
            return this->m_console.load()->log_level();
        }
    };

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

TEST(logging, close_is_a_file_completion_barrier) {
    remove_current_log_file(file_barrier_name);

    std::atomic<int> close_events = 0;
    spdlog::file_event_handlers handlers;
    handlers.after_close = [&close_events](const spdlog::filename_t&) {
        ++close_events;
    };

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
    expect_contains(content, "trace-message");
    expect_contains(content, "debug-message");
    expect_contains(content, "info-message");
    expect_contains(content, "warn-message");
    expect_contains(content, "error-message");
    expect_contains(content, "critical-message");

    // Closing an already closed logger is idempotent and must not destroy the sink twice.
    EXPECT_NO_THROW(log.close());
    EXPECT_EQ(close_events.load(), 1);
}

TEST(logging, console_routes_levels_before_close_returns) {
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

    expect_contains(stdout_text, "stdout-info");
    expect_contains(stdout_text, "stdout-warn");
    expect_contains(stderr_text, "stderr-error");
    EXPECT_EQ(stdout_text.find("stderr-error"), std::string::npos);
    EXPECT_EQ(stderr_text.find("stdout-info"), std::string::npos);
    EXPECT_EQ(stderr_text.find("stdout-warn"), std::string::npos);
}

TEST(logging, closed_state_drops_writes_and_rejects_live_resource_operations) {
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

TEST(logging, reopen_restores_levels_independently) {
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

TEST(logging, timed_close_preserves_closing_and_tolerates_concurrent_writes) {
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

TEST(logging, opening_state_rejects_conflicting_lifecycle_operations) {
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
    auto reopening = std::async(std::launch::async, [&] {
        log.reopen();
    });
    open_entered.wait();

    EXPECT_THROW(log.reopen(), Exceptions::bad_request);
    EXPECT_THROW(log.close(5ms), Exceptions::bad_request);
    EXPECT_THROW(log.flush(), Exceptions::failed_operation);
    EXPECT_NO_THROW(log.info("discarded-while-opening"));

    release_open_promise.set_value();
    EXPECT_NO_THROW(reopening.get());
}

TEST(logging, failed_reopen_returns_to_closed_and_can_retry) {
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

TEST(logging, reopen_restores_default_when_fallback_is_unchanged) {
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

TEST(logging, reopen_does_not_steal_an_explicitly_replaced_default) {
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

TEST(logging, logger_wrapper_forwards_close_timeout_and_compile_time_writes_remain_safe) {
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
    EXPECT_NO_THROW({
        UWARN(wrapper_name) << "discarded-compile-time-write";
    });

    retained_logger.reset();
    EXPECT_NO_THROW(Logger<wrapper_name>::close(close_timeout));

    // Closed has the same write contract, and the wrapper retains its legacy zero-argument close.
    EXPECT_NO_THROW({
        UERROR(wrapper_name) << "discarded-closed-compile-time-write";
    });
    EXPECT_NO_THROW(Logger<wrapper_name>::reopen());
    EXPECT_NO_THROW(Logger<wrapper_name>::close());
}
