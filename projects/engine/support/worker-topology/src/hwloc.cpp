#include <core/engine/worker-topology/hwloc.h>
#include <internals/exceptions.h>

#include <hwloc.h>

#include <algorithm>
#include <cerrno>
#include <iterator>
#include <memory>
#include <new>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

#if HWLOC_API_VERSION < 0x00020500 || HWLOC_API_VERSION >= 0x00030000
#error Cheryl worker topology requires hwloc 2.5 or newer with the 2.x API
#endif

namespace CE::Engine {
    namespace {
        using NativeTopology = std::unique_ptr<std::remove_pointer_t<hwloc_topology_t>, decltype(&hwloc_topology_destroy)>;
        using Bitmap = std::unique_ptr<std::remove_pointer_t<hwloc_bitmap_t>, decltype(&hwloc_bitmap_free)>;

        [[noreturn]] void fail(const std::string& operation) {
            const auto error = errno;
            throw Exceptions::failed_operation(
                CE_HERE, "hwloc " + operation + " failed: " + std::error_code(error, std::generic_category()).message()
            );
        }

        Bitmap bitmap() {
            auto result = hwloc_bitmap_alloc();
            if (!result)
                throw std::bad_alloc();
            return Bitmap(result, hwloc_bitmap_free);
        }

        std::vector<unsigned int> cpu_ids(const hwloc_const_bitmap_t mask) {
            if (!mask || hwloc_bitmap_weight(mask) < 0)
                throw Exceptions::failed_operation(CE_HERE, "hwloc returned an invalid or unbounded CPU mask");
            std::vector<unsigned int> result;
            for (int cpu = hwloc_bitmap_first(mask); cpu >= 0; cpu = hwloc_bitmap_next(mask, cpu))
                result.push_back(static_cast<unsigned int>(cpu));
            return result;
        }

        NativeTopology discover() {
            const auto version = hwloc_get_api_version();
            if (version < 0x00020500 || version >= 0x00030000)
                throw Exceptions::failed_operation(CE_HERE, "Loaded hwloc library requires the 2.5+ (2.x) API");
            hwloc_topology_t raw = nullptr;
            if (hwloc_topology_init(&raw) < 0)
                fail("topology initialization");
            NativeTopology topology(raw, hwloc_topology_destroy);
            // OS discovery retains reported topology while disabling discovery
            // components that would temporarily change process/thread binding.
            if (hwloc_topology_set_flags(raw, HWLOC_TOPOLOGY_FLAG_DONT_CHANGE_BINDING) < 0 ||
                hwloc_topology_set_cache_types_filter(raw, HWLOC_TYPE_FILTER_KEEP_ALL) < 0 ||
                hwloc_topology_set_icache_types_filter(raw, HWLOC_TYPE_FILTER_KEEP_ALL) < 0 ||
                hwloc_topology_set_io_types_filter(raw, HWLOC_TYPE_FILTER_KEEP_NONE) < 0)
                fail("topology configuration");
            if (hwloc_topology_load(raw) < 0)
                fail("topology discovery");
            return topology;
        }

        WorkerCacheType cache_type(const hwloc_obj_cache_type_t type) {
            switch (type) {
                case HWLOC_OBJ_CACHE_DATA:
                    return WorkerCacheType::Data;
                case HWLOC_OBJ_CACHE_INSTRUCTION:
                    return WorkerCacheType::Instruction;
                case HWLOC_OBJ_CACHE_UNIFIED:
                    return WorkerCacheType::Unified;
            }
            throw Exceptions::failed_operation(CE_HERE, "hwloc reported an unknown CPU cache type");
        }

        std::vector<unsigned int> members(const hwloc_obj_t object, const std::vector<unsigned int>& eligible) {
            if (!object->cpuset)
                return {};
            const auto cpus = cpu_ids(object->cpuset);
            std::vector<unsigned int> result;
            std::set_intersection(cpus.begin(), cpus.end(), eligible.begin(), eligible.end(), std::back_inserter(result));
            return result;
        }

        class HwlocWorkerTopology final : public WorkerTopologyProvider {
            NativeTopology native_;
            WorkerTopology snapshot_;

