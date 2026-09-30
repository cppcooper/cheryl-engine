#include <core/engine/platform-dispatcher.h>
#include <internals/exceptions.h>

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

    void PlatformDispatcher::open(std::function<void()> wake) {
        std::lock_guard lock(state_->mutex);
        if (state_->opened)
            throw Exceptions::failed_operation(CE_HERE, "PlatformDispatcher supports only one session");
        state_->wake = std::move(wake);
        state_->owner = std::this_thread::get_id();
        state_->opened = true;
        state_->accepting = true;
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
        }
        struct DrainGuard {
            std::shared_ptr<State> state;
            ~DrainGuard() {
                std::lock_guard lock(state->mutex);
                state->draining = false;
            }
        } guard{state_};
        // Reentrant posts stay in pending for a later drain. Futures contain
        // callback failures, so one failed request does not abort this batch.
        for (auto& request : batch)
            request(engine);
    }

    void PlatformDispatcher::close() {
        {
            std::lock_guard lock(state_->mutex);
            if (!state_->accepting)
                return;
            require_owner(*state_);
        }
        invalidate();
    }

    void PlatformDispatcher::invalidate() {
        std::vector<Task> cancelled;
        std::function<void()> wake;
        {
            std::lock_guard lock(state_->mutex);
            state_->accepting = false;
            cancelled.swap(state_->pending);
            wake.swap(state_->wake);
        }
        // Destruction outside the queue lock permits capture destructors to
        // attempt another post (which rejects) without deadlocking the queue.
    }
}
