#pragma once

// Included after the private Record definition, before any rebound node use.
// Only this executable's anonymous node types opt in. No shared void domain,
// State layout or normal timing instantiation changes its resource or scope.
namespace PoolTraceProfile {
    using Step = CE::Memory::Detail::PoolStep;
    constexpr auto steps = static_cast<std::size_t>(Step::Count);
    struct Totals {
        std::array<std::uint64_t, steps> ns{}, calls{};
        std::size_t requests{}, requested_bytes{}, live_bytes{}, peak_bytes{};
    };
    template <class Tag> Totals& totals() {
        static Totals value;
        return value;
    }
    template <class Tag> struct Resource final : std::pmr::memory_resource {
        void* do_allocate(std::size_t count, std::size_t alignment) override {
            auto& t = totals<Tag>();
            ++t.requests;
            t.requested_bytes += count;
            auto* p = std::pmr::new_delete_resource()->allocate(count, alignment);
            t.live_bytes += count;
            t.peak_bytes = std::max(t.peak_bytes, t.live_bytes);
            return p;
        }
        void do_deallocate(void* p, std::size_t count, std::size_t alignment) override {
            totals<Tag>().live_bytes -= count;
            std::pmr::new_delete_resource()->deallocate(p, count, alignment);
        }
        bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }
    };
    template <class Tag> struct Probe {
        template <Step step> struct Scope {
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            ~Scope() {
                auto& t = totals<Tag>();
                const auto index = static_cast<std::size_t>(step);
                t.ns[index] += static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()
                );
                ++t.calls[index];
            }
        };
        static std::pmr::memory_resource* metadata() {
            // Process-lifetime diagnostic resource: the production State borrows
            // it and may outlive static allocator directories during shutdown.
            // Its own allocation is excluded, never a production resource policy.
            static auto* const resource = new Resource<Tag>;
            return resource;
        }
    };
    struct ListTag {};
    struct MapTag {};
    template <class Tag> void reset() {
        const auto live = totals<Tag>().live_bytes;
        totals<Tag>() = {};
        totals<Tag>().live_bytes = totals<Tag>().peak_bytes = live;
    }
    void begin(std::string_view kind) {
        if (kind == "list")
            reset<ListTag>();
        else
            reset<MapTag>();
    }
    template <class Tag> void emit(std::string_view temperature) {
        constexpr std::array names{"allocation",   "context",         "checkout", "locks",        "reservation", "backing",
                                   "deallocation", "release_context", "return",   "construction", "destruction"};
        const auto& t = totals<Tag>();
        std::cout << "{\"kind\":\"profile\",\"temperature\":\"" << temperature << "\",\"steps\":[";
        for (std::size_t i = 0; i < steps; ++i) {
            if (i)
                std::cout << ',';
            std::cout << "{\"step\":\"" << names[i] << "\",\"calls\":" << t.calls[i] << ",\"ns\":" << t.ns[i] << '}';
        }
        std::cout << "],\"metadata_requests\":" << t.requests << ",\"metadata_requested_bytes\":" << t.requested_bytes
                  << ",\"metadata_live_bytes\":" << t.live_bytes << ",\"metadata_peak_bytes\":" << t.peak_bytes << "}\n";
    }
    void report(std::string_view kind, std::string_view temperature) {
        if (kind == "list")
            emit<ListTag>(temperature);
        else
            emit<MapTag>(temperature);
    }
}
namespace CE::Memory::Detail {
    template <> struct PoolProbe<std::_List_node<Record>> : PoolTraceProfile::Probe<PoolTraceProfile::ListTag> {};
    template <> struct PoolProbe<std::_Rb_tree_node<Pair>> : PoolTraceProfile::Probe<PoolTraceProfile::MapTag> {};
}
