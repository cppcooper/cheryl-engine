#include <cheryl/core/resources/allocators.h>
#include <core/resources/memory/managed-block.hpp>
#include <gtest/gtest.h>
#include <memory_resource>
#include <iostream>

namespace {
    struct RefuseMetadata final : std::pmr::memory_resource {
        bool refuse = false;
        std::size_t requests = 0, denied = 0, bytes = 0, peak = 0, reject_at = 0;
        void* do_allocate(std::size_t n, std::size_t alignment) override {
            ++requests;
            if (refuse || requests == reject_at) {
                ++denied;
                throw std::bad_alloc{};
            }
            void* p = std::pmr::new_delete_resource()->allocate(n, alignment);
            bytes += n;
            peak = std::max(peak, bytes);
            return p;
        }
        void do_deallocate(void* p, std::size_t n, std::size_t alignment) override {
            bytes -= n;
            std::pmr::new_delete_resource()->deallocate(p, n, alignment);
        }
        bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }
    };
    template <class T> struct ActualPool : CE::Memory::Objects::PoolState<T> {
        using State = CE::Memory::BlockManagement<T>::State;
        explicit ActualPool(std::pmr::memory_resource* resource) { this->state_ = std::make_shared<State>(resource); }
        auto state() const { return this->state_; }
    };
    struct Item {
        inline static int live = 0, made = 0, destroyed = 0, attempt = 0;
        inline static RefuseMetadata* reject_on_second = nullptr;
        int value;
        explicit Item(int v)
        : value(v) {
            if (reject_on_second && ++attempt == 2) {
                reject_on_second->refuse = true;
                throw 71;
            }
            ++live;
            ++made;
        }
        ~Item() {
            --live;
            ++destroyed;
        }
    };
}

TEST(allocator_release, typed_ranges_without_allocation) {
    RefuseMetadata metadata;
    {
        auto pool = std::make_shared<ActualPool<Item>>(&metadata);
        auto state = pool->state();
        auto block = pool->retrieve_block(9);
        const auto calls = metadata.requests;
        metadata.refuse = true;
        // Nonadjacent returns create fragmentation, followed by complete merging.
        for (std::size_t i : {4u, 0u, 8u, 2u, 6u, 1u, 7u, 3u, 5u})
            pool->return_objects(block.head.get() + i, 1);
        EXPECT_EQ(metadata.requests, calls);
        EXPECT_EQ(metadata.denied, 0u);
        EXPECT_EQ(std::get<1>(state->pool).size(), 1u);
        EXPECT_EQ(std::get<1>(state->sections).size(), 0u);
        EXPECT_EQ(std::get<1>(state->stale).size(), 1u);
        // Invalid calls remain diagnosed without changing the completed partition.
        EXPECT_THROW(pool->return_objects(block.head.get(), 1), CE::Exceptions::bad_request);
        EXPECT_THROW(pool->return_objects(block.head.get() + 9, 1), CE::Exceptions::bad_request);
        EXPECT_EQ(metadata.requests, calls);
        metadata.refuse = false;
        auto reused = pool->retrieve_block(9);
        EXPECT_EQ(reused.head, block.head);
        const auto after_checkout = metadata.requests;
        metadata.refuse = true;
        pool->return_objects(reused.head.get(), 9);
        EXPECT_EQ(metadata.requests, after_checkout);
        metadata.refuse = false;
        pool->cull(std::chrono::minutes{-1});
        pool->release_culled();
        EXPECT_TRUE(std::get<1>(state->registry).empty());
        std::cout << "actual metadata peak=" << metadata.peak << " bytes; return requests=" << metadata.denied << '\n';
    }
    EXPECT_EQ(metadata.bytes, 0u);
}

