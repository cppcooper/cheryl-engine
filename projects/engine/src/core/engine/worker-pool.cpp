#include <core/engine/worker-pool.h>
#include <internals/exceptions.h>
#include <internals/compile-time-logging.hpp>
#include "worker-affinity.h"
#include "worker-topology-internal.h"
#include "worker-placement.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <iterator>
#include <atomic>

namespace CE::Engine::WorkerDetail {
    struct GroupState {
        WorkerGroupOptions options;
        WorkerGroupPolicy policy;
        WorkerGroupPlacement placement;
        long double credit = 0;
        std::optional<std::size_t> last_worker;
        std::deque<Job> pending;
        WorkerGroupStatus status;
        std::atomic<std::uint64_t> callback_failures{0};
        std::atomic<std::uint64_t> reported_policy_failures{0};
        std::atomic<bool> reported_fallback{false};
    };

    struct PoolState {
        std::mutex mutex;
        std::condition_variable wake;
        std::vector<std::shared_ptr<GroupState>> groups;
        std::size_t capacity = 0;
        WorkerCapabilities capabilities;
        WorkerNativeAdapter native;
        std::shared_ptr<WorkerTopologyProvider> topology_provider;
        WorkerTopology topology;
        WorkerPlacement placement;
        bool accepting = true;
        const Diagnostics::DomainId domain = Diagnostics::next_domain_id();
        mutable std::atomic<bool> reported_startup{false};
    };

    thread_local PoolState* current_pool = nullptr;

    WorkerPoolOptions fixed_pool_options(const std::size_t worker_count) {
        if (worker_count == 0)
            throw Exceptions::invalid_args(CE_HERE, "A worker pool requires at least one thread");
        return WorkerPoolOptions{worker_count, {}};
    }

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

