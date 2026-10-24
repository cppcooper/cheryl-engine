#include <cheryl/core/resources/allocators.h>

#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    namespace A = CE::Memory::Allocators;
    using CE::Exceptions::bad_request;

    template <class T = void> bool registered(const void* pointer, bool free = false) {
        const auto state = CE::Memory::BlockManagement<T>::shared_state();
        if (free) {
            std::shared_lock lock(std::get<0>(state->pool));
            for (const auto& block : std::get<1>(state->pool))
                if (block.contains(const_cast<void*>(pointer)))
                    return true;
        } else {
            std::shared_lock lock(std::get<0>(state->registry));
            for (const auto& block : std::get<1>(state->registry))
                if (block.contains(const_cast<void*>(pointer)))
                    return true;
        }
        return false;
    }

    struct Activity {
        std::size_t allocations{}, returns{}, constructions{}, destructions{}, requested_bytes{};
        std::size_t node_allocations{}, subobjects{};
        std::vector<const void*> addresses;
    };

    // Measures ACTUAL allocator calls, delegating every operation to the public
    // service. This is test instrumentation, never an alternate storage producer.
    template <class T, template <class> class Service> struct Measured : Service<T> {
        template <class, template <class> class> friend struct Measured;
        std::shared_ptr<Activity> activity = std::make_shared<Activity>();
        using value_type = T;
        template <class U> struct rebind {
            using other = Measured<U, Service>;
        };
        Measured() = default;
        template <class U>
        Measured(const Measured<U, Service>& other)
        : Service<T>(other), activity(other.activity) {}
        Measured select_on_container_copy_construction() const { return *this; }
        T* allocate(std::size_t n) {
            auto* pointer = Service<T>::allocate(n);
            EXPECT_TRUE(registered(pointer)); // Actual byte backing, not equal output alone.
            EXPECT_FALSE(registered(pointer, true));
            if constexpr (std::is_same_v<Service<T>, A::ObjectPoolAllocator<T>>) {
                EXPECT_TRUE(registered<T>(pointer));
                EXPECT_FALSE(registered<T>(pointer, true));
                ++activity->node_allocations;
            }
            ++activity->allocations;
            activity->requested_bytes += n * sizeof(T);
            activity->addresses.push_back(pointer);
            return pointer;
        }
        void deallocate(T* p, std::size_t n) {
            Service<T>::deallocate(p, n);
            if constexpr (std::is_same_v<Service<T>, A::ObjectPoolAllocator<T>>)
                EXPECT_TRUE(registered<T>(p, true));
            else
                EXPECT_TRUE(registered(p, true));
            ++activity->returns;
        }
        template <class U, class... Args> void construct(U* p, Args&&... args) {
            Service<T>::template construct<U>(p, std::forward<Args>(args)...);
            ++activity->constructions;
            if constexpr (!std::is_same_v<T, U>)
                ++activity->subobjects;
        }
        template <class U> void destroy(U* p) {
            Service<T>::template destroy<U>(p);
            ++activity->destructions;
        }
    };

    struct Tracked {
        inline static int live{}, constructed{}, destroyed{}, copies{}, moves{};
        int value;
        explicit Tracked(int v = 0)
        : value(v) {
            if (v == -1)
                throw std::runtime_error("constructor refused");
            ++live;
            ++constructed;
        }
        Tracked(const Tracked& v)
        : Tracked(v.value) {
            ++copies;
        }
        Tracked(Tracked&& v) noexcept
        : Tracked(v.value) {
            ++moves;
        }
        Tracked& operator=(const Tracked&) = default;
        Tracked& operator=(Tracked&&) = default;
        ~Tracked() {
            --live;
            ++destroyed;
        }
        static void reset() {
            ASSERT_EQ(live, 0);
            constructed = destroyed = copies = moves = 0;
        }
    };

    template <template <class> class Service> void bytes() {
        using Alloc = Service<int>;
        using Traits = std::allocator_traits<Alloc>;
        static_assert(std::is_same_v<typename Traits::template rebind_alloc<double>, Service<double>>);
        static_assert(std::is_same_v<decltype(Traits::select_on_container_copy_construction(std::declval<const Alloc&>())), Alloc>);
        static_assert(Traits::is_always_equal::value && Traits::propagate_on_container_swap::value);
        static_assert(Traits::propagate_on_container_move_assignment::value);
        Alloc alloc;
        EXPECT_EQ(Traits::select_on_container_copy_construction(alloc), alloc);
        EXPECT_EQ(Service<double>(alloc), Service<double>{});
        const auto before = Alloc::manager_type::get().diagnostics();
        EXPECT_THROW(static_cast<void>(alloc.allocate(0)), bad_request);
        EXPECT_THROW(static_cast<void>(alloc.allocate(std::numeric_limits<std::size_t>::max() / sizeof(int) + 1)), bad_request);
        EXPECT_THROW(static_cast<void>(alloc.allocate(alloc.max_size() + 1)), bad_request);
        const auto after = Alloc::manager_type::get().diagnostics();
        EXPECT_EQ(before.total_bytes, after.total_bytes);
        EXPECT_EQ(before.available_bytes, after.available_bytes);
        EXPECT_EQ(before.ranges, after.ranges);
        auto* valid = Traits::allocate(alloc, 4);
        Traits::construct(alloc, valid, 71);
        EXPECT_TRUE(registered(valid));
        EXPECT_EQ(*valid, 71);
        Traits::destroy(alloc, valid);
        Traits::deallocate(alloc, valid, 4);
        EXPECT_TRUE(registered(valid, true));

        std::vector<int, Alloc> plain;
        EXPECT_TRUE(plain.empty());
        plain.reserve(12);
        EXPECT_TRUE(registered(plain.data())); // Unwrapped service proves actual std rebind.
        for (int i = 0; i < 300; ++i)
            plain.push_back(i);
        for (int i = 0; i < 300; ++i)
            EXPECT_EQ(plain[static_cast<std::size_t>(i)], i);
        auto copied = plain;
        EXPECT_NE(copied.data(), plain.data());
        EXPECT_TRUE(registered(copied.data()));
        auto moved = std::move(copied);
        EXPECT_EQ(moved, plain);
        std::vector<int, Alloc> assigned;
        assigned = plain;
        assigned.swap(moved);
        EXPECT_EQ(assigned, plain);
        plain.clear();
        auto* backing = plain.data();
        const auto capacity = plain.capacity();
        for (int i = 0; i < 300; ++i)
            plain.push_back(1000 + i);
        EXPECT_EQ(plain.data(), backing);
        EXPECT_EQ(plain.capacity(), capacity);

        using Text = std::basic_string<char, std::char_traits<char>, Service<char>>;
        Text text(300, 'a'); // Exceeds small inline storage.
        EXPECT_TRUE(registered(text.data()));
        text.append(100, 'b');
        EXPECT_TRUE(registered(text.data()));
        EXPECT_EQ(text.size(), 400);
        auto text_copy = text;
        EXPECT_TRUE(registered(text_copy.data()));
        EXPECT_NE(text_copy.data(), text.data());
        text.clear();
        text.assign(100, 'c');
        EXPECT_TRUE(registered(text.data()));

        using Pair = std::pair<const int, Text>;
        std::map<int, Text, std::less<int>, Service<Pair>> tree;
        for (int i = 0; i < 8; ++i)
            tree.emplace(i, Text(80, static_cast<char>('a' + i)));
        for (const auto& [key, value] : tree) {
            EXPECT_TRUE(registered(&key));
            EXPECT_TRUE(registered(value.data())); // Nested allocation is EXPLICIT.
            EXPECT_EQ(value.size(), 80);
        }
        auto clone = tree;
        for (const auto& entry : clone)
            EXPECT_TRUE(registered(&entry));
        clone.erase(2);
        EXPECT_TRUE(tree.contains(2));
        auto transferred = std::move(clone);
        decltype(tree) assigned_tree;
        assigned_tree = tree;
        assigned_tree = std::move(transferred);
        assigned_tree.swap(tree);
        EXPECT_FALSE(tree.contains(2));
        assigned_tree.clear();
        EXPECT_TRUE(assigned_tree.empty());
        assigned_tree.emplace(12, Text(100, 'z'));
        EXPECT_TRUE(registered(&*assigned_tree.begin()));
    }

    template <template <class> class Service> void lifetime() {
        Tracked::reset();
        using M = Measured<Tracked, Service>;
        M measured;
        auto activity = measured.activity;
        {
            std::vector<Tracked, M> values(measured);
            values.reserve(2);
            values.emplace_back(5);
            values.emplace_back(6);
            EXPECT_THROW(values.emplace_back(-1), std::runtime_error); // New buffer cleanup on growth failure.
            ASSERT_EQ(values.size(), 2);
            EXPECT_EQ(values[0].value, 5);
            EXPECT_EQ(Tracked::live, 2);
            for (int i = 0; i < 20; ++i)
                values.emplace_back(i);
            auto copy = values;
            auto moved = std::move(copy);
            moved.clear();
            EXPECT_EQ(Tracked::live, 22);
            values.clear();
            EXPECT_EQ(Tracked::live, 0);
            auto* pointer = values.data();
            const auto calls = activity->allocations;
            for (int i = 0; i < 22; ++i)
                values.emplace_back(i);
            EXPECT_EQ(activity->allocations, calls);
            EXPECT_EQ(values.data(), pointer);
        }
        EXPECT_EQ(Tracked::live, 0);
        EXPECT_EQ(Tracked::constructed, Tracked::destroyed);
        EXPECT_EQ(activity->allocations, activity->returns);
        EXPECT_EQ(activity->constructions, activity->destructions);
        EXPECT_GT(activity->allocations, 1);
        std::cout << "actual allocator calls=" << activity->allocations << " returns=" << activity->returns
                  << " successful constructions=" << activity->constructions << " destructions=" << activity->destructions
                  << " requested_bytes=" << activity->requested_bytes << " value copies=" << Tracked::copies << " moves=" << Tracked::moves
                  << '\n';
    }
}

