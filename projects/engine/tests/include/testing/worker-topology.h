#pragma once

#include <core/engine/worker-pool.h>

#include <atomic>
#include <future>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace CE::Testing {
    inline Engine::WorkerTopology sparse_worker_topology() {
        using enum Engine::WorkerCacheType;
        Engine::WorkerTopology topology;
        topology.eligible_cpus = {2, 17, 901, 2000, 4097, 5001};
        topology.cores = {{7, {2, 2000}}, {9, {17}}, {42, {901, 4097, 5001}}};
        topology.caches = {{51, 1, Data, 32768, 64, {2, 2000}}, {52, 1, Instruction, 16384, 64, {2, 2000}},
            {88, 2, Unified, 1048576, 64, {2, 17, 2000}}, {99, 3, Unified, 16777216, 128, topology.eligible_cpus},
            {100, 2, Unified, std::nullopt, std::nullopt, {901, 4097, 5001}}};
        topology.cpu_affinity = true;
        return topology;
    }

    // Per-thread synthetic masks exercise the real pool without changing host
    // affinity. Public inputs are configured before construction; faults are atomic.
    class WorkerTopologyProvider final : public Engine::WorkerTopologyProvider {
        mutable std::mutex mutex_;
        mutable std::map<std::thread::id, std::vector<unsigned int>> masks_;
        mutable std::vector<std::vector<unsigned int>> bindings_;

    public:
        Engine::WorkerTopology snapshot = sparse_worker_topology();
        std::vector<unsigned int> inherited = snapshot.eligible_cpus;
        mutable std::atomic<unsigned int> discoveries{0};
        mutable std::atomic<unsigned int> queries{0};
        std::atomic<bool> fail_query{false};
        std::atomic<bool> fail_singleton{false};
        std::atomic<bool> fail_binding{false};
        std::atomic<bool> ignore_singleton{false};

        Engine::WorkerTopology topology() const override {
            ++discoveries;
            return snapshot;
        }
        std::vector<unsigned int> current_thread_cpus() const override {
            ++queries;
            if (fail_query)
                throw std::runtime_error("Controlled mask query failure");
            std::lock_guard lock(mutex_);
            const auto found = masks_.find(std::this_thread::get_id());
            return found == masks_.end() ? inherited : found->second;
        }
        void bind_current_thread(const std::vector<unsigned int>& cpus) const override {
            std::lock_guard lock(mutex_);
            bindings_.push_back(cpus);
            if (fail_binding || (fail_singleton && cpus.size() == 1))
                throw std::runtime_error("Controlled mask binding failure");
            if (!ignore_singleton || cpus.size() != 1)
                masks_[std::this_thread::get_id()] = cpus;
        }
        std::vector<std::vector<unsigned int>> bindings() const {
            std::lock_guard lock(mutex_);
            return bindings_;
        }
        void reset_masks() {
            std::lock_guard lock(mutex_);
            masks_.clear();
        }
    };

    // Declare after the pool so an assertion/exception releases held callbacks
    // before pool destruction joins them.
    class WorkerRelease final {
        Engine::WorkerPool& pool_;
        std::promise<void> promise_;
        bool released_ = false;

    public:
        const std::shared_future<void> ready = promise_.get_future().share();
        explicit WorkerRelease(Engine::WorkerPool& pool)
        : pool_(pool) {}
        ~WorkerRelease() {
            release();
            pool_.shutdown();
        }
        void release() {
            if (!released_) {
                promise_.set_value();
                released_ = true;
            }
        }
    };
}
