#include "runtime-pressure-internal.h"

#include <core/diagnostics.h>
#include <internals/exceptions.h>
#include <internals/failure-reporting.h>

#include <limits>

namespace CE::Runtime::Detail {
    RuntimePressureState::RuntimePressureState() {
        current_.source = ::CE::Diagnostics::next_domain_id();
        if (!current_.source)
            throw ::CE::Exceptions::failed_operation(CE_HERE, "Runtime pressure source identities exhausted");
    }

    void RuntimePressureState::bind_owner() {
        std::lock_guard lock(mutex_);
        if (current_.closed || owner_ != std::thread::id{})
            throw ::CE::Exceptions::failed_operation(CE_HERE, "Runtime pressure source is closed or already bound");
        owner_ = std::this_thread::get_id();
    }

    void RuntimePressureState::stage_lag(
        const bool diagnosed,
        const std::uint64_t dropped_batches,
        const std::uint64_t dropped_nanoseconds
    ) noexcept {
        try {
            std::lock_guard lock(mutex_);
            if (current_.closed)
                return;
            staged_lag_ = diagnosed;
            dropped_batches_ = dropped_batches;
            dropped_nanoseconds_ = dropped_nanoseconds;
        } catch (...) {
            ::CE::Diagnostics::report_failure("runtime pressure staging", std::current_exception());
        }
    }

    std::optional<RuntimePressureChanged> RuntimePressureState::take_change(
        const bool diagnosed_backpressure,
        const std::uint64_t backpressure_observations,
        const Simulation::Clock::time_point observed_at
    ) noexcept {
        try {
            std::lock_guard lock(mutex_);
            if (owner_ != std::this_thread::get_id())
                throw ::CE::Exceptions::failed_operation(CE_HERE, "Runtime pressure selection requires its platform owner");
            if (current_.closed)
                return std::nullopt;
            const RuntimePressureConditions selected{staged_lag_, diagnosed_backpressure};
            if (selected == current_.conditions)
                return std::nullopt;
            if (current_.revision == std::numeric_limits<std::uint64_t>::max()) {
                current_.closed = true;
                throw ::CE::Exceptions::failed_operation(CE_HERE, "Runtime pressure revisions exhausted");
            }
            RuntimePressureChanged change;
            change.previous = current_.conditions;
            ++current_.revision;
            current_.conditions = selected;
            current_.observed_at = observed_at;
            current_.dropped_batches = dropped_batches_;
            current_.dropped_nanoseconds = dropped_nanoseconds_;
            current_.backpressure_observations = backpressure_observations;
            change.current = current_;
            return change;
        } catch (...) {
            ::CE::Diagnostics::report_failure("runtime pressure selection", std::current_exception());
            return std::nullopt;
        }
    }

    RuntimePressureSnapshot RuntimePressureState::current() const {
        std::lock_guard lock(mutex_);
        return current_;
    }

    RuntimePressureSnapshot RuntimePressureState::owner_snapshot() const {
        std::lock_guard lock(mutex_);
        if (owner_ != std::thread::id{} && owner_ != std::this_thread::get_id())
            throw ::CE::Exceptions::failed_operation(CE_HERE, "Runtime pressure observation requires its platform owner");
        return current_;
    }

    void RuntimePressureState::close() noexcept {
        try {
            std::lock_guard lock(mutex_);
            current_.closed = true;
        } catch (...) {
            ::CE::Diagnostics::report_failure("runtime pressure closure", std::current_exception());
        }
    }
}