TEST(allocator_containers, default_bytes) {
    bytes<A::DefaultAllocator>();
}
TEST(allocator_containers, object_bytes) {
    bytes<A::ObjectAllocator>();
}
TEST(allocator_containers, default_lifetimes) {
    lifetime<A::DefaultAllocator>();
}
TEST(allocator_containers, object_lifetimes) {
    lifetime<A::ObjectAllocator>();
}
TEST(allocator_containers, pool_lifetimes) {
    lifetime<A::ObjectPoolAllocator>();
}

TEST(allocator_containers, pool_nodes) {
    Tracked::reset();
    using Pair = std::pair<const int, Tracked>;
    using Service = A::ObjectPoolAllocator<Pair>;
    using M = Measured<Pair, A::ObjectPoolAllocator>;
    static_assert(std::is_same_v<typename std::allocator_traits<Service>::template rebind_alloc<Tracked>, A::ObjectPoolAllocator<Tracked>>);
    M measured;
    const auto counts = measured.activity;
    {
        std::map<int, Tracked, std::less<int>, M> values(std::less<int>{}, measured);
        for (int i = 0; i < 6; ++i)
            values.try_emplace(i, i + 10);
        EXPECT_THROW(values.try_emplace(9, -1), std::runtime_error);
        EXPECT_EQ(values.size(), 6);
        EXPECT_EQ(Tracked::live, 6);
        auto copy = values;
        copy.erase(2);
        auto moved = std::move(copy);
        decltype(values) assigned(std::less<int>{}, measured);
        assigned = values;
        assigned = std::move(moved);
        values.swap(assigned);
        EXPECT_FALSE(values.contains(2));
        EXPECT_TRUE(assigned.contains(2));
        assigned.clear();
        assigned.try_emplace(15, 15);
        EXPECT_EQ(assigned.at(15).value, 15);
    }
    EXPECT_EQ(Tracked::live, 0);
    EXPECT_EQ(Tracked::constructed, Tracked::destroyed);
    EXPECT_EQ(counts->allocations, counts->returns);
    EXPECT_EQ(counts->constructions, counts->destructions);
    EXPECT_GT(counts->node_allocations, 0);
    EXPECT_GT(counts->subobjects, 0); // Pair value constructed through rebound CLASS node allocator.
    // Also exercise an UNWRAPPED map, not only instrumentation's rebind.
    {
        std::map<int, Tracked, std::less<int>, Service> plain;
        plain.try_emplace(1, 19);
        EXPECT_TRUE(registered(&*plain.begin()));
    }
    EXPECT_EQ(Tracked::live, 0);
    std::cout << "actual pool node calls=" << counts->node_allocations << " value subobjects=" << counts->subobjects << '\n';
}

