#pragma once

#include <cheryl/export.h>
#include <core/game-framework/simulation-scheduler.h>
#include <core/subsystems/event-bus.h>

#include <cstdint>
#include <mutex>
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

    inline const ::CE::Events::EventChannel<RuntimePressureChanged> runtime_pressure_changed_event{"runtime-pressure-changed"};

    struct RuntimePressureWatch {
        ::CE::Events::EventBus::Registration registration;
        RuntimePressureSnapshot seed; // Current state, not a replayed transition; queued consumers reject older/equal revisions.
    };

    // Only lag staging and snapshot reads cross owners. Binding, publication and
    // watch attachment belong to the platform owner (or synchronized pre-run setup).
    // No bus/Context is retained, so a retained closed source can be inspected safely.
    // Staging coalesces intermediate diagnoses before the next platform publication.
    class RuntimePressureSource final {
        mutable std::mutex mutex_;
        RuntimePressureSnapshot current_;
        RuntimePressureConditions staged_;
        std::uint64_t dropped_batches_ = 0;
        std::uint64_t dropped_nanoseconds_ = 0;
        std::thread::id owner_;
        bool publishing_ = false;

    public:
        CE_API RuntimePressureSource();
        CE_API void bind_owner();
        CE_API void stage_lag(bool diagnosed, std::uint64_t dropped_batches, std::uint64_t dropped_nanoseconds) noexcept;
        CE_API void publish(
            ::CE::Events::EventBus& events,
            bool diagnosed_backpressure,
            std::uint64_t backpressure_observations,
            Simulation::Clock::time_point observed_at
        ) noexcept;
        [[nodiscard]] CE_API RuntimePressureSnapshot current() const;
        [[nodiscard]] CE_API RuntimePressureWatch
        watch(::CE::Events::EventBus& events, ::CE::Events::EventChannel<RuntimePressureChanged>::Callback callback);
        CE_API void close() noexcept;
    };
}
