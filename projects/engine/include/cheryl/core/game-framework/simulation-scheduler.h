#pragma once

#include <cheryl/export.h>

#include <chrono>
#include <cstddef>
#include <vector>

namespace CE::Runtime::Simulation {
    /** Monotonic observation clock; simulation durations are selected separately. */
    using Clock = std::chrono::steady_clock;

    /** Simulation delta selection, independent of sequential/concurrent runtime execution. */
    enum class Mode { Variable, ///< One elapsed-time update when the variable interval is due.
        Fixed ///< Fixed-size steps, with bounded lag recovery.
    };
    /** Recovery policy after the fixed-update budget is exceeded. */
    enum class LagRecovery { DropExcessLag, ///< Drop whole-step debt while preserving the fractional step remainder.
        VariableCatchUp ///< Execute the configured fixed prefix and one capped elapsed-time recovery step.
    };
    /** Kind of an actual simulation step supplied to the game. */
    enum class UpdateKind { Variable, ///< Ordinary elapsed-time variable update.
        Fixed, ///< Ordinary fixed-duration update.
        VariableCatchUp ///< Recovery update representing accumulated lag, subject to recovery_cap.
    };

    /** Inert pacing and bounded-recovery settings validated by Scheduler construction. */
    struct TimingOptions {
        // TODO: an optimization configurer may suggest limits after profiling;
        // it must preserve explicitly selected timing/input contracts.
        ::CE::Runtime::Simulation::Mode mode = ::CE::Runtime::Simulation::Mode::Variable; ///< Select variable pacing or fixed-step accumulation.
        ::CE::Runtime::Simulation::Clock::duration variable_interval =
            std::chrono::duration_cast<::CE::Runtime::Simulation::Clock::duration>(
                std::chrono::nanoseconds{16666667}
            ); // Zero permits an unpaced variable loop.
        ::CE::Runtime::Simulation::Clock::duration fixed_step =
            std::chrono::duration_cast<::CE::Runtime::Simulation::Clock::duration>(std::chrono::nanoseconds{16666667});
        std::size_t max_fixed_updates = 1; ///< Positive maximum fixed updates before overload recovery.
        ::CE::Runtime::Simulation::LagRecovery recovery = ::CE::Runtime::Simulation::LagRecovery::DropExcessLag; ///< Behavior when fixed-step debt exceeds the budget.
        std::size_t fixed_updates_before_recovery = 0; ///< Fixed prefix for overloaded VariableCatchUp; must not exceed max_fixed_updates.
        ::CE::Runtime::Simulation::Clock::duration recovery_cap = std::chrono::milliseconds{100}; ///< Maximum variable recovery delta; nonnegative, with zero disabling this cap.
    };

    /** One actual game update; owned value with a selected simulation delta. */
    struct Step {
        ::CE::Runtime::Simulation::Clock::duration delta; ///< Simulation duration represented by this update, not necessarily its observation interval.
        ::CE::Runtime::Simulation::UpdateKind kind; ///< Ordinary variable/fixed or overload-recovery step.
    };

    /** Owned finite update sequence selected once from an observation interval. */
    struct Batch {
        std::vector<::CE::Runtime::Simulation::Step> steps; ///< Actual updates in execution order; empty when no step is due.
        ::CE::Runtime::Simulation::Clock::duration observed_elapsed{}; ///< Elapsed observation time since the preceding advance() call.
        ::CE::Runtime::Simulation::Clock::duration dropped{}; ///< Unsimulated lag; runtime reports it once on the final actual update.
    };

    /** Clock input is supplied by the owner; scheduling never sleeps or polls input.
     * advance() selects a bounded batch once. Time spent running that batch enters
     * the next cycle, so expensive updates cannot extend the current batch forever.
     */
    class Scheduler final {
        ::CE::Runtime::Simulation::TimingOptions options_;
        ::CE::Runtime::Simulation::Clock::time_point observed_at_;
        ::CE::Runtime::Simulation::Clock::duration accumulated_{};

    public:
        CE_API explicit Scheduler(
            ::CE::Runtime::Simulation::TimingOptions options = ::CE::Runtime::Simulation::TimingOptions{},
            ::CE::Runtime::Simulation::Clock::time_point start = ::CE::Runtime::Simulation::Clock::now()
        );

        [[nodiscard]] const ::CE::Runtime::Simulation::TimingOptions& options() const { return options_; }
        [[nodiscard]] CE_API ::CE::Runtime::Simulation::Batch advance(::CE::Runtime::Simulation::Clock::time_point now);
        [[nodiscard]] CE_API ::CE::Runtime::Simulation::Clock::time_point next_update_at() const;
    };
}

/** @var CE::Runtime::Simulation::Clock::duration CE::Runtime::Simulation::TimingOptions::variable_interval
 * Nonnegative minimum accumulated duration before a variable update. Zero permits an unpaced loop.
 */
/** @var CE::Runtime::Simulation::Clock::duration CE::Runtime::Simulation::TimingOptions::fixed_step
 * Strictly positive duration of each ordinary fixed update; independent of wall-clock frame frequency.
 */

/**
 * @fn CE::Runtime::Simulation::Scheduler::Scheduler(TimingOptions options, Clock::time_point start)
 * @brief Validate and retain a scheduling policy and initial observation time.
 * @param options Valid mode/recovery, nonnegative variable interval/cap, positive fixed step and update budget, bounded recovery prefix.
 * @param start Initial monotonic observation boundary.
 * @throws CE::Exceptions::invalid_args For invalid policy values.
 * Creates no threads and performs no sleeping or native/input work.
 * @see [Detailed guide](cheryl-guide:runtime/simulation-timing)
 */

/**
 * @fn const CE::Runtime::Simulation::TimingOptions& CE::Runtime::Simulation::Scheduler::options() const
 * @brief Borrow the immutable policy retained by this scheduler.
 * @return Reference valid until scheduler destruction; does not observe mutable external settings.
 * @see [Detailed guide](cheryl-guide:runtime/simulation-timing)
 */

/**
 * @fn CE::Runtime::Simulation::Batch CE::Runtime::Simulation::Scheduler::advance(Clock::time_point now)
 * @brief Consume observation time and select a finite update batch.
 * @param now Monotonic time at or after the previous observation.
 * @return Owned step values, observation elapsed time and explicitly dropped lag.
 * @throws CE::Exceptions::invalid_args For backwards time or unrepresentable elapsed/accumulated durations.
 * One owner serializes calls. Time spent executing the returned batch is observed by the next call; it cannot extend this batch. This operation does not execute the steps.
 * @see [Detailed guide](cheryl-guide:runtime/simulation-timing)
 */

/**
 * @fn CE::Runtime::Simulation::Clock::time_point CE::Runtime::Simulation::Scheduler::next_update_at() const
 * @brief Compute the next due observation boundary from retained debt and policy.
 * @return Earliest next update time, saturated at Clock::time_point::max() on overflow.
 * Does not sleep, advance the clock or synchronize concurrent access.
 * @see [Detailed guide](cheryl-guide:runtime/simulation-timing)
 */
