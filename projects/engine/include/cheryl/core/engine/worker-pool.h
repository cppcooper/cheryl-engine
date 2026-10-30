#pragma once

#include <cheryl/export.h>
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

    /** Whether unavailable CPU/locality requirements may degrade. */
    enum class PolicyStrength { Preferred, ///< Permit reported portable/eligible-set fallback.
        Required ///< Reject unavailable requirements or fail a job before callback entry.
    };
    /** CPU placement policy within verified eligible topology, not a residency guarantee. */
    enum class LocalityPolicy { Automatic, ///< Use reuse for resolved concurrency one, spread otherwise; no workload profiling is performed.
        Reuse, ///< Prefer the prior eligible CPU/core when load is equal.
        Spread ///< Rotate equally idle eligible CPU/core candidates.
    };

    /** Requested eligible CPUs and optional cache/locality constraints; unsupported NUMA requirements do not imply memory-placement support. */
    struct CpuPolicy {
        // Explicit logical CPU IDs, not indices into the available CPU list.
        // Empty means the pool's inherited eligible set.
        std::vector<unsigned int> cpus; ///< Explicit OS logical CPU IDs; empty selects inherited eligible capacity.
        PolicyStrength strength = PolicyStrength::Preferred; ///< Whether unsupported placement may fall back or must reject.
        bool prefer_same_worker = true; ///< Tie-break preference for the worker that last ran this group, not an affinity guarantee.
        std::optional<unsigned int> shared_cache_domain; ///< Requested provider snapshot cache ID, not a cache-level number.
        std::optional<unsigned int> numa_node; ///< Requested NUMA identity; placement is currently unavailable.
        // Unspecified uses Automatic with a provider, or ordinary affinity without
        // one. Explicit locality requirements need reported topology and binding.
        std::optional<LocalityPolicy> locality; ///< Absent chooses Automatic with a provider, ordinary affinity otherwise; explicit choices need topology/binding.
    };

    /** Owned pool-startup capability snapshot; OS eligibility can change later. */
    struct Capabilities {
        std::vector<unsigned int> available_cpus; ///< Inherited eligible OS logical CPU IDs known at pool startup.
        bool cpu_affinity = false; ///< Whether calling-thread CPU binding/readback is available.
        bool cache_topology = false; ///< Whether the provider supplied cache domains.
        bool numa_placement = false; ///< Whether memory/node placement is available; currently false.
        std::string limitations; ///< Human-readable unavailable/degraded capability reasons.
        bool core_topology = false; ///< Whether eligible physical-core coverage is complete.
    };

    /** Physical capacity and optional retained topology provider used at pool construction. */
    struct PoolOptions {
        // Zero explicitly selects eligible physical cores, or eligible logical
        // CPUs if core information is incomplete, or one thread if unknown.
        std::size_t worker_count = 1; ///< Physical thread count; zero chooses eligible cores, then logical CPUs, then one.
        std::shared_ptr<TopologyProvider> topology_provider; ///< Optional provider retained through worker shutdown; must support concurrent worker calls.
    };

    /** Logical scheduling share, concurrency limit and native policy for one group. */
    struct GroupOptions {
        // Zero uses the root pool's capacity. Groups share, rather than own, threads.
        std::size_t max_concurrency = 0; ///< Concurrent jobs allowed; zero uses pool capacity, without owning additional threads.
        unsigned int weight = 1; ///< Scheduling share in [1,1024], not a latency or completion-order promise.
        unsigned int priority = 0; ///< Share multiplier in [0,7]; effective share multiplies weight by priority+1.
        CpuPolicy cpu; ///< Requested CPU/cache/locality policy resolved by make_group().
    };

    // Common structural validation for direct C++ options and configuration loaders.
    // Does not discover topology, create threads, or apply native policy.
    CE_API void validate_group_options(const GroupOptions& options);

    /** Immutable resolved policy reported at group creation, distinct from per-job native observations. */
    struct GroupPolicy {
        GroupOptions requested; ///< Copy of caller options, including unresolved preferences.
        std::vector<unsigned int> effective_cpus; ///< Resolved eligible OS CPU IDs used by this group.
        bool affinity_supported = false; ///< Whether the pool can enforce verified CPU masks.
        std::string limitations; ///< Reasons requested capabilities are unsupported or degraded.
        bool preferred_fallback = false; ///< Whether preferred requirements degraded during creation.
        std::optional<LocalityPolicy> effective_locality; ///< Resolved locality choice, or absent when portable scheduling is used.
        std::optional<unsigned int> effective_cache_domain; ///< Resolved provider snapshot cache ID, if constrained.
        std::size_t effective_max_concurrency = 0; ///< Resolved job concurrency limit against physical capacity.
        bool core_topology = false; ///< Whether placement has complete eligible-core information.
    };

    /** Cumulative group observations and current scheduler counts; not a completion barrier. */
    struct GroupStatus {
        CE::Diagnostics::DomainId domain = 0; ///< Process-local identity of this logical group.
        CE::Diagnostics::DomainId pool = 0; ///< Process-local identity of its physical pool.
        std::uint64_t accepted = 0; ///< Jobs successfully admitted since group creation.
        std::uint64_t completed = 0; ///< Accepted jobs settled after execution/policy failure and capture release.
        std::size_t pending = 0; ///< Jobs waiting in this group queue.
        std::size_t running = 0; ///< Jobs selected and not yet fully settled.
        std::uint64_t policy_failures = 0; ///< Native policy failures observed at job entry, including reported degradation.
        std::uint64_t callback_failures = 0; ///< Invoked callbacks that threw into their result futures.
        std::uint64_t peak_pending = 0; ///< Greatest observed group queue length.
        std::uint64_t queue_nanoseconds = 0; ///< Cumulative time from enqueue to selection, in nanoseconds.
        bool accepting = false; ///< Whether this group accepts work; pool closure can independently reject.
        // Last verified singleton mask at job entry; empty after an unbound or
        // multi-CPU job. This is observation, not a completion or residency promise.
        std::optional<unsigned int> last_cpu; ///< Last verified singleton native CPU mask; empty for unbound/multiple CPUs, not proof of residency.
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

        CE_API Group(std::shared_ptr<Detail::PoolState> pool, std::shared_ptr<Detail::GroupState> group);
        CE_API void enqueue(Detail::Job job) const;
        CE_API static void record_failure(const std::weak_ptr<Detail::GroupState>& group) noexcept;

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

        CE_API void close() const;
        CE_API void drain() const;
        // Owned snapshots; status is observation only, policy is immutable after creation.
        [[nodiscard]] CE_API GroupStatus status() const;
        [[nodiscard]] CE_API GroupPolicy policy() const;
        // Explicit observer: caller holds no scheduler/context/application lock.
        // Callback failures remain future-owned; policy degradation is coalesced.
        CE_API void report_diagnostics() const noexcept;
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

        CE_API Pool(std::size_t worker_count, Detail::NativeAdapter adapter);
        CE_API Pool(PoolOptions options, Detail::NativeAdapter adapter);

    public:
        // Positive physical thread count; partial thread-start failure joins created threads.
        CE_API explicit Pool(std::size_t worker_count = 1);
        CE_API explicit Pool(PoolOptions options);
        CE_API ~Pool();
        Pool(const Pool&) = delete;
        Pool& operator=(const Pool&) = delete;

        // Thread-safe publication; invalid options or unavailable required policy throw.
        // Preferred policy can fall back as reported by the returned group's policy().
        [[nodiscard]] CE_API Group make_group(GroupOptions options = GroupOptions{});
        [[nodiscard]] std::size_t worker_count() const { return workers_.size(); }
        // Owned startup snapshot, not a live query of changing OS eligibility.
        [[nodiscard]] CE_API Capabilities capabilities() const;
        [[nodiscard]] CE_API Topology topology() const;
        [[nodiscard]] CE_API CE::Diagnostics::DomainId diagnostic_id() const noexcept;
        // Explicit observer; constructors/shutdown do not log under an outer owner lock.
        CE_API void report_diagnostics() const noexcept;
        CE_API void close();
        CE_API void shutdown();
    };
}

