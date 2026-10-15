#pragma once

#include <chrono>
#include <cstddef>
#include <vector>

namespace CE::Runtime::Simulation {
    using Clock = std::chrono::steady_clock;

    enum class Mode { Variable, Fixed };
    enum class LagRecovery { DropExcessLag, VariableCatchUp };
    enum class UpdateKind { Variable, Fixed, VariableCatchUp };

    struct TimingOptions {
        // TODO: an optimization configurer may suggest limits after profiling;
        // it must preserve explicitly selected timing/input contracts.
        ::CE::Runtime::Simulation::Mode mode = ::CE::Runtime::Simulation::Mode::Variable;
        ::CE::Runtime::Simulation::Clock::duration variable_interval =
            std::chrono::duration_cast<::CE::Runtime::Simulation::Clock::duration>(
                std::chrono::nanoseconds{16666667}
            ); // Zero permits an unpaced variable loop.
        ::CE::Runtime::Simulation::Clock::duration fixed_step =
            std::chrono::duration_cast<::CE::Runtime::Simulation::Clock::duration>(std::chrono::nanoseconds{16666667});
        std::size_t max_fixed_updates = 1;
        ::CE::Runtime::Simulation::LagRecovery recovery = ::CE::Runtime::Simulation::LagRecovery::DropExcessLag;
        std::size_t fixed_updates_before_recovery = 0;                           // Bounded prefix for an overloaded VariableCatchUp cycle.
        ::CE::Runtime::Simulation::Clock::duration recovery_cap = std::chrono::milliseconds{100}; // Zero disables the recovery cap.
    };

    struct Step {
        ::CE::Runtime::Simulation::Clock::duration delta;
        ::CE::Runtime::Simulation::UpdateKind kind;
    };

    struct Batch {
        std::vector<::CE::Runtime::Simulation::Step> steps;
        ::CE::Runtime::Simulation::Clock::duration observed_elapsed{};
        ::CE::Runtime::Simulation::Clock::duration dropped{}; // Report once, on the batch's final actual update.
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
        explicit Scheduler(
            ::CE::Runtime::Simulation::TimingOptions options = ::CE::Runtime::Simulation::TimingOptions{},
            ::CE::Runtime::Simulation::Clock::time_point start = ::CE::Runtime::Simulation::Clock::now()
        );

        [[nodiscard]] const ::CE::Runtime::Simulation::TimingOptions& options() const { return options_; }
        [[nodiscard]] ::CE::Runtime::Simulation::Batch advance(::CE::Runtime::Simulation::Clock::time_point now);
        [[nodiscard]] ::CE::Runtime::Simulation::Clock::time_point next_update_at() const;
    };
}
