#include "../tests/hwloc-access.h"

#include <hwloc.h>

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace CE::Workers::Hwloc::Testing {
    namespace {
        using NativeTopology = std::unique_ptr<std::remove_pointer_t<hwloc_topology_t>, decltype(&hwloc_topology_destroy)>;
        using Bitmap = std::unique_ptr<std::remove_pointer_t<hwloc_bitmap_t>, decltype(&hwloc_bitmap_free)>;

        struct ProbeState {
            NativeTopology topology{nullptr, hwloc_topology_destroy};

            ProbeState() {
                hwloc_topology_t raw = nullptr;
                if (hwloc_topology_init(&raw) != 0)
                    throw std::runtime_error("Cannot initialize native test topology");
                topology.reset(raw);
                if (hwloc_topology_set_flags(raw, HWLOC_TOPOLOGY_FLAG_DONT_CHANGE_BINDING) != 0 || hwloc_topology_load(raw) != 0)
                    throw std::runtime_error("Cannot discover native test topology");
            }
        };

        struct PolicyState {
            Bitmap nodes{hwloc_bitmap_alloc(), hwloc_bitmap_free};
            hwloc_membind_policy_t policy{};

            PolicyState() {
                if (!nodes)
                    throw std::bad_alloc{};
            }
        };
    }

    MemoryPolicy::MemoryPolicy(std::shared_ptr<const void> native)
    : native_(std::move(native)) {}

    int MemoryPolicy::policy() const noexcept {
        return static_cast<int>(std::static_pointer_cast<const PolicyState>(native_)->policy);
    }

    bool MemoryPolicy::same_nodes(const MemoryPolicy& other) const noexcept {
        return hwloc_bitmap_isequal(
            std::static_pointer_cast<const PolicyState>(native_)->nodes.get(),
            std::static_pointer_cast<const PolicyState>(other.native_)->nodes.get()
        );
    }

    MemoryProbe::MemoryProbe()
    : native_(std::make_shared<ProbeState>()) {}

    std::optional<MemoryPolicy> MemoryProbe::capture() const {
        const auto state = std::make_shared<PolicyState>();
        constexpr auto flags = HWLOC_MEMBIND_THREAD | HWLOC_MEMBIND_BYNODESET;
        if (hwloc_get_membind(std::static_pointer_cast<ProbeState>(native_)->topology.get(), state->nodes.get(), &state->policy, flags) !=
            0)
            return {};
        return MemoryPolicy{std::move(state)};
    }
}