TEST(allocator_release, byte_whole_without_allocation) {
    RefuseMetadata metadata;
    using Tx = CE::Memory::BlockTransactions<void>;
    using State = CE::Memory::BlockManagement<void>::State;
    {
        auto state = std::make_shared<State>(&metadata);
        auto first = Tx::checkout(state, 128, std::align_val_t{64}, [] {
            auto& manager = CE::Memory::ExactManager::get();
            auto block = manager.checkout_chunk(512, 64);
            auto owner = CE::Memory::make_managed_block<void>(manager, block);
            return CE::Memory::HeapBlock{owner, owner, block.alignment, block.length};
        });
        auto second = Tx::checkout(state, 128, std::align_val_t{64}, [] {
            ADD_FAILURE();
            return CE::Memory::HeapBlock{};
        });
        auto calls = metadata.requests;
        metadata.refuse = true;
        EXPECT_TRUE(Tx::return_range(state, first.head.get(), std::nullopt));
        EXPECT_TRUE(Tx::return_range(state, second.head.get(), std::nullopt));
        EXPECT_EQ(metadata.requests, calls);
        EXPECT_EQ(metadata.denied, 0u);
        EXPECT_EQ(std::get<1>(state->pool).size(), 1u);
        metadata.refuse = false;
        Tx::cull(state, std::chrono::minutes{-1});
        Tx::release_culled(state);
        EXPECT_TRUE(std::get<1>(state->registry).empty());
    }
    EXPECT_EQ(metadata.bytes, 0u);
}

TEST(allocator_release, constructor_unwind_without_allocation) {
    RefuseMetadata metadata;
    Item::live = Item::made = Item::destroyed = Item::attempt = 0;
    {
        auto pool = std::make_shared<ActualPool<Item>>(&metadata);
        // Admit all storage/return metadata before user construction starts.
        auto reserved = pool->retrieve_block(4);
        pool->return_objects(reserved.head.get(), 4);
        Item::reject_on_second = &metadata;
        EXPECT_THROW(static_cast<void>(pool->retrieve_objects(4, 8)), int);
        Item::reject_on_second = nullptr;
        EXPECT_TRUE(metadata.refuse);
        EXPECT_EQ(metadata.denied, 0u);
        EXPECT_EQ(Item::live, 0);
        EXPECT_EQ(Item::made, Item::destroyed);
        metadata.refuse = false;
        auto later = pool->retrieve_objects(1, 9);
        ASSERT_EQ(later.size(), 1u);
        EXPECT_EQ(later[0]->value, 9);
        auto calls = metadata.requests;
        metadata.refuse = true;
        pool.reset(); // Actual final owner survives the facade.
        later.clear();
        EXPECT_EQ(metadata.requests, calls);
        EXPECT_EQ(Item::live, 0);
        EXPECT_EQ(Item::made, Item::destroyed);
    }
    EXPECT_EQ(metadata.bytes, 0u);
}

TEST(allocator_release, admission_failure_preserves_returns) {
    std::size_t refused = 0;
    for (std::size_t ordinal = 1; ordinal <= 12; ++ordinal) {
        RefuseMetadata metadata;
        {
            auto pool = std::make_shared<ActualPool<Item>>(&metadata);
            auto block = pool->retrieve_block(1);
            auto state = pool->state();
            const auto snapshot = [](const auto& tuple) {
                const auto& set = std::get<1>(tuple);
                return std::vector<typename std::decay_t<decltype(set)>::value_type>(set.begin(), set.end());
            };
            auto owners = snapshot(state->registry), sections = snapshot(state->sections), free = snapshot(state->pool);
            metadata.reject_at = metadata.requests + ordinal;
            CE::Memory::Block<Item> next;
            bool failed = false;
            try {
                next = pool->retrieve_block(1);
            } catch (const std::bad_alloc&) {
                failed = true;
                ++refused;
            }
            if (failed) {
                EXPECT_EQ(snapshot(state->registry), owners);
                EXPECT_EQ(snapshot(state->sections), sections);
                EXPECT_EQ(snapshot(state->pool), free);
            } else
                pool->return_objects(next.head.get(), 1);
            const auto calls = metadata.requests;
            metadata.refuse = true;
            pool->return_objects(block.head.get(), 1); // Earlier promised cleanup survives LATE failed admission.
            EXPECT_EQ(metadata.requests, calls);
        }
        EXPECT_EQ(metadata.bytes, 0u);
    }
    EXPECT_GT(refused, 6u);
    std::cout << "actual metadata admission refusals=" << refused << " of12 positions; earlier return completed\n";
}
