#include <core/game-framework/runtime-lifecycle.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <limits>
#include <mutex>

namespace CE::Runtime::Detail {
    struct LifecycleState {
        mutable std::mutex mutex;
        RuntimeLifecycleSnapshot current;

        void advance(RuntimeStage stage) noexcept {
            current.stage = stage;
            ++current.sequence; // Requests reserve capacity for the remaining milestones.
            current.observed_at = std::chrono::steady_clock::now();
        }
    };
}

namespace CE::Runtime {
    namespace {
        RuntimeFailure describe(const char* phase, std::exception_ptr error) noexcept {
            RuntimeFailure result;
            if (!error)
                return result;
            result.present = true;
            const auto copy = [&](auto& target, const char* text) {
                std::size_t index = 0;
                if (text) {
                    for (; index + 1 < target.size() && text[index]; ++index)
                        target[index] = text[index];
                    result.truncated |= text[index] != '\0';
                }
                target[index] = '\0';
            };
            copy(result.phase, phase);
            try {
                std::rethrow_exception(error);
            } catch (const std::exception& failure) {
                copy(result.message, failure.what());
            } catch (...) {
                copy(result.message, "Non-standard exception");
            }
            return result;
        }
    }

    RuntimeLifecycle::RuntimeLifecycle()
    : state_(std::make_shared<Detail::LifecycleState>()) {}

    RuntimeLifecycleSnapshot RuntimeLifecycle::snapshot() const {
        std::lock_guard lock(state_->mutex);
        return state_->current;
    }
    void RuntimeLifecycle::begin(std::uint64_t context, std::uint64_t session) {
        std::lock_guard lock(state_->mutex);
        state_->current.context = context;
        state_->current.session = session;
        state_->current.stage = RuntimeStage::Initializing;
        state_->current.observed_at = std::chrono::steady_clock::now();
    }
    void RuntimeLifecycle::request_stop() {
        std::lock_guard lock(state_->mutex);
        state_->current.stop_requested = true;
    }
    std::optional<RuntimeLifecycleSnapshot> RuntimeLifecycle::ready(InitializedCapabilities initialized, bool stopped) {
        std::lock_guard lock(state_->mutex);
        auto& current = state_->current;
        if (current.stage != RuntimeStage::Initializing || current.stop_requested || stopped)
            return {};
        current.initialized = initialized;
        current.reached_ready = true;
        state_->advance(RuntimeStage::Ready);
        return current;
    }
    std::optional<RuntimeLifecycleSnapshot> RuntimeLifecycle::starting(InitializedCapabilities initialized, ShutdownCause cause) {
        std::lock_guard lock(state_->mutex);
        auto& current = state_->current;
        if (current.stage != RuntimeStage::Initializing && current.stage != RuntimeStage::Ready)
            return {};
        current.initialized = initialized;
        current.cause = cause;
        if (cause == ShutdownCause::AcceptedRequest)
            current.committed_request = current.pending_request;
        else
            current.superseded_request = current.pending_request;
        current.pending_request.reset();
        current.stop_requested = true;
        state_->advance(RuntimeStage::Stopping);
        return current;
    }
    std::optional<RuntimeLifecycleSnapshot> RuntimeLifecycle::quiescing() {
        std::lock_guard lock(state_->mutex);
        if (state_->current.stage != RuntimeStage::Stopping)
            return {};
        state_->advance(RuntimeStage::Quiescing);
        return state_->current;
    }
    RuntimeLifecycleSnapshot RuntimeLifecycle::pending(std::optional<ShutdownRequest> request, InitializedCapabilities initialized) {
        std::lock_guard lock(state_->mutex);
        // Reserve cancellation, a possible Ready, and all terminal milestones.
        if (request && state_->current.sequence > std::numeric_limits<std::uint64_t>::max() - 6)
            throw CE::Exceptions::failed_operation(CE_HERE, "Lifecycle request sequence exhausted");
        state_->current.pending_request = request;
        state_->current.initialized = initialized;
        ++state_->current.sequence;
        state_->current.observed_at = std::chrono::steady_clock::now();
        return state_->current;
    }

    RuntimeLifecycleSnapshot RuntimeLifecycle::stopped(const RuntimeStats& stats, const char* phase, std::exception_ptr error) {
        const auto primary = describe(phase, error);
        std::lock_guard lock(state_->mutex);
        auto& current = state_->current;
        current.primary_failure = primary;
        current.outcome = error ? RuntimeOutcome::Failed : RuntimeOutcome::Completed;
        current.stats = stats;
        current.closed = true;
        state_->advance(RuntimeStage::Stopped);
        return current;
    }
    void RuntimeLifecycle::secondary(const char* phase, std::exception_ptr error) noexcept {
        const auto failure = describe(phase, error);
        if (!failure.present)
            return;
        std::lock_guard lock(state_->mutex);
        auto& current = state_->current;
        if (current.secondary_failure_count < current.secondary_failures.size())
            current.secondary_failures[current.secondary_failure_count] = failure;
        if (current.secondary_failure_count != std::numeric_limits<std::uint64_t>::max())
            ++current.secondary_failure_count;
    }
    void RuntimeLifecycle::close() noexcept {
        std::lock_guard lock(state_->mutex);
        state_->current.closed = true;
    }
}
