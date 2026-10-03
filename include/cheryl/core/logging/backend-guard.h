#pragma once
#include <spdlog/common.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/async_logger.h>
#include <spdlog/details/thread_pool.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <vector>

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
        std::size_t recursive_submissions = 0;
        std::size_t rejected_reentry = 0;
    };
}

namespace CE::LogDetail {
    struct BackendCounters {
        std::atomic_size_t failed_operations{0};
        std::atomic_size_t callback_failures{0};
        std::atomic_size_t suppressed_operations{0};
        std::atomic_size_t recursive_submissions{0};
        std::atomic_size_t rejected_reentry{0};
        std::atomic_bool summary_pending{false};
    };

    class BackendScope final {
        BackendCounters* previous_;

    public:
        explicit BackendScope(BackendCounters& counters) noexcept;
        ~BackendScope();
        BackendScope(const BackendScope&) = delete;
        BackendScope& operator=(const BackendScope&) = delete;
    };

    // Suppress ordinary writes; reject operations that could wait on this backend.
    // Both run before singleton initialization or lifecycle/delegate locks.
    [[nodiscard]] bool suppress_backend_emission() noexcept;
    void reject_backend_reentry(const char* operation);
    [[nodiscard]] bool in_backend() noexcept;

    struct QueueReports {
        std::atomic_size_t reported_discards{0};
    };

    void report_queue_loss(const std::shared_ptr<QueueReports>& reports, std::size_t discards) noexcept;
    void report_backend_summary(const char* name, const std::shared_ptr<BackendCounters>& counters) noexcept;
    [[nodiscard]] std::shared_ptr<spdlog::details::thread_pool> make_owned_pool(const std::shared_ptr<QueueReports>& reports);

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
        [[nodiscard]] BackendCounters& counters() const noexcept { return *counters_; }
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

    /** Published native owners submit through this frontend. The private async
     * backend retains the same guarded destinations and borrows the pool. The
     * sink vector is fixed; changing it is rejected rather than bypassing guards.
     */
    class OwnedLogger final : public spdlog::logger {
        const std::vector<spdlog::sink_ptr> owned_sinks_;
        const std::shared_ptr<BackendCounters> counters_;
        const std::weak_ptr<spdlog::details::thread_pool> pool_;
        const spdlog::async_overflow_policy overflow_;
        const std::shared_ptr<spdlog::async_logger> backend_;

        [[nodiscard]] bool valid_graph() const noexcept;

    public:
        OwnedLogger(
            std::string name,
            const std::vector<spdlog::sink_ptr>& sinks,
            std::weak_ptr<spdlog::details::thread_pool> pool,
            spdlog::async_overflow_policy overflow,
            std::shared_ptr<BackendCounters> counters
        );
        ~OwnedLogger() override;
        // Clones retain the fixed graph and the same guarded submission boundary.
        std::shared_ptr<spdlog::logger> clone(std::string name) override;

    protected:
        void sink_it_(const spdlog::details::log_msg& message) override;
        void flush_() override;
    };

    [[nodiscard]] spdlog::file_event_handlers guard_file_handlers(
        const spdlog::file_event_handlers& handlers, const std::shared_ptr<DestinationState>& state
    );
    [[nodiscard]] bool sink_admits(const spdlog::sink_ptr& sink, spdlog::level level) noexcept;
}
