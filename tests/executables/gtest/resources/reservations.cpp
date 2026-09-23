#include <gtest/gtest.h>
#include <core/resources/objects/object-reservation.hpp>
#include <core/resources/memory/allocators/default-allocator.hpp>
#include <core/resources/memory/allocators/object-allocator.hpp>
#include <core/resources/objects/factory.hpp>

#include <stdexcept>

namespace {
    // Separate item types keep lifetime counters and pool bookkeeping isolated
    // between the reservation, teardown, and failed-construction scenarios.
    struct ReservationItem {
        inline static int live = 0;
        inline static int destroyed = 0;
        int value;
        explicit ReservationItem(int v) : value(v) {
            if (v == -1) throw std::runtime_error("construction failed");
            ++live;
        }
        ~ReservationItem() { --live; ++destroyed; }
    };

    struct LateItem {
        inline static int destroyed = 0;
        int value;
        explicit LateItem(int v) : value(v) {}
        ~LateItem() { ++destroyed; }
    };

    struct FailingBatchItem {
        inline static int attempted = 0;
        inline static int live = 0;
        explicit FailingBatchItem() {
            if (++attempted == 2) throw std::runtime_error("batch construction failed");
            ++live;
        }
        ~FailingBatchItem() { --live; }
    };

    // Observe whole-allocation calls without changing allocator behavior.
    template<typename T>
    struct CountingAllocator {
        using value_type = T;
        inline static int allocated = 0;
        inline static int deallocated = 0;
        T* allocate(std::size_t n) {
            ++allocated;
            return std::allocator<T>{}.allocate(n);
        }
        void deallocate(T* p, std::size_t n) {
            ++deallocated;
            std::allocator<T>{}.deallocate(p, n);
        }
    };

    // Only an allocator with a manager-backed subrange release context may
    // instantiate ObjectReservation; ordinary allocators lack that contract.
    static_assert(CE::Obj::ReservationAllocator<CE::Mem::ObjectPoolAllocator<ReservationItem>, ReservationItem>);
    static_assert(!CE::Obj::ReservationAllocator<CE::Mem::DefaultAllocator<ReservationItem>, ReservationItem>);
    static_assert(!CE::Obj::ReservationAllocator<CE::Mem::ObjectAllocator<ReservationItem>, ReservationItem>);
    static_assert(!CE::Obj::ReservationAllocator<std::allocator<ReservationItem>, ReservationItem>);
}

TEST(memory, reservation_tracks_only_unclaimed_ranges) {
    using CE::Obj::ObjectReservation;
    ReservationItem::live = 0;
    ReservationItem::destroyed = 0;
    std::shared_ptr<ReservationItem> first, second, third;
    ReservationItem* base = nullptr;
    {
        ObjectReservation<ReservationItem> reserved(8);
        ASSERT_EQ(reserved.remaining_ranges().size(), 1);
        base = reserved.remaining_ranges()[0].head.get();
        ASSERT_EQ(reserved.remaining_ranges()[0].length, 8);
        // Claim two adjacent interior slots and a separate interior slot.
        // A failed constructor and a duplicate claim must leave ranges intact.
        first = reserved.emplace(3, 30);
        second = reserved.emplace(4, 40);
        third = reserved.emplace(1, 10);
        ASSERT_EQ(ReservationItem::live, 3);
        EXPECT_EQ(first.get(), base + 3);
        EXPECT_EQ(second.get(), base + 4);
        EXPECT_EQ(third.get(), base + 1);
        EXPECT_THROW(reserved.emplace(6, -1), std::runtime_error);
        EXPECT_THROW(reserved.emplace(3, 99), CE::Exceptions::bad_request);
        EXPECT_EQ(ReservationItem::live, 3);

        // Slots 0, 2, and 5..7 remain available as three contiguous ranges.
        const auto& ranges = reserved.remaining_ranges();
        ASSERT_EQ(ranges.size(), 3);
        EXPECT_EQ(ranges[0].head.get(), base);
        EXPECT_EQ(ranges[0].length, 1);
        EXPECT_EQ(ranges[1].head.get(), base + 2);
        EXPECT_EQ(ranges[1].length, 1);
        EXPECT_EQ(ranges[2].head.get(), base + 5);
        EXPECT_EQ(ranges[2].length, 3);
    }
    // Destruction of the reservation returns only unused slots; each handle
    // still owns a constructed object and releases its slot independently.
    EXPECT_EQ(ReservationItem::live, 3);
    EXPECT_EQ(first->value, 30);
    EXPECT_EQ(second->value, 40);
    EXPECT_EQ(third->value, 10);
    third.reset();
    first.reset();
    second.reset();
    EXPECT_EQ(ReservationItem::live, 0);
    EXPECT_EQ(ReservationItem::destroyed, 3);

    BlockManagement<ReservationItem> bm;
    auto& sections = std::get<1>(bm.sections);
    for (const auto& section : sections) {
        EXPECT_NE(section.owner.get(), base);
    }
}

