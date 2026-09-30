#pragma once

#include <chrono>
#include <cstddef>
#include <vector>

namespace CE::GFramework {
    using SimulationClock = std::chrono::steady_clock;

    enum class SimulationMode { Variable, Fixed };
    enum class LagRecovery { DropExcessLag, VariableCatchUp };
    enum class UpdateKind { Variable, Fixed, VariableCatchUp };

    struct SimulationTimingOptions {
        // TODO: an optimization configurer may suggest limits after profiling;
        // it must preserve explicitly selected timing/input contracts.
        SimulationMode mode = SimulationMode::Variable;
        SimulationClock::duration variable_interval = std::chrono::duration_cast<SimulationClock::duration>(
            std::chrono::nanoseconds{16666667}); // Zero permits an unpaced variable loop.
        SimulationClock::duration fixed_step = std::chrono::duration_cast<SimulationClock::duration>(
            std::chrono::nanoseconds{16666667});
        std::size_t max_fixed_updates = 1;
        LagRecovery recovery = LagRecovery::DropExcessLag;
        std::size_t fixed_updates_before_recovery = 0; // Bounded prefix for an overloaded VariableCatchUp cycle.
        SimulationClock::duration recovery_cap = std::chrono::milliseconds{100}; // Zero disables the recovery cap.
    };

    struct SimulationStep {
        SimulationClock::duration delta;
        UpdateKind kind;
    };

    struct SimulationBatch {
        std::vector<SimulationStep> steps;
        SimulationClock::duration observed_elapsed{};
        SimulationClock::duration dropped{}; // Report once, on the batch's final actual update.
    };

    /** Clock input is supplied by the owner; scheduling never sleeps or polls input.
     * advance() selects a bounded batch once. Time spent running that batch enters
     * the next cycle, so expensive updates cannot extend the current batch forever.
     */
    class SimulationScheduler final {
        SimulationTimingOptions options_;
        SimulationClock::time_point observed_at_;
        SimulationClock::duration accumulated_{};

    public:
        explicit SimulationScheduler(
            SimulationTimingOptions options = SimulationTimingOptions{},
            SimulationClock::time_point start = SimulationClock::now()
        );

        [[nodiscard]] const SimulationTimingOptions& options() const { return options_; }
        [[nodiscard]] SimulationBatch advance(SimulationClock::time_point now);
        [[nodiscard]] SimulationClock::time_point next_update_at() const;
    };
}
