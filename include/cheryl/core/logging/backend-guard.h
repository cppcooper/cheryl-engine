#pragma once
#include <spdlog/common.h>
#include <spdlog/sinks/sink.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>

namespace CE {
    /** Cumulative observations for one Log, including failed openings and all
     * generations. Fields are sampled independently. Degradation describes the
     * last published generation and clears only after completed close/reopen.
     * Suppressed operations were already queued when their destination failed.
     */
    struct LogBackendStats {
        std::size_t failed_operations = 0;
        std::size_t callback_failures = 0;
        std::size_t suppressed_operations = 0;
        bool file_degraded = false;
        bool console_degraded = false;
    };
}

namespace CE::LogDetail {
    struct BackendCounters {
        std::atomic_size_t failed_operations{0};
        std::atomic_size_t callback_failures{0};
        std::atomic_size_t suppressed_operations{0};
    };

    class DestinationState final {
        const std::shared_ptr<BackendCounters> counters_;
        const bool file_;
        std::atomic_bool degraded_{false};
        std::mutex callback_mutex_;
        std::array<std::exception_ptr, 2> close_failures_{};

    public:
        DestinationState(std::shared_ptr<BackendCounters> counters, bool file);
        [[nodiscard]] bool degraded() const noexcept;
        void suppress() noexcept;
        void fail_operation(bool flush, std::exception_ptr failure) noexcept;
        void fail_open_callback() noexcept;
        void fail_close_callback(std::size_t slot, std::exception_ptr failure) noexcept;
        // Called only after delegate locks have unwound, or after destruction.
        void report_close_failures() noexcept;
    };

    struct BackendGeneration {
        std::shared_ptr<DestinationState> file;
        std::shared_ptr<DestinationState> console;
    };

    /** The delegate remains available to the owned Log for levels, patterns and
     * retained-file ownership. Backend operations use this guard; direct delegate
     * log/flush and native sink-graph replacement bypass the supported contract.
     */
    class GuardedSink final : public spdlog::sinks::sink {
        const spdlog::sink_ptr delegate_;
        const std::shared_ptr<DestinationState> state_;

    public:
        GuardedSink(spdlog::sink_ptr delegate, std::shared_ptr<DestinationState> state);
        [[nodiscard]] bool admits(spdlog::level level) const noexcept;
        void log(const spdlog::details::log_msg& message) override;
        void flush() override;
        void set_pattern(const std::string& pattern) override;
        void set_formatter(std::unique_ptr<spdlog::formatter> formatter) override;
    };

    [[nodiscard]] spdlog::file_event_handlers guard_file_handlers(
        const spdlog::file_event_handlers& handlers, const std::shared_ptr<DestinationState>& state
    );
    [[nodiscard]] bool sink_admits(const spdlog::sink_ptr& sink, spdlog::level level) noexcept;
}
