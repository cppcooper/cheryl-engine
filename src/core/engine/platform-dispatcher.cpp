#include <core/engine/platform-dispatcher.h>
#include <internals/exceptions.h>
#include <internals/compile-time-logging.hpp>
#include <algorithm>

namespace CE::Engine {
    PlatformDispatcher::~PlatformDispatcher() {
        // Normal runtime teardown already closed on the owner. Invalidation also
        // makes a saved endpoint safe if an unopened dispatcher is destroyed.
        invalidate();
    }

    void PlatformDispatcher::enqueue(const std::shared_ptr<State>& state, Task request) {
        std::function<void()> wake;
        {
            std::lock_guard lock(state->mutex);
            if (!state->accepting)
                throw Exceptions::failed_operation(CE_HERE, "Platform requests require an active runtime");
            // Copy before publication: allocation failure must not orphan a request.
            wake = state->wake;
            state->pending.push_back(std::move(request));
            ++state->diagnostics.accepted;
            state->diagnostics.peak_pending = std::max<std::uint64_t>(state->diagnostics.peak_pending, state->pending.size());
        }
        // Wake callbacks own their scheduler and never borrow the runtime. A copy
        // made just before close can therefore safely run just after close.
        if (wake)
            wake();
    }

    bool PlatformDispatcher::has_pending() const {
        std::lock_guard lock(state_->mutex);
        return !state_->pending.empty();
    }

    Diagnostics::DispatchStats PlatformDispatcher::diagnostics() const {
        std::lock_guard lock(state_->mutex);
        auto result = state_->diagnostics;
        result.pending = state_->pending.size();
        result.failures = state_->failures.load(std::memory_order_relaxed);
        return result;
    }

    void PlatformDispatcher::open(std::function<void()> wake) {
        {
            std::lock_guard lock(state_->mutex);
            if (state_->opened)
                throw Exceptions::failed_operation(CE_HERE, "PlatformDispatcher supports only one session");
            state_->wake = std::move(wake);
            state_->owner = std::this_thread::get_id();
            state_->opened = true;
            state_->accepting = true;
        }
        CE_LOG_DEBUG(CE::enginelog, "subsystem=platform_dispatch domain={} operation=open outcome=ready", state_->diagnostics.domain);
    }

    void PlatformDispatcher::require_owner(const State& state) {
        if (state.owner != std::this_thread::get_id())
            throw Exceptions::failed_operation(CE_HERE, "Platform requests must execute on their owner thread");
    }

    void PlatformDispatcher::drain(EngineContext& engine) {
        std::vector<Task> batch;
        {
            std::lock_guard lock(state_->mutex);
            require_owner(*state_);
            if (!state_->accepting || state_->draining)
                return;
            batch.swap(state_->pending);
            state_->draining = true;
            state_->diagnostics.running = batch.size();
        }
        struct DrainGuard {
            std::shared_ptr<State> state;
            std::uint64_t completed = 0;
            ~DrainGuard() {
                std::lock_guard lock(state->mutex);
                state->diagnostics.completed += completed;
                state->diagnostics.running = 0;
                state->draining = false;
            }
        } guard{state_};
        // Reentrant posts stay in pending for a later drain. Futures contain
        // callback failures, so one failed request does not abort this batch.
        for (auto& request : batch) {
            request(engine);
            ++guard.completed;
        }
    }

    void PlatformDispatcher::close() {
        {
            std::lock_guard lock(state_->mutex);
            if (!state_->accepting)
                return;
            require_owner(*state_);
        }
        invalidate();
        const auto summary = diagnostics();
        CE_LOG_DEBUG(CE::enginelog, "subsystem=platform_dispatch domain={} operation=close accepted={} completed={} cancelled={} failures={} peak_pending={}",
                     summary.domain, summary.accepted, summary.completed, summary.cancelled, summary.failures, summary.peak_pending);
        if (summary.peak_pending >= 1024)
            CE_LOG_WARN(CE::enginelog, "subsystem=platform_dispatch domain={} operation=queue_growth outcome=high_water peak_pending={}",
                        summary.domain, summary.peak_pending);
    }

    void PlatformDispatcher::invalidate() {
        std::vector<Task> cancelled;
        std::function<void()> wake;
        {
            std::lock_guard lock(state_->mutex);
            state_->accepting = false;
            cancelled.swap(state_->pending);
            state_->diagnostics.cancelled += cancelled.size();
            wake.swap(state_->wake);
        }
        // Destruction outside the queue lock permits capture destructors to
        // attempt another post (which rejects) without deadlocking the queue.
    }
}
