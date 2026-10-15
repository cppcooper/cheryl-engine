#pragma once
#include <core/diagnostics.h>
#include <core/engine/worker-topology.h>
#include <chrono>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace CE::Workers {
    namespace Detail {
        struct PoolState;
        struct GroupState;
        struct NativeAdapter;
        struct PoolAccess;
        struct Job {
            std::move_only_function<void()> run;
            std::move_only_function<void(std::exception_ptr)> fail;
            std::chrono::steady_clock::time_point queued_at;
        };
    }

    enum class PolicyStrength { Preferred, Required };
    enum class LocalityPolicy { Automatic, Reuse, Spread };

    struct CpuPolicy {
        // Explicit logical CPU IDs, not indices into the available CPU list.
        // Empty means the pool's inherited eligible set.
        std::vector<unsigned int> cpus;
        PolicyStrength strength = PolicyStrength::Preferred;
        bool prefer_same_worker = true;
        std::optional<unsigned int> shared_cache_domain;
        std::optional<unsigned int> numa_node;
        // Unspecified uses Automatic with a provider, or ordinary affinity without
        // one. Explicit locality requirements need reported topology and binding.
        std::optional<LocalityPolicy> locality;
    };

    struct Capabilities {
        std::vector<unsigned int> available_cpus;
        bool cpu_affinity = false;
        bool cache_topology = false;
        bool numa_placement = false;
        std::string limitations;
        bool core_topology = false;
    };

    struct PoolOptions {
        // Zero explicitly selects eligible physical cores, or eligible logical
        // CPUs if core information is incomplete, or one thread if unknown.
        std::size_t worker_count = 1;
        std::shared_ptr<TopologyProvider> topology_provider;
    };

    struct GroupOptions {
        // Zero uses the root pool's capacity. Groups share, rather than own, threads.
        std::size_t max_concurrency = 0;
        unsigned int weight = 1;   // 1..1024, weighted fair job selection.
        unsigned int priority = 0; // 0..7, multiplies scheduling share by priority+1.
        CpuPolicy cpu;
    };

    // Common structural validation for direct C++ options and configuration loaders.
    // Does not discover topology, create threads, or apply native policy.
    void validate_group_options(const GroupOptions& options);

    struct GroupPolicy {
        GroupOptions requested;
        std::vector<unsigned int> effective_cpus;
        bool affinity_supported = false;
        std::string limitations;
        bool preferred_fallback = false;
        std::optional<LocalityPolicy> effective_locality;
        std::optional<unsigned int> effective_cache_domain;
        std::size_t effective_max_concurrency = 0;
        bool core_topology = false;
    };

    struct GroupStatus {
        CE::Diagnostics::DomainId domain = 0;
        CE::Diagnostics::DomainId pool = 0;
        std::uint64_t accepted = 0;
        std::uint64_t completed = 0;
        std::size_t pending = 0;
        std::size_t running = 0;
        std::uint64_t policy_failures = 0;
        std::uint64_t callback_failures = 0;
        std::uint64_t peak_pending = 0;
        std::uint64_t queue_nanoseconds = 0;
        bool accepting = false;
        // Last verified singleton mask at job entry; empty after an unbound or
        // multi-CPU job. This is observation, not a completion or residency promise.
        std::optional<unsigned int> last_cpu;
    };

    /** A workload handle sharing physical capacity with other groups in its pool.
     * Dropping a handle does not cancel accepted jobs. Close rejects new work;
     * drain waits for accepted jobs and requires close first. No forced termination.
     * Copies share one group. Submission/close/status are synchronized across producers;
     * keep the physical pool alive until work settles. Same-pool drainage rejects.
     */
    class Group final {
        friend class Pool;
        std::weak_ptr<Detail::PoolState> pool_;
        std::shared_ptr<Detail::GroupState> group_;

        Group(std::shared_ptr<Detail::PoolState> pool, std::shared_ptr<Detail::GroupState> group);
        void enqueue(Detail::Job job) const;
        static void record_failure(const std::weak_ptr<Detail::GroupState>& group) noexcept;

    public:
        // Owns the callable/captures until execution or policy failure. Closure/pool
        // expiry throws before acceptance; accepted callback/policy failure reaches
        // the future. Borrowed captures/results need their own lifetime protection.
        template <typename Work> [[nodiscard]] auto submit(Work&& work) const -> std::future<std::invoke_result_t<std::decay_t<Work>&>> {
            using Result = std::invoke_result_t<std::decay_t<Work>&>;
            auto completion = std::make_shared<std::promise<Result>>();
            auto result = completion->get_future();
            Detail::Job job;
            job.run = [completion, tracked = std::weak_ptr<Detail::GroupState>(group_), work = std::forward<Work>(work)]() mutable {
                try {
                    if constexpr (std::is_void_v<Result>) {
                        std::invoke(work);
                        completion->set_value();
                    } else {
                        completion->set_value(std::invoke(work));
                    }
                } catch (...) {
                    record_failure(tracked);
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
        // Owned snapshots; status is observation only, policy is immutable after creation.
        [[nodiscard]] GroupStatus status() const;
        [[nodiscard]] GroupPolicy policy() const;
        // Explicit observer: caller holds no scheduler/context/application lock.
        // Callback failures remain future-owned; policy degradation is coalesced.
        void report_diagnostics() const noexcept;
    };

    /** Independently owned reusable CPU workers. One thread is the modest default;
     * an application can configure a root pool and obtain multiple logical groups.
     * Destruction closes, drains accepted jobs, and joins its physical threads.
     * Blocking on jobs/drains from this same pool can exhaust its capacity; use
     * external completion checks instead. Pool ownership must outlive its jobs.
     */
    class Pool final {
        friend struct Detail::PoolAccess;
        std::shared_ptr<Detail::PoolState> state_;
        std::vector<std::thread> workers_;
        std::mutex shutdown_mutex_;

        Pool(std::size_t worker_count, Detail::NativeAdapter adapter);
        Pool(PoolOptions options, Detail::NativeAdapter adapter);

    public:
        // Positive physical thread count; partial thread-start failure joins created threads.
        explicit Pool(std::size_t worker_count = 1);
        explicit Pool(PoolOptions options);
        ~Pool();
        Pool(const Pool&) = delete;
        Pool& operator=(const Pool&) = delete;

        // Thread-safe publication; invalid options or unavailable required policy throw.
        // Preferred policy can fall back as reported by the returned group's policy().
        [[nodiscard]] Group make_group(GroupOptions options = GroupOptions{});
        [[nodiscard]] std::size_t worker_count() const { return workers_.size(); }
        // Owned startup snapshot, not a live query of changing OS eligibility.
        [[nodiscard]] Capabilities capabilities() const;
        [[nodiscard]] Topology topology() const;
        [[nodiscard]] CE::Diagnostics::DomainId diagnostic_id() const noexcept;
        // Explicit observer; constructors/shutdown do not log under an outer owner lock.
        void report_diagnostics() const noexcept;
        void close();
        void shutdown();
    };
}