TEST(allocator_containers, alignment_and_domains) {
    struct alignas(256) Wide {
        int value;
    };
    auto exercise = []<class Alloc>(Alloc alloc) {
        const auto before = CE::Memory::Manager<>::get().diagnostics();
        EXPECT_THROW(static_cast<void>(alloc.allocate(0)), bad_request);
        EXPECT_THROW(static_cast<void>(alloc.allocate(alloc.max_size() + 1)), bad_request);
        const auto after = CE::Memory::Manager<>::get().diagnostics();
        EXPECT_EQ(after.total_bytes, before.total_bytes);
        EXPECT_EQ(after.available_bytes, before.available_bytes);
        auto* pointer = alloc.allocate(3);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(pointer) % 256, 0);
        alloc.construct(pointer, Wide{11});
        EXPECT_EQ(pointer->value, 11);
        alloc.destroy(pointer);
        alloc.deallocate(pointer, 3);
    };
    exercise(A::DefaultAllocator<Wide>{});
    exercise(A::ObjectAllocator<Wide>{});
    exercise(A::ObjectPoolAllocator<Wide>{});
    A::DefaultAllocator<char> bytes;
    A::ObjectAllocator<double> objects;
    EXPECT_EQ(decltype(bytes)::manager_type::get().diagnostics().domain, decltype(objects)::manager_type::get().diagnostics().domain);
}

TEST(allocator_containers, pool_retained_context) {
    struct Item {
        int value;
        explicit Item(int v)
        : value(v) {}
    };
    using Pool = CE::Memory::Objects::Pool<Item>;
    using Alloc = A::ObjectPoolAllocator<Item>;
    auto facade = std::make_unique<Pool>();
    Alloc old(*facade);
    auto* pointer = old.allocate(4);
    old.construct(pointer, 17);
    facade.reset();
    Alloc current;
    EXPECT_NE(current.context(), old.context());
    EXPECT_EQ(current, old); // Same T block/tracker domain despite different context handles.
    EXPECT_EQ(std::allocator_traits<Alloc>::select_on_container_copy_construction(old).context(), old.context());
    auto copied = old;
    auto moved = std::move(copied);
    EXPECT_EQ(copied.context(), old.context()); // Moved-from allocator stays usable.
    moved.destroy(pointer);
    current.deallocate(pointer, 4); // Interchangeability is tested, not inferred from handles.
    auto* later = old.allocate(2);
    old.construct(later, 23);
    EXPECT_EQ(later->value, 23);
    old.destroy(later);
    old.deallocate(later, 2);
    // Cross-type comparison identifies the shared service family, not typed-pointer interchangeability.
    struct Other {
        int value;
    };
    A::ObjectPoolAllocator<Other> rebound(old);
    EXPECT_TRUE(rebound == old);
    EXPECT_TRUE(old == rebound);
    Alloc roundtrip(rebound);
    EXPECT_EQ(roundtrip, old);
}

