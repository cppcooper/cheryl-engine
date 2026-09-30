#include <core/engine/worker-pool.h>
#include <internals/exceptions.h>

#include <condition_variable>
#include <deque>
#include <mutex>

namespace CE::Engine::WorkerDetail {
    struct GroupState {
        WorkerGroupOptions options;
        std::deque<Job> pending;
        WorkerGroupStatus status;
    };

    struct PoolState {
        std::mutex mutex;
        std::condition_variable wake;
        std::vector<std::shared_ptr<GroupState>> groups;
        std::size_t capacity = 0;
        std::size_t next_group = 0;
        bool accepting = true;
    };

    thread_local PoolState* current_pool = nullptr;

    bool eligible(const GroupState& group, const PoolState& pool) {
        const auto cap = group.options.max_concurrency == 0 ? pool.capacity : group.options.max_concurrency;
        return !group.pending.empty() && group.status.running < cap;
    }

    bool has_pending(const PoolState& pool) {
        for (const auto& group : pool.groups)
            if (!group->pending.empty())
                return true;
        return false;
    }

    bool has_eligible(const PoolState& pool) {
        for (const auto& group : pool.groups)
            if (eligible(*group, pool))
                return true;
        return false;
    }

    void run_worker(const std::shared_ptr<PoolState>& pool) {
        current_pool = pool.get();
        while (true) {
            std::shared_ptr<GroupState> selected;
            Job job;
            {
                std::unique_lock lock(pool->mutex);
                pool->wake.wait(lock, [&] { return has_eligible(*pool) || (!pool->accepting && !has_pending(*pool)); });
                if (!has_pending(*pool) && !pool->accepting)
                    break;
                // Fair round-robin group selection, FIFO within each eligible group.
                for (std::size_t offset = 0; offset < pool->groups.size(); ++offset) {
                    const auto index = (pool->next_group + offset) % pool->groups.size();
                    if (eligible(*pool->groups[index], *pool)) {
                        selected = pool->groups[index];
                        pool->next_group = (index + 1) % pool->groups.size();
                        break;
                    }
                }
                job = std::move(selected->pending.front());
                selected->pending.pop_front();
                --selected->status.pending;
                ++selected->status.running;
            }
            job.run(); // The owned wrapper captures callback exceptions in its future.
            // Release captures before declaring completion. Their destructors may
            // post to another group, so no scheduler lock may be held here.
            job = {};
            {
                std::lock_guard lock(pool->mutex);
                --selected->status.running;
                ++selected->status.completed;
            }
            pool->wake.notify_all();
        }
        current_pool = nullptr;
    }
}

namespace CE::Engine {
    WorkerGroup::WorkerGroup(std::shared_ptr<WorkerDetail::PoolState> pool, std::shared_ptr<WorkerDetail::GroupState> group)
    : pool_(std::move(pool)), group_(std::move(group)) {}

    void WorkerGroup::enqueue(WorkerDetail::Job job) const {
        auto pool = pool_.lock();
        if (!pool)
            throw Exceptions::failed_operation(CE_HERE, "Worker pool no longer exists");
        {
            std::lock_guard lock(pool->mutex);
            if (!pool->accepting || !group_->status.accepting)
                throw Exceptions::failed_operation(CE_HERE, "Worker group is closed");
            group_->pending.push_back(std::move(job));
            ++group_->status.accepted;
            ++group_->status.pending;
        }
        pool->wake.notify_all();
    }

    void WorkerGroup::close() const {
        auto pool = pool_.lock();
        if (!pool)
            return;
        {
            std::lock_guard lock(pool->mutex);
            group_->status.accepting = false;
        }
        pool->wake.notify_all();
    }

    void WorkerGroup::drain() const {
        auto pool = pool_.lock();
        if (!pool)
            return; // Pool destruction already drained accepted work.
        if (WorkerDetail::current_pool == pool.get())
            throw Exceptions::failed_operation(CE_HERE, "A worker cannot block draining its own pool");
        std::unique_lock lock(pool->mutex);
        if (group_->status.accepting)
            throw Exceptions::failed_operation(CE_HERE, "Close the worker group before draining it");
        pool->wake.wait(lock, [&] { return group_->pending.empty() && group_->status.running == 0; });
    }

    WorkerGroupStatus WorkerGroup::status() const {
        auto pool = pool_.lock();
        if (!pool)
            return group_->status; // Immutable after pool destruction/join.
        std::lock_guard lock(pool->mutex);
        return group_->status;
    }

    WorkerPool::WorkerPool(const std::size_t worker_count)
    : state_(std::make_shared<WorkerDetail::PoolState>()) {
        if (worker_count == 0)
            throw Exceptions::invalid_args(CE_HERE, "A worker pool requires at least one thread");
        state_->capacity = worker_count;
        try {
            workers_.reserve(worker_count);
            for (std::size_t i = 0; i < worker_count; ++i)
                workers_.emplace_back([state = state_] { WorkerDetail::run_worker(state); });
        }
        catch (...) {
            // Already-created threads must wake and join before construction fails.
            close();
            for (auto& worker : workers_)
                worker.join();
            throw;
        }
    }

    WorkerPool::~WorkerPool() {
        shutdown();
    }

    WorkerGroup WorkerPool::make_group(const WorkerGroupOptions options) {
        auto group = std::make_shared<WorkerDetail::GroupState>();
        group->options = options;
        group->status.accepting = true;
        {
            std::lock_guard lock(state_->mutex);
            if (!state_->accepting)
                throw Exceptions::failed_operation(CE_HERE, "Worker pool is closed");
            state_->groups.push_back(group);
        }
        return WorkerGroup(state_, std::move(group));
    }

    void WorkerPool::close() {
        {
            std::lock_guard lock(state_->mutex);
            state_->accepting = false;
            for (const auto& group : state_->groups)
                group->status.accepting = false;
        }
        state_->wake.notify_all();
    }

    void WorkerPool::shutdown() {
        if (WorkerDetail::current_pool == state_.get())
            throw Exceptions::failed_operation(CE_HERE, "A worker cannot join its own pool");
        close();
        for (auto& worker : workers_)
            if (worker.joinable())
                worker.join();
    }
}