/**
 * @fn void CE::Workers::validate_group_options(const GroupOptions& options)
 * @brief Validate structural group settings without topology discovery or thread creation.
 * @param options Borrowed options, unchanged by validation.
 * @throws CE::Exceptions::invalid_args For invalid share/priority/enum values or malformed CPU/locality combinations.
 * Successful validation does not establish that the running machine can satisfy a required policy.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn template<typename Work> auto CE::Workers::Group::submit(Work&& work) const -> std::future<std::invoke_result_t<std::decay_t<Work>&>>
 * @brief Admit an owned callable to this workload without executing it inline.
 * @tparam Work Callable invoked without arguments; supports move-only captures and void/value/reference results.
 * @param work Callable/captures retained until execution or policy failure; borrowed captures/results require independent lifetime protection.
 * @return Future containing the result, callback exception or native-policy failure before entry.
 * @throws CE::Exceptions::failed_operation Before acceptance when the pool expired or submissions are closed. Allocation/callable preparation can also propagate.
 * Copies share one queue. Dropping the future/group does not cancel work. Keep the physical pool alive; do not block on same-pool jobs where capacity could be exhausted.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn void CE::Workers::Group::close() const
 * @brief Idempotently reject new group submissions without cancelling accepted jobs.
 * Synchronized across producers. An already destroyed pool makes this a no-op; close does not wait for completion.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn void CE::Workers::Group::drain() const
 * @brief Wait for accepted jobs and their captures to settle after close().
 * @throws CE::Exceptions::failed_operation If the group is still open or the caller is a worker of this same pool.
 * No forced cancellation occurs. An expired pool has already settled its accepted work; do not hold locks required by finishing jobs.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn CE::Workers::GroupStatus CE::Workers::Group::status() const
 * @brief Copy synchronized scheduler counts and cumulative failure observations.
 * @return Owned snapshot, still inspectable after pool destruction; it is not an admission/completion barrier.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn CE::Workers::GroupPolicy CE::Workers::Group::policy() const
 * @brief Copy immutable policy resolved when the group was created.
 * @return Requested/effective settings; later per-job policy failures are observed through status().
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn void CE::Workers::Group::report_diagnostics() const noexcept
 * @brief Emit an explicit group summary and coalesced policy degradation observations.
 * Call outside scheduler/context/application locks and sensitive native callbacks. Callback exceptions remain future-owned; reporting failures use the emergency path.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn CE::Workers::Pool::Pool(std::size_t worker_count)
 * @brief Start a pool with a strictly positive explicit physical thread count.
 * @param worker_count Number of threads; unlike PoolOptions, zero is invalid.
 * @throws CE::Exceptions::invalid_args For zero count.
 * Native thread/allocation failures propagate after joining any threads already started. No context owns this pool unless explicitly composed.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn CE::Workers::Pool::Pool(PoolOptions options)
 * @brief Discover optional topology and start physical workers.
 * @param options Owned capacity/provider settings; zero count selects eligible physical cores, eligible logical CPUs or one when unknown.
 * Provider state is retained through shutdown. Discovery/validation/thread creation can fail; partially started threads are joined. Creation does not bind the calling thread or place NUMA memory.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn CE::Workers::Pool::~Pool()
 * @brief Close, drain and join accepted work before releasing provider state.
 * The caller must settle producers and avoid destruction on this pool's workers. Jobs must be able to complete without locks or dispatch progress blocked by this caller.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn CE::Workers::Pool::Pool(const Pool&)
 * @brief Copying physical thread ownership is disabled; groups share capacity instead.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn Pool& CE::Workers::Pool::operator=(const Pool&)
 * @brief Replacing physical pool ownership through copy assignment is disabled.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn CE::Workers::Group CE::Workers::Pool::make_group(GroupOptions options)
 * @brief Publish a logical workload sharing this pool's physical capacity.
 * @param options Scheduling share/concurrency and requested CPU/locality constraints.
 * @return Copyable group with immutable resolved policy.
 * Thread-safe while the pool lives. Invalid settings, closed pool or unavailable required policy reject; preferred policy may fall back as recorded by policy().
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn std::size_t CE::Workers::Pool::worker_count() const
 * @brief Read the fixed physical worker capacity; this does not count active or pending jobs.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn CE::Workers::Capabilities CE::Workers::Pool::capabilities() const
 * @brief Copy startup capability discovery, not a live query of changing native eligibility.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn CE::Workers::Topology CE::Workers::Pool::topology() const
 * @brief Copy retained eligible CPU/core/cache discovery.
 * Topology IDs belong to this provider snapshot and do not describe NUMA allocation or guarantee cache residency.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn CE::Diagnostics::DomainId CE::Workers::Pool::diagnostic_id() const noexcept
 * @brief Read this physical pool's process-local diagnostic identity.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn void CE::Workers::Pool::report_diagnostics() const noexcept
 * @brief Emit an explicit startup/capability summary outside outer owner locks.
 * Reporting is observational and cannot prove placement speedup or completed jobs.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn void CE::Workers::Pool::close()
 * @brief Idempotently stop accepting groups/jobs while allowing accepted work to finish.
 * Does not join workers or cancel queued jobs; shutdown() completes that lifecycle.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn void CE::Workers::Pool::shutdown()
 * @brief Close, drain and join all physical workers.
 * Concurrent shutdown callers are serialized; never invoke from this same pool's worker. Completion requires accepted jobs and capture destructors to progress; no forced thread termination is provided.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */
