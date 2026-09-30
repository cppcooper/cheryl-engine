#include <core/engine/platform-queue.h>
#include <internals/exceptions.h>

namespace CE::Engine {
    void PlatformTaskQueue::enqueue(Task request) {
        std::function<void()> wake;
        {
            std::lock_guard lock(mutex_);
            if (!accepting_)
                throw Exceptions::failed_operation(CE_HERE, "Platform requests require an active runtime");
            // Copy the wake before publishing, so allocation failure cannot orphan a request.
            wake = wake_;
            pending_.push_back(std::move(request));
        }
        // Scheduler predicates may inspect this queue while holding their own mutex.
        // Never take that scheduler mutex while still holding the queue mutex.
        if (wake)
            wake();
    }

    bool PlatformTaskQueue::has_pending() const {
        std::lock_guard lock(mutex_);
        return !pending_.empty();
    }

    void PlatformTaskQueue::open(std::function<void()> wake) {
        std::lock_guard lock(mutex_);
        wake_ = std::move(wake);
        owner_ = std::this_thread::get_id();
        accepting_ = true;
    }

    void PlatformTaskQueue::require_owner() const {
        if (owner_ != std::this_thread::get_id())
            throw Exceptions::failed_operation(CE_HERE, "Platform requests must execute on their owner thread");
    }

    void PlatformTaskQueue::drain(EngineContext& engine) {
        std::vector<Task> batch;
        {
            std::lock_guard lock(mutex_);
            require_owner();
            if (!accepting_)
                return;
            batch.swap(pending_);
        }
        // A callback may submit another request, which belongs to the next drain.
        // Exceptions are captured by packaged_task rather than unwinding the loop.
        for (auto& request : batch)
            request(engine);
    }

    void PlatformTaskQueue::close() {
        std::vector<Task> cancelled;
        std::function<void()> wake;
        {
            std::lock_guard lock(mutex_);
            if (!accepting_)
                return;
            require_owner();
            accepting_ = false;
            cancelled.swap(pending_);
            wake.swap(wake_);
        }
        // Destroy captured data outside the lock, on the platform and before game cleanup.
    }
}