    void run_worker(const std::shared_ptr<PoolState>& pool, const std::size_t worker_index, std::vector<unsigned int> current_mask) {
        bool mask_known = !pool->topology_provider;
        std::vector<unsigned int> target_mask;
        current_pool = pool.get();
        while (true) {
            std::shared_ptr<GroupState> selected;
            Job job;
            std::optional<std::size_t> target;
            {
                std::unique_lock lock(pool->mutex);
                pool->wake.wait(lock, [&] { return has_eligible(*pool) || (!pool->accepting && !has_pending(*pool)); });
                if (!has_pending(*pool) && !pool->accepting)
                    break;
                // Smooth weighted selection accounts only for currently eligible
                // groups. Idle/capped groups do not accumulate a future burst.
                long double total_weight = 0;
                for (const auto& group : pool->groups) {
                    if (!eligible(*group, *pool)) {
                        group->credit = 0;
                        continue;
                    }
                    const auto share = group->options.weight * (group->options.priority + 1);
                    total_weight += share;
                    group->credit += share;
                    const bool local = group->options.cpu.prefer_same_worker && group->last_worker == worker_index;
                    const bool selected_local =
                        selected && selected->options.cpu.prefer_same_worker && selected->last_worker == worker_index;
                    if (!selected || group->credit > selected->credit || (group->credit == selected->credit && local && !selected_local))
                        selected = group;
                }
                selected->credit -= total_weight;
                selected->last_worker = worker_index;
                if (selected->policy.effective_locality)
                    target = reserve_cpu(pool->placement, selected->placement, selected->policy,
                                         mask_known && current_mask.size() == 1 ? std::optional{current_mask.front()} : std::nullopt);
                job = std::move(selected->pending.front());
                selected->pending.pop_front();
                --selected->status.pending;
                ++selected->status.running;
                selected->status.queue_nanoseconds += std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - job.queued_at).count();
            }
            std::exception_ptr policy_error;
            if (pool->capabilities.cpu_affinity) {
                try {
                    if (target)
                        target_mask.assign(1, pool->placement.cpus[*target].cpu);
                    const auto& desired = target ? target_mask : selected->policy.effective_cpus;
                    if (!mask_known || current_mask != desired) {
                        apply_affinity(pool->native, desired);
                        current_mask = desired;
                        mask_known = true;
                    } else if (selected->options.cpu.strength == WorkerPolicyStrength::Required &&
                               pool->native.query_affinity() != desired) {
                        // Restrictions may change while the pool is alive.
                        apply_affinity(pool->native, desired);
                    }
                } catch (...) {
                    policy_error = std::current_exception();
                    mask_known = false;
                    if (selected->options.cpu.strength == WorkerPolicyStrength::Preferred) {
                        try {
                            apply_affinity(pool->native, pool->capabilities.available_cpus);
                            current_mask = pool->capabilities.available_cpus;
                            mask_known = true;
                            policy_error = {};
                        } catch (...) {
                            policy_error = std::current_exception();
                        }
                    }
                    std::lock_guard lock(pool->mutex);
                    ++selected->status.policy_failures;
                }
            }
            {
                std::lock_guard lock(pool->mutex);
                selected->status.last_cpu = pool->capabilities.cpu_affinity && mask_known && !policy_error && current_mask.size() == 1 ?
                                            std::optional{current_mask.front()} : std::nullopt;
                selected->placement.last_target = selected->status.last_cpu ?
                                                  find_cpu(pool->placement, *selected->status.last_cpu) : std::nullopt;
                if (target && (policy_error || selected->placement.last_target != target)) {
                    release_cpu(pool->placement, *target);
                    target.reset();
                }
            }
            if (policy_error)
                job.fail(policy_error); // Never run a job under an unverified mask.
            else
                job.run(); // Callback exceptions reach their future.
            // Release captures before declaring completion. Their destructors may
            // post to another group, so no scheduler lock may be held here.
            job = {};
            {
                std::lock_guard lock(pool->mutex);
                --selected->status.running;
                ++selected->status.completed;
                if (target)
                    release_cpu(pool->placement, *target);
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
            job.queued_at = std::chrono::steady_clock::now();
            group_->pending.push_back(std::move(job));
            ++group_->status.accepted;
            ++group_->status.pending;
            group_->status.peak_pending = std::max<std::uint64_t>(group_->status.peak_pending, group_->status.pending);
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
        WorkerGroupStatus result;
        if (!pool)
            result = group_->status; // Immutable after pool destruction/join.
        else {
            std::lock_guard lock(pool->mutex);
            result = group_->status;
        }
        result.callback_failures = group_->callback_failures.load(std::memory_order_relaxed);
        return result;
    }

    void WorkerGroup::record_failure(const std::weak_ptr<WorkerDetail::GroupState>& group) noexcept {
        if (const auto retained = group.lock())
            retained->callback_failures.fetch_add(1, std::memory_order_relaxed);
    }

    void WorkerGroup::report_diagnostics() const noexcept {
        try {
            const auto snapshot = status();
            CE_LOG_DEBUG(CE::enginelog, "subsystem=workers domain={} pool={} operation=group_summary accepted={} completed={} pending={} running={} callback_failures={} policy_failures={} peak_pending={} queue_ns={}",
                         snapshot.domain, snapshot.pool, snapshot.accepted, snapshot.completed, snapshot.pending, snapshot.running,
                         snapshot.callback_failures, snapshot.policy_failures, snapshot.peak_pending, snapshot.queue_nanoseconds);
            if (group_->policy.preferred_fallback && !group_->reported_fallback.exchange(true))
                CE_LOG_WARN(CE::enginelog, "subsystem=workers domain={} pool={} operation=preferred_policy outcome=fallback cpus={}",
                            snapshot.domain, snapshot.pool, group_->policy.effective_cpus.size());
            auto reported = group_->reported_policy_failures.load();
            while (snapshot.policy_failures > reported) {
                if (group_->reported_policy_failures.compare_exchange_weak(reported, snapshot.policy_failures)) {
                    CE_LOG_WARN(CE::enginelog, "subsystem=workers domain={} pool={} operation=native_policy outcome=degraded failures={} new={} strength={}",
                                snapshot.domain, snapshot.pool, snapshot.policy_failures, snapshot.policy_failures - reported,
                                static_cast<int>(group_->options.cpu.strength));
                    break;
                }
            }
        } catch (...) {
            Diagnostics::report_failure("worker diagnostic snapshot", std::current_exception());
        }
    }

    WorkerPool::WorkerPool(const std::size_t worker_count)
    : WorkerPool(worker_count, WorkerDetail::native_worker_adapter()) {}

    WorkerPool::WorkerPool(const std::size_t worker_count, WorkerDetail::WorkerNativeAdapter adapter)
    : WorkerPool(WorkerDetail::fixed_pool_options(worker_count), std::move(adapter)) {}

    WorkerPool::WorkerPool(WorkerPoolOptions options)
    : WorkerPool(std::move(options), WorkerDetail::native_worker_adapter()) {}

    WorkerPool::WorkerPool(WorkerPoolOptions options, WorkerDetail::WorkerNativeAdapter adapter)
    : state_(std::make_shared<WorkerDetail::PoolState>()) {
        state_->native = std::move(adapter);
        state_->topology_provider = std::move(options.topology_provider);
        if (state_->topology_provider) {
            state_->topology = WorkerDetail::prepare_topology(*state_->topology_provider);
            const auto provider = state_->topology_provider;
            state_->native.cpu_affinity = state_->topology.cpu_affinity;
            state_->native.query_affinity = [provider] { return provider->current_thread_cpus(); };
            state_->native.set_affinity = [provider](const std::vector<unsigned int>& cpus) { provider->bind_current_thread(cpus); };
            state_->capabilities.available_cpus = state_->topology.eligible_cpus;
            state_->capabilities.cpu_affinity = state_->topology.cpu_affinity;
            state_->capabilities.cache_topology = !state_->topology.caches.empty();
            state_->capabilities.core_topology = WorkerDetail::complete_core_topology(state_->topology);
            state_->capabilities.limitations = state_->topology.limitations;
        } else {
            state_->capabilities = WorkerDetail::discover_capabilities(state_->native);
            state_->topology.eligible_cpus = state_->capabilities.available_cpus;
            state_->topology.cpu_affinity = state_->capabilities.cpu_affinity;
            state_->topology.limitations = state_->capabilities.limitations;
        }
        const auto automatic_count = state_->capabilities.core_topology ? state_->topology.cores.size() :
                                     std::max<std::size_t>(1, state_->capabilities.available_cpus.size());
        const auto worker_count = options.worker_count == 0 ? automatic_count : options.worker_count;
        state_->capacity = worker_count;
        state_->placement = WorkerDetail::make_placement(state_->topology);
        try {
            workers_.reserve(worker_count);
            for (std::size_t i = 0; i < worker_count; ++i)
                workers_.push_back(state_->native.start_thread([state = state_, i, mask = state_->capabilities.available_cpus]() mutable {
                    WorkerDetail::run_worker(state, i, std::move(mask));
                }));
        } catch (...) {
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

    std::unique_ptr<WorkerPool> WorkerDetail::WorkerPoolAccess::create(const std::size_t worker_count, WorkerNativeAdapter adapter) {
        return std::unique_ptr<WorkerPool>(new WorkerPool(worker_count, std::move(adapter)));
    }

    std::unique_ptr<WorkerPool> WorkerDetail::WorkerPoolAccess::create(WorkerPoolOptions options, WorkerNativeAdapter adapter) {
        return std::unique_ptr<WorkerPool>(new WorkerPool(std::move(options), std::move(adapter)));
    }

    WorkerGroupPolicy WorkerGroup::policy() const {
        return group_->policy; // Policy is immutable after group publication.
    }

    WorkerCapabilities WorkerPool::capabilities() const {
        return state_->capabilities; // Inherited capability snapshot; restrictions may later change.
    }

    WorkerTopology WorkerPool::topology() const {
        return state_->topology;
    }

    Diagnostics::DomainId WorkerPool::diagnostic_id() const noexcept {
        return state_->domain;
    }

    void WorkerPool::report_diagnostics() const noexcept {
        if (!state_->reported_startup.exchange(true))
            CE_LOG_INFO(CE::enginelog, "subsystem=workers domain={} operation=pool_capabilities capacity={} affinity={} cpus={} cache_topology={} numa={}",
                        state_->domain, state_->capacity, state_->capabilities.cpu_affinity, state_->capabilities.available_cpus.size(),
                        state_->capabilities.cache_topology, state_->capabilities.numa_placement);
    }

    WorkerGroup WorkerPool::make_group(const WorkerGroupOptions options) {
        auto policy = WorkerDetail::resolve_policy(options, state_->capabilities, state_->topology, state_->placement,
                                                  state_->topology_provider != nullptr, state_->capacity);
        auto group = std::make_shared<WorkerDetail::GroupState>();
        group->status.domain = Diagnostics::next_domain_id();
        group->status.pool = state_->domain;
        group->options = options;
        group->policy = std::move(policy);
        group->placement = WorkerDetail::make_group_placement(group->policy, state_->placement);
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
        std::lock_guard shutdown_lock(shutdown_mutex_);
        close();
        for (auto& worker : workers_)
            if (worker.joinable())
                worker.join();
    }
}