TEST(memory, handles_retain_pool_state_after_facade_destruction) {
    LateItem::destroyed = 0;
    std::weak_ptr<CE::Obj::PoolState<LateItem>> weak;
    std::shared_ptr<LateItem> object;
    {
        // A local facade gives us a teardown boundary we can control. The
        // handle must retain the exact state it will use for its late release.
        auto facade = std::make_unique<CE::Obj::Pool<LateItem>>();
        auto context = facade->release_context();
        weak = context;
        auto objects = facade->retrieve_objects(1, 99);
        ASSERT_EQ(objects.size(), 1);
        object = std::move(objects[0]);
        facade.reset();
        context.reset();
    }
    ASSERT_FALSE(weak.expired());
    EXPECT_EQ(object->value, 99);
    // The state becomes unowned only after the last object handle is dropped.
    object.reset();
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(LateItem::destroyed, 1);
}

TEST(memory, pool_releases_unconstructed_tail_after_constructor_failure) {
    FailingBatchItem::attempted = 0;
    FailingBatchItem::live = 0;
    auto context = std::make_shared<CE::Obj::PoolState<FailingBatchItem>>();
    // The second constructor fails: the first handle and the unconstructed
    // tail must both return their slots before another batch can be retrieved.
    EXPECT_THROW(context->retrieve_objects(3), std::runtime_error);
    EXPECT_EQ(FailingBatchItem::live, 0);
    auto objects = context->retrieve_objects(3);
    EXPECT_EQ(objects.size(), 3);
    EXPECT_EQ(FailingBatchItem::live, 3);
    objects.clear();
    EXPECT_EQ(FailingBatchItem::live, 0);
}

TEST(memory, factory_deallocates_one_batch_after_its_last_element) {
    ReservationItem::live = 0;
    ReservationItem::destroyed = 0;
    using Allocator = CountingAllocator<ReservationItem>;
    Allocator::allocated = 0;
    Allocator::deallocated = 0;
    auto objects = CE::Obj::Factory<ReservationItem, Allocator>::create(3, 7);
    ASSERT_EQ(objects.size(), 3);
    EXPECT_EQ(Allocator::allocated, 1);
    EXPECT_EQ(ReservationItem::live, 3);
    // Keeping one interior handle alive must also keep the original three-slot
    // allocation alive, even when the returned vector is cleared.
    auto retained = objects[1];
    objects.clear();
    EXPECT_EQ(ReservationItem::live, 1);
    EXPECT_EQ(retained->value, 7);
    EXPECT_EQ(Allocator::deallocated, 0);
    retained.reset();
    EXPECT_EQ(ReservationItem::live, 0);
    EXPECT_EQ(ReservationItem::destroyed, 3);
    EXPECT_EQ(Allocator::deallocated, 1);
}

TEST(memory, factory_releases_batch_after_constructor_failure) {
    using Allocator = CountingAllocator<FailingBatchItem>;
    Allocator::allocated = 0;
    Allocator::deallocated = 0;
    FailingBatchItem::attempted = 0;
    FailingBatchItem::live = 0;
    // Failure halfway through construction destroys completed objects and
    // deallocates the original allocation once.
    EXPECT_THROW((CE::Obj::Factory<FailingBatchItem, Allocator>::create(3)), std::runtime_error);
    EXPECT_EQ(FailingBatchItem::live, 0);
    EXPECT_EQ(Allocator::allocated, 1);
    EXPECT_EQ(Allocator::deallocated, 1);
}
