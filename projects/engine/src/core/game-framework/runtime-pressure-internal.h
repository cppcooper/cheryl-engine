#pragma once

#include <cheryl/export.h>
#include <core/game-framework/simulation-scheduler.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>

namespace CE::Runtime::Detail {
    // Internal diagnosed conditions, not scheduling policy or a supported module API.
    struct RuntimePressureConditions {
        bool simulation_lag = false;
        bool input_backpressure = false;

        bool operator==(const RuntimePressureConditions&) const = default;
    };

    struct RuntimePressureSnapshot {
        std::uint64_t source = 0;   // Checked process-local source/session identity; never a diagnostic counter.
        std::uint64_t revision = 0; // Zero is the baseline; only committed condition changes advance it.
        RuntimePressureConditions conditions;
        Simulation::Clock::time_point observed_at{};
        std::uint64_t dropped_batches = 0;
        std::uint64_t dropped_nanoseconds = 0;
        std::uint64_t backpressure_observations = 0;
        bool closed = false; // Closure preserves the last published conditions, including pressure.
    };

    struct RuntimePressureChanged {
        RuntimePressureSnapshot current;
        RuntimePressureConditions previous;
    };

    // State tracking only. Simulation stages diagnoses; the platform owner selects
    // copied changes. GameRuntime owns event delivery and subscription sequencing.
    // Retained closed state has no dependency on its runtime or Context.
    class RuntimePressureState final {
        mutable std::mutex mutex_;
        RuntimePressureSnapshot current_;
        bool staged_lag_ = false;
        std::uint64_t dropped_batches_ = 0;
        std::uint64_t dropped_nanoseconds_ = 0;
        std::thread::id owner_;

    public:
        CE_API RuntimePressureState();
        CE_API void bind_owner();
        CE_API void stage_lag(bool diagnosed, std::uint64_t dropped_batches, std::uint64_t dropped_nanoseconds) noexcept;
        [[nodiscard]] CE_API std::optional<RuntimePressureChanged> take_change(
            bool diagnosed_backpressure,
            std::uint64_t backpressure_observations,
            Simulation::Clock::time_point observed_at
        ) noexcept;
        [[nodiscard]] CE_API RuntimePressureSnapshot current() const;
        [[nodiscard]] CE_API RuntimePressureSnapshot owner_snapshot() const;
        CE_API void close() noexcept;
    };
}