namespace {
    template <class Left, class Right> void equality_laws(const Left& left, const Right& right) {
        static_assert(std::is_nothrow_constructible_v<Left, const Right&>);
        static_assert(std::is_nothrow_constructible_v<Right, const Left&>);
        const Left round_left(right);
        const Right round_right(left);
        EXPECT_EQ(left == right, right == left);
        EXPECT_EQ(left == right, left == round_left);
        EXPECT_EQ(right == left, right == round_right);
        EXPECT_EQ(left == round_left, round_left == left);
        EXPECT_TRUE(left == round_right);
        EXPECT_TRUE(right == round_left);
    }
}

TEST(allocator_containers, rebind_laws) {
    struct First {
        int value;
    };
    struct Second {
        double value;
    };
    struct Third {
        int value;
    };
    A::ObjectPoolAllocator<First> first;
    ASSERT_EQ(CE::Memory::Objects::Pool<Second>::get_existing(), nullptr);
    A::ObjectPoolAllocator<Second> second(first);
    EXPECT_EQ(CE::Memory::Objects::Pool<Second>::get_existing(), nullptr); // First conversion did NOT acquire destination.
    equality_laws(first, second);
    A::ObjectPoolAllocator<Third> third(second);
    equality_laws(first, third);
    EXPECT_EQ(CE::Memory::Objects::Pool<Second>::get_existing(), nullptr);
    EXPECT_EQ(CE::Memory::Objects::Pool<Third>::get_existing(), nullptr);
    auto* p = second.allocate(2); // First type setup is admitted here, where allocation failure is allowed.
    second.construct(p, Second{3.5});
    EXPECT_EQ(p->value, 3.5);
    A::ObjectPoolAllocator<Second> other(third);
    other.destroy(p);
    other.deallocate(p, 2);
    equality_laws(A::DefaultAllocator<int>{}, A::DefaultAllocator<double>{});
    equality_laws(A::ObjectAllocator<int>{}, A::ObjectAllocator<double>{});
    static_assert(noexcept(first.deallocate(nullptr, 1)));
    static_assert(noexcept(A::DefaultAllocator<int>{}.deallocate(nullptr, 1)));
    static_assert(noexcept(A::ObjectAllocator<int>{}.deallocate(nullptr, 1)));
}

TEST(allocator_containers, extracted_node_owner) {
    Tracked::reset();
    using Pair = std::pair<const int, Tracked>;
    using M = Measured<Pair, A::ObjectPoolAllocator>;
    using Map = std::map<int, Tracked, std::less<int>, M>;
    M alloc;
    Map::node_type detached;
    Tracked* stable = nullptr;
    const auto counters = alloc.activity;
    {
        Map source(std::less<int>{}, alloc);
        source.try_emplace(1, 101);
        source.try_emplace(2, 202);
        stable = &source.at(2);
        detached = source.extract(2);
        EXPECT_EQ(&detached.mapped(), stable);
    }
    ASSERT_TRUE(detached);
    EXPECT_EQ(Tracked::live, 1);
    EXPECT_EQ(detached.mapped().value, 202);
    EXPECT_EQ(counters->allocations - counters->returns, 1u);
    {
        Map destination(std::less<int>{}, detached.get_allocator());
        const auto inserted = destination.insert(std::move(detached));
        ASSERT_TRUE(inserted.inserted);
        EXPECT_EQ(&inserted.position->second, stable);
        destination.try_emplace(3, 303);
        auto copy = destination;
        auto moved = std::move(copy);
        destination.swap(moved);
        EXPECT_EQ(destination.at(2).value, 202);
        detached = destination.extract(2);
    }
    EXPECT_EQ(Tracked::live, 1);
    EXPECT_EQ(detached.mapped().value, 202);
    detached = {}; // Last extracted node returns its actual rebound class slot.
    EXPECT_EQ(Tracked::live, 0);
    EXPECT_EQ(counters->allocations, counters->returns);
    EXPECT_EQ(counters->constructions, counters->destructions);
    EXPECT_EQ(Tracked::constructed, Tracked::destroyed);
}

