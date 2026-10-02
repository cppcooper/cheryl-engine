#include <core/game-framework/simulation-scheduler.h>

#include <internals/exceptions.h>

#include <algorithm>
#include <utility>

namespace CE::GFramework {
    SimulationScheduler::SimulationScheduler(
        const SimulationTimingOptions options,
        const SimulationClock::time_point start
    )
    : options_(options), observed_at_(start) {
        if (options_.mode != SimulationMode::Variable && options_.mode != SimulationMode::Fixed)
            throw Exceptions::invalid_args(CE_HERE, "Unknown simulation timing mode");
        if (options_.recovery != LagRecovery::DropExcessLag && options_.recovery != LagRecovery::VariableCatchUp)
            throw Exceptions::invalid_args(CE_HERE, "Unknown simulation lag recovery policy");
        if (options_.variable_interval < SimulationClock::duration::zero() || options_.fixed_step <= SimulationClock::duration::zero() ||
            options_.max_fixed_updates == 0 || options_.fixed_updates_before_recovery > options_.max_fixed_updates ||
            options_.recovery_cap < SimulationClock::duration::zero())
            throw Exceptions::invalid_args(CE_HERE, "Simulation timing requires positive fixed steps and bounded recovery configuration");
    }

    SimulationBatch SimulationScheduler::advance(
        const SimulationClock::time_point now
    ) {
        if (now < observed_at_)
            throw Exceptions::invalid_args(CE_HERE, "Simulation clock must advance monotonically");
        const auto previous = observed_at_.time_since_epoch();
        if (previous < SimulationClock::duration::zero() && now.time_since_epoch() > SimulationClock::duration::max() + previous)
            throw Exceptions::invalid_args(CE_HERE, "Simulation clock interval exceeds its duration range");
        const auto elapsed = now - observed_at_;
        if (elapsed > SimulationClock::duration::max() - accumulated_)
            throw Exceptions::invalid_args(CE_HERE, "Simulation elapsed time exceeds the clock duration range");
        observed_at_ = now;
        accumulated_ += elapsed;
        SimulationBatch batch;
        batch.observed_elapsed = elapsed;

        if (options_.mode == SimulationMode::Variable) {
            if (accumulated_ >= options_.variable_interval) {
                batch.steps.push_back({accumulated_, UpdateKind::Variable});
                accumulated_ = SimulationClock::duration::zero();
            }
            return batch;
        }

        const auto due = accumulated_ / options_.fixed_step;
        if (options_.recovery == LagRecovery::VariableCatchUp && std::cmp_greater(due, options_.max_fixed_updates)) {
            // The fixed prefix is explicit, never inferred from the stall length.
            for (std::size_t i = 0; i < options_.fixed_updates_before_recovery; ++i) {
                batch.steps.push_back({options_.fixed_step, UpdateKind::Fixed});
                accumulated_ -= options_.fixed_step;
            }
            const auto recovery =
                options_.recovery_cap == SimulationClock::duration::zero() ? accumulated_ : std::min(accumulated_, options_.recovery_cap);
            batch.steps.push_back({recovery, UpdateKind::VariableCatchUp});
            batch.dropped = accumulated_ - recovery;
            accumulated_ = SimulationClock::duration::zero();
            return batch;
        }

        while (accumulated_ >= options_.fixed_step && batch.steps.size() < options_.max_fixed_updates) {
            batch.steps.push_back({options_.fixed_step, UpdateKind::Fixed});
            accumulated_ -= options_.fixed_step;
        }
        // Drop only whole-step debt. Keeping the remainder preserves the phase
        // of future fixed updates without demanding an unbounded catch-up burst.
        const auto remainder = accumulated_ % options_.fixed_step;
        batch.dropped = accumulated_ - remainder;
        accumulated_ = remainder;
        return batch;
    }

    SimulationClock::time_point SimulationScheduler::next_update_at() const {
        const auto interval = options_.mode == SimulationMode::Variable ? options_.variable_interval : options_.fixed_step;
        const auto remaining = interval > accumulated_ ? interval - accumulated_ : SimulationClock::duration::zero();
        return observed_at_ > SimulationClock::time_point::max() - remaining ? SimulationClock::time_point::max()
                                                                             : observed_at_ + remaining;
    }
}
