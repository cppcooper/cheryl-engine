#include <core/resources/memory/allocators/default-allocator.hpp>
#include <core/resources/objects/object-reservation.hpp>
#include <core/logging/process-lifetime.h>
#include "regional-checks.h"
#include "runtime-admission.h"
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>

namespace {
    template <class T> using Alloc = CE::Memory::Allocators::DefaultAllocator<T>;
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }
    bool engine_owns(void* pointer) {
        CE::Memory::BlockManagement<void> records;
        std::shared_lock lock(std::get<0>(records.registry));
        for (const auto& owner : std::get<1>(records.registry))
            if (owner.contains(pointer)) return true;
        return false;
    }
    struct alignas(256) Aligned { int value = 9; };
    struct Item {
        inline static int live = 0;
        int value;
        explicit Item(int n) : value(n) {
            if (n < 0) throw std::runtime_error("requested construction failure");
            ++live;
        }
        ~Item() { --live; }
    };
}
int main() {
    try {
        CE::Logging::ProcessLifetime logging;
        using Traits = std::allocator_traits<Alloc<int>>;
        static_assert(std::same_as<Traits::rebind_alloc<double>, Alloc<double>>);
        static_assert(Traits::is_always_equal::value);
        using PoolPair = std::pair<const int, std::string>;
        using PoolAlloc = CE::Memory::Allocators::ObjectPoolAllocator<PoolPair>;
        static_assert(std::same_as<std::allocator_traits<PoolAlloc>::rebind_alloc<Item>, CE::Memory::Allocators::ObjectPoolAllocator<Item>>);
        {
            std::map<int, std::string, std::less<int>, PoolAlloc> nodes;
            nodes.emplace(3, "typed node");
            auto copied = nodes;
            require(copied == nodes && engine_owns(const_cast<PoolPair*>(&*nodes.begin())), "typed rebound node allocation and generic construction");
        }
        auto manager = std::make_shared<Alloc<int>::manager_type>();
        std::weak_ptr<Alloc<int>::manager_type> weak = manager;
        {
            Alloc<int> allocator(manager);
            manager.reset();
            require(!weak.expired(), "allocator must retain checkout authority");
            std::vector<int, Alloc<int>> values(allocator);
            values.reserve(32);
            auto* first = values.data();
            for (int i = 0; i < 32; ++i) values.push_back(i);
            values.clear();
            values.push_back(42);
            require(values.data() == first && values[0] == 42, "vector capacity reuse");
            auto copy = values;
            require(copy == values && copy.data() != values.data(), "copy selection retains Engine allocator");
            auto moved = std::move(values);
            values.push_back(7);
            require(moved[0] == 42 && values[0] == 7, "moved-from allocator remains usable");
            require(engine_owns(moved.data()), "vector must use Engine domain");
            std::basic_string<char, std::char_traits<char>, Alloc<char>> text(300, 'x', Alloc<char>(allocator));
            require(engine_owns(text.data()), "non-inline string must use Engine domain");
            using Pair = std::pair<const int, std::string>;
            std::map<int, std::string, std::less<int>, Alloc<Pair>> map{Alloc<Pair>(allocator)};
            map.emplace(5, "node");
            require(engine_owns(const_cast<Pair*>(&*map.begin())), "rebound map node must use Engine domain");
            Alloc<Aligned> aligned(allocator);
            auto* p = aligned.allocate(1);
            require(reinterpret_cast<std::uintptr_t>(p) % alignof(Aligned) == 0, "over-alignment");
            std::allocator_traits<Alloc<Aligned>>::construct(aligned, p);
            require(p->value == 9, "generic construction");
            std::allocator_traits<Alloc<Aligned>>::destroy(aligned, p);
            aligned.deallocate(p, 1);
            bool overflow = false;
            try { static_cast<void>(aligned.allocate(aligned.max_size() + 1)); }
            catch (const std::bad_array_new_length&) { overflow = true; }
            require(overflow, "count overflow refused before checkout");
            require(allocator.allocate(0) == nullptr, "zero allocation");
            allocator.deallocate(nullptr, 0);
            alignas(double) std::byte storage[sizeof(double)];
            auto* subobject = reinterpret_cast<double*>(storage);
            Traits::construct(allocator, subobject, 1.25);
            require(*subobject == 1.25, "primary traits construct a different subobject type");
            Traits::destroy(allocator, subobject);
            auto selected = Traits::select_on_container_copy_construction(allocator);
            require(selected == allocator, "copy selection preserves domain");
            Alloc<int> other(std::make_shared<Alloc<int>::manager_type>());
            auto* cross = allocator.allocate(5);
            other.deallocate(cross, 5);
            const auto backing_calls = CE::Memory::Detail::backing_allocations.snapshot().calls;
            auto* recycled = allocator.allocate(5);
            require(CE::Memory::Detail::backing_allocations.snapshot().calls == backing_calls, "returned range avoids new backing allocation");
            allocator.deallocate(recycled, 5);
        }
        require(weak.expired(), "facade released after last allocator");
        std::shared_ptr<Item> held;
        {
            CE::Memory::Objects::Pool<Item> facade;
            CE::Memory::Allocators::ObjectPoolAllocator<Item> allocator(facade.release_context());
            CE::Memory::Objects::Reservation<Item> slots(2, allocator);
            bool failed = false;
            try { static_cast<void>(slots.emplace(0, -1)); }
            catch (const std::runtime_error&) { failed = true; }
            require(failed && Item::live == 0, "failed construction preserves reserved slot");
            held = slots.emplace(0, 17);
            bool duplicate = false;
            try { static_cast<void>(slots.emplace(0, 4)); }
            catch (const CE::Exceptions::bad_request&) { duplicate = true; }
            require(duplicate && held->value == 17 && Item::live == 1, "duplicate claim refused");
        }
        require(held->value == 17 && Item::live == 1, "typed owner outlives reservation");
        held.reset();
        require(Item::live == 0, "typed owner destroyed exactly once");
        {
            using PoolAllocator = CE::Memory::Allocators::ObjectPoolAllocator<Item>;
            std::optional<PoolAllocator> retained;
            std::weak_ptr<PoolAllocator::context_type> weak_pool;
            {
                CE::Memory::Objects::Pool<Item> facade;
                auto context = facade.release_context();
                weak_pool = context;
                retained.emplace(context);
            }
            auto* fresh = retained->allocate(5000);
            std::allocator_traits<PoolAllocator>::construct(*retained, fresh, 23);
            require(fresh->value == 23 && Item::live == 1, "typed growth after facade teardown");
            std::allocator_traits<PoolAllocator>::destroy(*retained, fresh);
            retained->deallocate(fresh, 5000);
            require(CE::Memory::ObjectMemoryManager<Item>::get_existing() == nullptr, "typed checkout uses retained facade without singleton revival");
            retained.reset();
            require(weak_pool.expired() && Item::live == 0, "retained typed context final lifetime");
        }
        {
            using namespace ProcGen;
            auto current = ProcGenRegionalChecks::generated();
            const auto pinned = current;
            std::shared_ptr<WeatherWorkspace> workspace;
            {
                CE::Memory::Objects::Pool<WeatherWorkspace> facade;
                CE::Memory::Allocators::ObjectPoolAllocator<WeatherWorkspace> allocator(facade.release_context());
                CE::Memory::Objects::Reservation<WeatherWorkspace> reservation(1, allocator);
                workspace = reservation.emplace(0);
            }
            double* retained_targets = nullptr;
            for (int tick = 0; tick < 3; ++tick) {
                const auto fresh = advance_regional(current, 1, regional_hard_limits).value();
                auto reused = advance_regional(current, 1, regional_hard_limits, *workspace).value();
                static_cast<void>(ProcGenRegionalChecks::inspect(current, reused));
                require(ProcGenRegionalChecks::same(fresh.state, reused.state), "fresh/reused numerical equivalence");
                require(fresh.report.transfers == reused.report.transfers, "fresh/reused transfer equivalence");
                require(workspace->next_water.data() != reused.state.weather().water().data(), "scratch must not alias published water");
                require(workspace->next_flow.data() != reused.state.weather().flow().data(), "scratch must not alias published flow");
                if (tick) require(workspace->targets.data() == retained_targets, "retained scratch allocation");
                retained_targets = workspace->targets.data();
                current = std::move(reused.state);
            }
            const auto before = current;
            auto budget = regional_hard_limits;
            budget.substeps_per_tick = 0;
            auto refused = advance_regional(current, 1, budget, *workspace);
            require(!refused && refused.error().reason == RegionalReason::substeps, "real substep refusal");
            require(ProcGenRegionalChecks::same(before, current), "refusal preserves published state");
            workspace.reset();
            require(current.weather().tick() == pinned.weather().tick() + 3, "published owner survives workspace release");
            {
                auto old = ProcGenRegionalChecks::literal();
                std::optional<RegionalState> escaped;
                {
                    CE::Memory::Objects::Reservation<WeatherWorkspace> reservation(1);
                    auto temporary = reservation.emplace(0);
                    auto out = advance_regional(old, 1, regional_hard_limits, *temporary).value();
                    require(out.state.weather().usage().substeps_per_tick == 1, "odd swap fixture");
                    escaped.emplace(std::move(out.state));
                }
                require(escaped->weather().tick() == old.weather().tick() + 1 && escaped->weather().water()[0].vapour_mm > 0,
                        "odd-swap output owns former scratch after workspace destruction");
            }
            auto small = ProcGenRegionalChecks::literal();
            WeatherWorkspace scratch;
            RuntimeAllowance allowance{std::numeric_limits<Index>::max(), std::numeric_limits<Index>::max()};
            require(allowance.remaining_work() == 6'000'000 && allowance.remaining_updates() == 4,
                    "caller allowance requests clamp to hard batch limits");
            const auto calls = CE::Memory::Detail::byte_checkout_calls.load();
            auto ticks = advance_runtime(small, 2, allowance, scratch);
            auto profile = advance_runtime(current, 1, allowance, scratch); //600s exceeds120s profile
            require(!ticks && ticks.error().reason == RegionalReason::ticks, "runtime tick admission");
            require(!profile && profile.error().reason == RegionalReason::substeps, "runtime model admission");
            require(CE::Memory::Detail::byte_checkout_calls.load() == calls, "early refusal performs no checkout");
            for (int i = 0; i < 4; ++i) {
                auto next = advance_runtime(small, 1, allowance, scratch);
                require(next.has_value(), "bounded runtime accepts positive path");
                small = std::move(next->state);
            }
            auto exhausted = advance_runtime(small, 1, allowance, scratch);
            require(!exhausted && exhausted.error().reason == RegionalReason::work, "total regional batch admission");
        }
        require(CE::Memory::Detail::backing_allocations.snapshot().calls > 0, "real Engine backing allocations");
        require(CE::Memory::Detail::metadata_allocations.snapshot().calls > 0, "real bookkeeping allocations");
        std::cout << "Engine vector/string/map, retained growth, alignment/count and typed lifetime checks pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