TEST(allocator_containers, list_node_owner) {
    Tracked::reset();
    using M = Measured<Tracked, A::ObjectPoolAllocator>;
    M alloc;
    const auto counts = alloc.activity;
    {
        std::list<Tracked, M> values(alloc);
        for (int i = 0; i < 8; ++i)
            values.emplace_back(i);
        auto survivor = std::next(values.begin(), 3);
        const auto* stable = &*survivor;
        for (auto it = values.begin(); it != values.end();)
            if (it->value % 2 == 0)
                it = values.erase(it);
            else
                ++it;
        EXPECT_EQ(&*survivor, stable);
        EXPECT_EQ(stable->value, 3);
        for (int i : {8, 10, 12, 14})
            values.emplace_back(i);
        const std::array expected{1, 3, 5, 7, 8, 10, 12, 14};
        std::size_t at = 0;
        for (const auto& value : values) {
            ASSERT_LT(at, expected.size());
            EXPECT_EQ(value.value, expected[at++]);
        }
        EXPECT_EQ(at, expected.size());
        EXPECT_EQ(&*survivor, stable);
        std::list<Tracked, M> other(alloc);
        other.emplace_back(99);
        values.swap(other);
        EXPECT_EQ(&*survivor, stable);
        EXPECT_EQ(values.front().value, 99);
        auto copy = other;
        auto moved = std::move(copy);
        values = std::move(moved);
        EXPECT_EQ(values.size(), expected.size());
        auto it = values.begin();
        for (int want : expected) {
            ASSERT_NE(it, values.end());
            EXPECT_EQ(it->value, want);
            ++it;
        }
        other.clear();
        values.clear();
        EXPECT_EQ(Tracked::live, 0);
    }
    EXPECT_EQ(counts->allocations, counts->returns);
    EXPECT_EQ(counts->constructions, counts->destructions);
    EXPECT_GT(counts->subobjects, 0u); // std::list constructs Record inside actual rebound node.
    EXPECT_EQ(Tracked::constructed, Tracked::destroyed);
}

namespace {
    struct alignas(256) AlignedItem {
        inline static int live = 0, made = 0, destroyed = 0;
        const int id;
        explicit AlignedItem(int v)
        : id(v) {
            if (v < 0)
                throw 17;
            ++live;
            ++made;
        }
        AlignedItem(const AlignedItem& other)
        : AlignedItem(other.id) {}
        ~AlignedItem() {
            --live;
            ++destroyed;
        }
    };
}
TEST(allocator_containers, aligned_throwing_nodes) {
    AlignedItem::live = AlignedItem::made = AlignedItem::destroyed = 0;
    {
        using ListAlloc = A::ObjectPoolAllocator<AlignedItem>;
        std::list<AlignedItem, ListAlloc> list;
        list.emplace_back(7);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&list.front()) % 256, 0u);
        EXPECT_THROW(list.emplace_back(-1), int);
        EXPECT_EQ(list.size(), 1u);
        list.emplace_back(8);
        EXPECT_EQ(list.back().id, 8);
        using Pair = std::pair<const int, AlignedItem>;
        std::map<int, AlignedItem, std::less<int>, A::ObjectPoolAllocator<Pair>> map;
        map.try_emplace(3, 13);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&map.at(3)) % 256, 0u);
        EXPECT_THROW(map.try_emplace(4, -1), int);
        EXPECT_EQ(map.size(), 1u);
        map.try_emplace(4, 14);
        EXPECT_EQ(map.at(4).id, 14);
    }
    EXPECT_EQ(AlignedItem::live, 0);
    EXPECT_EQ(AlignedItem::made, AlignedItem::destroyed);
}
