#include <core/logging/backend-guard.h>
#include <internals/failure-reporting.h>

#include <utility>
#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace CE::LogDetail {
    namespace {
        thread_local BackendCounters* active_backend = nullptr;

        void report_counts(const char* name, const BackendCounters& counters) noexcept {
            std::array<char, 512> record{};
            const auto size = std::snprintf(
                record.data(), record.size(),
                "[Cheryl logging] %.100s: failures=%zu callbacks=%zu suppressed=%zu recursive=%zu rejected_reentry=%zu\n",
                name, counters.failed_operations.load(std::memory_order_relaxed),
                counters.callback_failures.load(std::memory_order_relaxed),
                counters.suppressed_operations.load(std::memory_order_relaxed),
                counters.recursive_submissions.load(std::memory_order_relaxed),
                counters.rejected_reentry.load(std::memory_order_relaxed)
            );
            if (size > 0) {
                (void)std::fwrite(record.data(), 1, std::min(static_cast<std::size_t>(size), record.size() - 1), stderr);
                (void)std::fflush(stderr);
            }
        }
    }

    BackendScope::BackendScope(BackendCounters& counters) noexcept
    : previous_(std::exchange(active_backend, &counters)) {}

    BackendScope::~BackendScope() { active_backend = previous_; }

    bool in_backend() noexcept { return active_backend != nullptr; }

    bool suppress_backend_emission() noexcept {
        if (!active_backend)
            return false;
        active_backend->recursive_submissions.fetch_add(1, std::memory_order_relaxed);
        active_backend->summary_pending.store(true, std::memory_order_relaxed);
        return true;
    }

    void reject_backend_reentry(const char* operation) {
        if (!active_backend)
            return;
        active_backend->rejected_reentry.fetch_add(1, std::memory_order_relaxed);
        active_backend->summary_pending.store(true, std::memory_order_relaxed);
        throw Exceptions::bad_request(CE_HERE, std::format("Cannot {} from a logging backend or file callback.", operation));
    }

    void report_queue_loss(const std::shared_ptr<QueueReports>& reports, const std::size_t discards) noexcept {
        auto previous = reports->reported_discards.load(std::memory_order_relaxed);
        while (previous < discards) {
            if (reports->reported_discards.compare_exchange_weak(previous, discards, std::memory_order_relaxed)) {
                std::array<char, 256> record{};
                const auto size = std::snprintf(
                    record.data(), record.size(), "[Cheryl logging] shared queue: discarded=%zu new=%zu (records and flushes)\n",
                    discards, discards - previous
                );
                if (size > 0) {
                    (void)std::fwrite(record.data(), 1, std::min(static_cast<std::size_t>(size), record.size() - 1), stderr);
                    (void)std::fflush(stderr);
                }
                return;
            }
        }
    }

    void report_backend_summary(const char* name, const std::shared_ptr<BackendCounters>& counters) noexcept {
        if (counters->summary_pending.exchange(false, std::memory_order_relaxed))
            report_counts(name, *counters);
    }

    std::shared_ptr<spdlog::details::thread_pool> make_owned_pool(const std::shared_ptr<QueueReports>& reports) {
        reject_backend_reentry("initialize the logging pool");
        return {new spdlog::details::thread_pool(8192, 1), [reports](spdlog::details::thread_pool* pool) noexcept {
            const auto discarded = pool->discard_counter();
            // Destruction drains accepted work and joins the single worker. No
            // queued logger/sink owns this pool; final reporting cannot reenter it.
            delete pool;
            report_queue_loss(reports, discarded);
        }};
    }
    DestinationState::DestinationState(std::shared_ptr<BackendCounters> counters, const bool file)
    : counters_(std::move(counters)), file_(file) {}

    bool DestinationState::degraded() const noexcept { return degraded_.load(std::memory_order_relaxed); }

    void DestinationState::suppress() noexcept {
        counters_->suppressed_operations.fetch_add(1, std::memory_order_relaxed);
        counters_->summary_pending.store(true, std::memory_order_relaxed);
    }

    void DestinationState::fail_operation(const bool flush, std::exception_ptr failure) noexcept {
        counters_->failed_operations.fetch_add(1, std::memory_order_relaxed);
        counters_->summary_pending.store(true, std::memory_order_relaxed);
        degraded_.store(true, std::memory_order_relaxed);
        const auto* phase = file_ ? (flush ? "logging file flush" : "logging file write")
                                  : (flush ? "logging console flush" : "logging console write");
        Diagnostics::report_failure(phase, std::move(failure));
    }

    void DestinationState::fail_open_callback() noexcept {
        counters_->callback_failures.fetch_add(1, std::memory_order_relaxed);
        counters_->summary_pending.store(true, std::memory_order_relaxed);
    }

    void DestinationState::fail_close_callback(const std::size_t slot, std::exception_ptr failure) noexcept {
        counters_->callback_failures.fetch_add(1, std::memory_order_relaxed);
        counters_->summary_pending.store(true, std::memory_order_relaxed);
        degraded_.store(true, std::memory_order_relaxed);
        try {
            const std::lock_guard lock(callback_mutex_);
            if (!close_failures_[slot])
                close_failures_[slot] = std::move(failure);
        } catch (...) {
            // Counting and degradation remain available even if bookkeeping fails.
        }
    }

    void DestinationState::report_close_failures() noexcept {
        std::array<std::exception_ptr, 2> failures;
        try {
            const std::lock_guard lock(callback_mutex_);
            failures.swap(close_failures_);
        } catch (...) {
            return;
        }
        if (failures[0])
            Diagnostics::report_failure("logging before_close callback", std::move(failures[0]));
        if (failures[1])
            Diagnostics::report_failure("logging after_close callback", std::move(failures[1]));
    }

    GuardedSink::GuardedSink(spdlog::sink_ptr delegate, std::shared_ptr<DestinationState> state)
    : delegate_(std::move(delegate)), state_(std::move(state)) {}

    bool GuardedSink::admits(const spdlog::level level) const noexcept {
        return !state_->degraded() && delegate_->should_log(level);
    }

    void GuardedSink::log(const spdlog::details::log_msg& message) {
        if (suppress_backend_emission())
            return;
        if (state_->degraded()) {
            state_->suppress();
            return;
        }
        if (!delegate_->should_log(message.log_level))
            return;
        std::exception_ptr failure;
        try {
            const BackendScope scope{state_->counters()};
            delegate_->log(message);
        } catch (...) {
            failure = std::current_exception();
        }
        state_->report_close_failures();
        if (failure)
            state_->fail_operation(false, std::move(failure));
    }

    void GuardedSink::flush() {
        reject_backend_reentry("flush a logging destination");
        if (state_->degraded()) {
            state_->suppress();
            return;
        }
        std::exception_ptr failure;
        try {
            const BackendScope scope{state_->counters()};
            delegate_->flush();
        } catch (...) {
            failure = std::current_exception();
        }
        state_->report_close_failures();
        if (failure)
            state_->fail_operation(true, std::move(failure));
    }

    void GuardedSink::set_pattern(const std::string& pattern) {
        reject_backend_reentry("set a logging pattern");
        delegate_->set_pattern(pattern);
    }

    void GuardedSink::set_formatter(std::unique_ptr<spdlog::formatter> formatter) {
        reject_backend_reentry("set a logging formatter");
        delegate_->set_formatter(std::move(formatter));
    }

    OwnedLogger::OwnedLogger(
        std::string name,
        const std::vector<spdlog::sink_ptr>& sinks,
        std::weak_ptr<spdlog::details::thread_pool> pool,
        const spdlog::async_overflow_policy overflow,
        std::shared_ptr<BackendCounters> counters
    )
    : spdlog::logger(std::move(name), sinks.begin(), sinks.end()),
      owned_sinks_(sinks),
      counters_(std::move(counters)),
      pool_(std::move(pool)),
      overflow_(overflow),
      backend_(std::make_shared<spdlog::async_logger>(name_, sinks.begin(), sinks.end(), pool_, overflow_)) {
        backend_->set_level(spdlog::level::trace);
        const auto report_error = [counters = counters_](const std::string&) noexcept {
            counters->failed_operations.fetch_add(1, std::memory_order_relaxed);
            counters->summary_pending.store(true, std::memory_order_relaxed);
            // The library's error string can contain absolute user paths. Keep
            // the fallback bounded and leave exact details to caller-owned errors.
            Diagnostics::report_failure("logging submission or formatting", std::current_exception());
        };
        set_error_handler(report_error);
        backend_->set_error_handler(report_error);
    }

    bool OwnedLogger::valid_graph() const noexcept { return sinks_ == owned_sinks_; }

    OwnedLogger::~OwnedLogger() {
        if (in_backend()) {
            Diagnostics::report_failure("final native logger release from a backend callback", {});
            std::terminate();
        }
    }

    void OwnedLogger::sink_it_(const spdlog::details::log_msg& message) {
        if (suppress_backend_emission())
            return;
        try {
            if (!valid_graph())
                throw std::logic_error("The owned logging sink graph cannot be replaced");
            // Admission has already checked actual delegates for facade writes.
            // Native submission retains its queue contract, including observations
            // of already-degraded destinations by the backend guards.
            backend_->log(message.time, message.source, message.log_level, message.payload);
            if (should_flush_(message))
                backend_->flush();
        } catch (...) {
            counters_->failed_operations.fetch_add(1, std::memory_order_relaxed);
            counters_->summary_pending.store(true, std::memory_order_relaxed);
            Diagnostics::report_failure("logging native submission", std::current_exception());
        }
    }

    void OwnedLogger::flush_() {
        reject_backend_reentry("flush a logger");
        if (!valid_graph())
            throw std::logic_error("The owned logging sink graph cannot be replaced");
        backend_->flush();
    }

    std::shared_ptr<spdlog::logger> OwnedLogger::clone(std::string name) {
        reject_backend_reentry("clone a logger");
        auto result = std::make_shared<OwnedLogger>(std::move(name), owned_sinks_, pool_, overflow_, counters_);
        result->set_level(log_level());
        result->flush_on(flush_level());
        return result;
    }

    spdlog::file_event_handlers guard_file_handlers(
        const spdlog::file_event_handlers& handlers, const std::shared_ptr<DestinationState>& state
    ) {
        spdlog::file_event_handlers guarded;
        if (handlers.before_open) {
            guarded.before_open = [callback = handlers.before_open, state](const spdlog::filename_t& path) {
                try {
                    const BackendScope scope{state->counters()};
                    callback(path);
                } catch (...) {
                    state->fail_open_callback();
                    throw;
                }
            };
        }
        if (handlers.after_open) {
            guarded.after_open = [callback = handlers.after_open, state](const spdlog::filename_t& path, std::FILE* file) {
                try {
                    const BackendScope scope{state->counters()};
                    callback(path, file);
                } catch (...) {
                    state->fail_open_callback();
                    throw;
                }
            };
        }
        if (handlers.before_close) {
            guarded.before_close = [callback = handlers.before_close, state](const spdlog::filename_t& path, std::FILE* file) noexcept {
                try {
                    const BackendScope scope{state->counters()};
                    callback(path, file);
                } catch (...) {
                    state->fail_close_callback(0, std::current_exception());
                }
            };
        }
        if (handlers.after_close) {
            guarded.after_close = [callback = handlers.after_close, state](const spdlog::filename_t& path) noexcept {
                try {
                    const BackendScope scope{state->counters()};
                    callback(path);
                } catch (...) {
                    state->fail_close_callback(1, std::current_exception());
                }
            };
        }
        return guarded;
    }

    bool sink_admits(const spdlog::sink_ptr& sink, const spdlog::level level) noexcept {
        if (!sink || !sink->should_log(level))
            return false;
        if (const auto* guarded = dynamic_cast<const GuardedSink*>(sink.get()))
            return guarded->admits(level);
        return true;
    }
}
