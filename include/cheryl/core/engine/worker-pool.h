#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace CE::Engine {
    namespace WorkerDetail {
        struct PoolState;
        struct GroupState;
        struct Job {
            std::move_only_function<void()> run;
            std::move_only_function<void(std::exception_ptr)> fail;
        };
    }

    struct WorkerGroupOptions {
        // Zero uses the root pool's capacity. Groups share, rather than own, threads.
        std::size_t max_concurrency = 0;
    };

    struct WorkerGroupStatus {
        std::uint64_t accepted = 0;
        std::uint64_t completed = 0;
        std::size_t pending = 0;
        std::size_t running = 0;
        bool accepting = false;
    };

    /** A workload handle sharing physical capacity with other groups in its pool.
     * Dropping a handle does not cancel accepted jobs. Close rejects new work;
     * drain waits for accepted jobs and requires close first. No forced termination.
     */
    class WorkerGroup final {
        friend class WorkerPool;
        std::weak_ptr<WorkerDetail::PoolState> pool_;
        std::shared_ptr<WorkerDetail::GroupState> group_;

        WorkerGroup(std::shared_ptr<WorkerDetail::PoolState> pool, std::shared_ptr<WorkerDetail::GroupState> group);
        void enqueue(WorkerDetail::Job job) const;

    public:
        template <typename Work>
        [[nodiscard]] auto submit(Work&& work) const -> std::future<std::invoke_result_t<std::decay_t<Work>&>> {
            using Result = std::invoke_result_t<std::decay_t<Work>&>;
            auto completion = std::make_shared<std::promise<Result>>();
            auto result = completion->get_future();
            WorkerDetail::Job job;
            job.run = [completion, work = std::forward<Work>(work)]() mutable {
                try {
                    if constexpr (std::is_void_v<Result>) {
                        std::invoke(work);
                        completion->set_value();
                    }
                    else {
                        completion->set_value(std::invoke(work));
                    }
                }
                catch (...) {
                    completion->set_exception(std::current_exception());
                }
            };
            // Separate failure delivery lets native policy rejection settle a
            // future without invoking the job or retaining its CPU captures.
            job.fail = [completion](std::exception_ptr error) { completion->set_exception(std::move(error)); };
            enqueue(std::move(job));
            return result;
        }

        void close() const;
        void drain() const;
        [[nodiscard]] WorkerGroupStatus status() const;
    };

    /** Independently owned reusable CPU workers. One thread is the modest default;
     * an application can configure a root pool and obtain multiple logical groups.
     * Destruction closes, drains accepted jobs, and joins its physical threads.
     * Blocking on jobs/drains from this same pool can exhaust its capacity; use
     * external completion checks instead. Pool ownership must outlive its jobs.
     */
    class WorkerPool final {
        std::shared_ptr<WorkerDetail::PoolState> state_;
        std::vector<std::thread> workers_;

    public:
        explicit WorkerPool(std::size_t worker_count = 1);
        ~WorkerPool();
        WorkerPool(const WorkerPool&) = delete;
        WorkerPool& operator=(const WorkerPool&) = delete;

        [[nodiscard]] WorkerGroup make_group(WorkerGroupOptions options = {});
        [[nodiscard]] std::size_t worker_count() const { return workers_.size(); }
        void close();
        void shutdown();
    };
}