        public:
            HwlocWorkerTopology()
            : native_(discover()) {
                const auto native = native_.get();
                if (!hwloc_topology_is_thissystem(native)) {
                    snapshot_.limitations = "hwloc topology does not describe the executing machine; native binding is unavailable";
                    return;
                }
                const auto binding = hwloc_topology_get_support(native)->cpubind;
                if (!binding->get_thisthread_cpubind) {
                    snapshot_.limitations = "hwloc cannot query inherited thread eligibility; native binding is unavailable";
                    return;
                }
                std::vector<unsigned int> inherited;
                try {
                    inherited = current_thread_cpus();
                } catch (const Exceptions::failed_operation& error) {
                    snapshot_.limitations = error.what();
                    return;
                }
                const auto allowed = cpu_ids(hwloc_topology_get_allowed_cpuset(native));
                std::set_intersection(
                    allowed.begin(), allowed.end(), inherited.begin(), inherited.end(), std::back_inserter(snapshot_.eligible_cpus)
                );
                snapshot_.cpu_affinity = binding->set_thisthread_cpubind && !snapshot_.eligible_cpus.empty();
                if (!snapshot_.cpu_affinity)
                    snapshot_.limitations = "hwloc thread-specific binding is unavailable or eligible CPUs are unknown";

                for (int depth = 0; depth < hwloc_topology_get_depth(native); ++depth) {
                    hwloc_obj_t object = nullptr;
                    while ((object = hwloc_get_next_obj_by_depth(native, depth, object))) {
                        if (object->type != HWLOC_OBJ_CORE && !hwloc_obj_type_is_cache(object->type))
                            continue;
                        auto cpus = members(object, snapshot_.eligible_cpus);
                        if (cpus.empty())
                            continue;
                        if (object->type == HWLOC_OBJ_CORE) {
                            snapshot_.cores.push_back(WorkerCore{object->logical_index, std::move(cpus)});
                        } else if (object->attr) {
                            const auto& reported = object->attr->cache;
                            WorkerCacheDomain cache;
                            cache.id = static_cast<unsigned int>(snapshot_.caches.size());
                            cache.level = reported.depth;
                            cache.type = cache_type(reported.type);
                            if (reported.size > 0)
                                cache.capacity_bytes = reported.size;
                            if (reported.linesize > 0)
                                cache.line_bytes = static_cast<unsigned int>(reported.linesize);
                            cache.cpus = std::move(cpus);
                            snapshot_.caches.push_back(std::move(cache));
                        }
                    }
                }
                if (snapshot_.cores.empty())
                    snapshot_.limitations += "; hwloc did not report eligible physical cores";
                if (snapshot_.caches.empty())
                    snapshot_.limitations += "; hwloc did not report eligible CPU caches";
            }

            WorkerTopology topology() const override { return snapshot_; }

            std::vector<unsigned int> current_thread_cpus() const override {
                const auto native = native_.get();
                if (!hwloc_topology_is_thissystem(native) || !hwloc_topology_get_support(native)->cpubind->get_thisthread_cpubind)
                    throw Exceptions::failed_operation(CE_HERE, "hwloc cannot query this thread's native CPU mask");
                auto mask = bitmap();
                if (hwloc_get_cpubind(native, mask.get(), HWLOC_CPUBIND_THREAD) < 0)
                    fail("thread affinity query");
                auto result = cpu_ids(mask.get());
                if (result.empty())
                    throw Exceptions::failed_operation(CE_HERE, "hwloc reported an empty native thread CPU mask");
                return result;
            }

            void bind_current_thread(const std::vector<unsigned int>& cpus) const override {
                if (!snapshot_.cpu_affinity)
                    throw Exceptions::failed_operation(CE_HERE, "hwloc thread binding is unavailable");
                if (cpus.empty())
                    throw Exceptions::invalid_args(CE_HERE, "hwloc thread binding requires an eligible CPU");
                auto mask = bitmap();
                for (const auto cpu : cpus) {
                    if (!std::binary_search(snapshot_.eligible_cpus.begin(), snapshot_.eligible_cpus.end(), cpu))
                        throw Exceptions::invalid_args(CE_HERE, "hwloc binding cannot expand inherited CPU eligibility");
                    if (hwloc_bitmap_set(mask.get(), cpu) < 0)
                        throw std::bad_alloc();
                }
                if (hwloc_set_cpubind(native_.get(), mask.get(), HWLOC_CPUBIND_THREAD | HWLOC_CPUBIND_STRICT | HWLOC_CPUBIND_NOMEMBIND) < 0)
                    fail("thread affinity binding without memory placement");
            }
        };
    }

    std::shared_ptr<WorkerTopologyProvider> make_hwloc_worker_topology() {
        return std::make_shared<HwlocWorkerTopology>();
    }
}
