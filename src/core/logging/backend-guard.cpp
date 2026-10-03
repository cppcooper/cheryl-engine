#include <core/logging/backend-guard.h>
#include <internals/failure-reporting.h>

#include <utility>

namespace CE::LogDetail {
    DestinationState::DestinationState(std::shared_ptr<BackendCounters> counters, const bool file)
    : counters_(std::move(counters)), file_(file) {}

    bool DestinationState::degraded() const noexcept { return degraded_.load(std::memory_order_relaxed); }

    void DestinationState::suppress() noexcept { counters_->suppressed_operations.fetch_add(1, std::memory_order_relaxed); }

    void DestinationState::fail_operation(const bool flush, std::exception_ptr failure) noexcept {
        counters_->failed_operations.fetch_add(1, std::memory_order_relaxed);
        degraded_.store(true, std::memory_order_relaxed);
        const auto* phase = file_ ? (flush ? "logging file flush" : "logging file write")
                                  : (flush ? "logging console flush" : "logging console write");
        Diagnostics::report_failure(phase, std::move(failure));
    }

    void DestinationState::fail_open_callback() noexcept {
        counters_->callback_failures.fetch_add(1, std::memory_order_relaxed);
    }

    void DestinationState::fail_close_callback(const std::size_t slot, std::exception_ptr failure) noexcept {
        counters_->callback_failures.fetch_add(1, std::memory_order_relaxed);
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
        if (state_->degraded()) {
            state_->suppress();
            return;
        }
        if (!delegate_->should_log(message.log_level))
            return;
        std::exception_ptr failure;
        try {
            delegate_->log(message);
        } catch (...) {
            failure = std::current_exception();
        }
        state_->report_close_failures();
        if (failure)
            state_->fail_operation(false, std::move(failure));
    }

    void GuardedSink::flush() {
        if (state_->degraded()) {
            state_->suppress();
            return;
        }
        std::exception_ptr failure;
        try {
            delegate_->flush();
        } catch (...) {
            failure = std::current_exception();
        }
        state_->report_close_failures();
        if (failure)
            state_->fail_operation(true, std::move(failure));
    }

    void GuardedSink::set_pattern(const std::string& pattern) { delegate_->set_pattern(pattern); }

    void GuardedSink::set_formatter(std::unique_ptr<spdlog::formatter> formatter) {
        delegate_->set_formatter(std::move(formatter));
    }

    spdlog::file_event_handlers guard_file_handlers(
        const spdlog::file_event_handlers& handlers, const std::shared_ptr<DestinationState>& state
    ) {
        spdlog::file_event_handlers guarded;
        if (handlers.before_open) {
            guarded.before_open = [callback = handlers.before_open, state](const spdlog::filename_t& path) {
                try {
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
                    callback(path, file);
                } catch (...) {
                    state->fail_close_callback(0, std::current_exception());
                }
            };
        }
        if (handlers.after_close) {
            guarded.after_close = [callback = handlers.after_close, state](const spdlog::filename_t& path) noexcept {
                try {
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
