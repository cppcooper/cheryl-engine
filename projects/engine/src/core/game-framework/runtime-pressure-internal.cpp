#include "runtime-pressure-internal.h"

#include <core/diagnostics.h>
#include <internals/exceptions.h>
#include <internals/failure-reporting.h>

#include <limits>
#include <memory>
#include <utility>

namespace CE::Runtime::Detail {
    RuntimePressureSource::RuntimePressureSource() {
        current_.source = ::CE::Diagnostics::next_domain_id();
        if (!current_.source)
            throw ::CE::Exceptions::failed_operation(CE_HERE, "Runtime pressure source identities exhausted");
    }

    void RuntimePressureSource::bind_owner() {
        std::lock_guard lock(mutex_);
        if (current_.closed || owner_ != std::thread::id{})
            throw ::CE::Exceptions::failed_operation(CE_HERE, "Runtime pressure source is closed or already bound");
        owner_ = std::this_thread::get_id();
    }

    void RuntimePressureSource::stage_lag(
        const bool diagnosed,
        const std::uint64_t dropped_batches,
        const std::uint64_t dropped_nanoseconds
    ) noexcept {
        try {
            std::lock_guard lock(mutex_);
            if (current_.closed)
                return;
            staged_.simulation_lag = diagnosed;
            dropped_batches_ = dropped_batches;
            dropped_nanoseconds_ = dropped_nanoseconds;
        } catch (...) {
            ::CE::Diagnostics::report_failure("runtime pressure staging", std::current_exception());
        }
    }

    void RuntimePressureSource::publish(
        ::CE::Events::EventBus& events,
        const bool diagnosed_backpressure,
        const std::uint64_t backpressure_observations,
        const Simulation::Clock::time_point observed_at
    ) noexcept {
        try {
            RuntimePressureChanged change;
            {
                std::lock_guard lock(mutex_);
                if (owner_ != std::this_thread::get_id())
                    throw ::CE::Exceptions::failed_operation(CE_HERE, "Runtime pressure publication requires its platform owner");
                if (current_.closed || publishing_)
                    return;
                const RuntimePressureConditions selected{staged_.simulation_lag, diagnosed_backpressure};
                if (selected == current_.conditions)
                    return;
                if (current_.revision == std::numeric_limits<std::uint64_t>::max()) {
                    current_.closed = true;
                    throw ::CE::Exceptions::failed_operation(CE_HERE, "Runtime pressure revisions exhausted");
                }
                change.previous = current_.conditions;
                ++current_.revision;
                current_.conditions = selected;
                current_.observed_at = observed_at;
                current_.dropped_batches = dropped_batches_;
                current_.dropped_nanoseconds = dropped_nanoseconds_;
                current_.backpressure_observations = backpressure_observations;
                change.current = current_;
                publishing_ = true;
            }
            // One committed change per safe pass. Reentrant staging waits for the
            // next pass, keeping listener order bounded without a transition queue.
            events.notify(runtime_pressure_changed_event, change);
            std::lock_guard lock(mutex_);
            publishing_ = false;
        } catch (...) {
            ::CE::Diagnostics::report_failure("runtime pressure publication", std::current_exception());
        }
    }

    RuntimePressureSnapshot RuntimePressureSource::current() const {
        std::lock_guard lock(mutex_);
        return current_;
    }

    RuntimePressureWatch
    RuntimePressureSource::watch(::CE::Events::EventBus& events, ::CE::Events::EventChannel<RuntimePressureChanged>::Callback callback) {
        if (!callback)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Runtime pressure watch requires a callback");
        // Keep the target alive until after the source unlocks, including failed
        // bus registration. Releasing callback captures can reenter the source.
        auto target = std::make_shared<::CE::Events::EventChannel<RuntimePressureChanged>::Callback>(std::move(callback));
        std::lock_guard lock(mutex_);
        if (owner_ != std::thread::id{} && owner_ != std::this_thread::get_id())
            throw ::CE::Exceptions::failed_operation(CE_HERE, "Runtime pressure watch requires its platform owner");
        if (current_.closed)
            return {{}, current_};
        auto registration = events.register_listener(
            runtime_pressure_changed_event, [source = current_.source, target](const RuntimePressureChanged& change) {
                if (change.current.source == source)
                    (*target)(change);
            }
        );
        return {std::move(registration), current_};
    }

    void RuntimePressureSource::close() noexcept {
        try {
            std::lock_guard lock(mutex_);
            current_.closed = true;
        } catch (...) {
            ::CE::Diagnostics::report_failure("runtime pressure closure", std::current_exception());
        }
    }
}
