#include <templates/asset-mgr.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

namespace {
    struct CacheKey {
        int number;
        bool reject_hash = false;
        bool operator==(
            const CacheKey&
        ) const = default;
    };
}

namespace std {
    template <> struct hash<CacheKey> {
        std::size_t operator()(
            const CacheKey& key
        ) const {
            if (key.reject_hash)
                throw CE::Exceptions::failed_operation(CE_HERE, "Controlled cache key rejection");
            return std::hash<int>{}(key.number);
        }
    };
}

namespace {
    struct Cache final : CE::Assets::AssetMgr<const int, CacheKey> {
        spointer publish(
            const CacheKey& key,
            spointer candidate
        ) {
            return publish_asset(key, std::move(candidate));
        }
    };

    struct AllocationControl {
        enum class Failure { None, Node, Buckets };
        Failure failure = Failure::None;
        std::size_t rejected = 0;
        std::size_t outstanding = 0;
    };

    // A per-map allocator fails actual node or bucket allocation requests without
    // replacing global new or affecting the rest of the aggregate test executable.
    template <typename T> struct FailingCacheAllocator {
        using value_type = T;
        std::shared_ptr<AllocationControl> control;

        FailingCacheAllocator()
        : control(std::make_shared<AllocationControl>()) {}

        explicit FailingCacheAllocator(
            std::shared_ptr<AllocationControl> control
        ) noexcept
        : control(std::move(control)) {}

        template <typename U>
        FailingCacheAllocator(
            const FailingCacheAllocator<U>& other
        ) noexcept
        : control(other.control) {}

        T* allocate(
            const std::size_t count
        ) {
            const auto phase = count == 1 ? AllocationControl::Failure::Node : AllocationControl::Failure::Buckets;
            if (control->failure == phase) {
                ++control->rejected;
                throw CE::Exceptions::bad_alloc(CE_HERE);
            }
            auto* result = std::allocator<T>{}.allocate(count);
            ++control->outstanding;
            return result;
        }

        void deallocate(
            T* pointer,
            const std::size_t count
        ) noexcept {
            --control->outstanding;
            std::allocator<T>{}.deallocate(pointer, count);
        }

        template <typename U>
        bool operator==(
            const FailingCacheAllocator<U>& other
        ) const noexcept {
            return control == other.control;
        }
    };

    using AllocationCacheBase =
        CE::Assets::AssetMgr<const int, CacheKey, FailingCacheAllocator<std::pair<const CacheKey, std::shared_ptr<const int>>>>;

    struct AllocationCache final : AllocationCacheBase {
        explicit AllocationCache(
            const std::shared_ptr<AllocationControl>& control
        )
        : AllocationCacheBase(FailingCacheAllocator<std::pair<const CacheKey, std::shared_ptr<const int>>>{control}) {}

        spointer publish(
            const CacheKey& key,
            spointer candidate
        ) {
            return publish_asset(key, std::move(candidate));
        }

        template <typename Published>
        spointer publish(
            const CacheKey& key,
            spointer candidate,
            Published&& published
        ) {
            return publish_asset(key, std::move(candidate), std::forward<Published>(published));
        }

        spointer replace(
            const CacheKey& key,
            spointer candidate
        ) {
            return replace_asset(key, std::move(candidate));
        }

        void require_growth() {
            std::unique_lock lock(assets_mutex_);
            // The next insertion crosses the existing bucket load threshold.
            loaded_assets.max_load_factor(0.01f);
        }
    };
}

TEST(
    asset_cache,
    a_duplicate_candidate_can_release_and_publish_another_key_after_lookup
) {
    Cache cache;
    auto original = cache.publish({1}, std::make_shared<const int>(10));
    bool released = false;
    auto duplicate = std::shared_ptr<const int>(new int{20}, [&](const int* value) {
        released = true;
        EXPECT_EQ(cache.get_asset({1}), original);
        cache.publish({2}, std::make_shared<const int>(30));
        delete value;
    });
    auto retained = cache.publish({1}, std::move(duplicate));
    EXPECT_TRUE(released);
    EXPECT_EQ(retained, original);
    ASSERT_TRUE(cache.get_asset({2}));
    EXPECT_EQ(*cache.get_asset({2}), 30);
}

TEST(
    asset_cache,
    a_rejected_insertion_releases_its_candidate_after_the_write_lock_unwinds
) {
    Cache cache;
    auto original = cache.publish({1}, std::make_shared<const int>(10));
    bool released = false;
    auto candidate = std::shared_ptr<const int>(new int{20}, [&](const int* value) {
        released = true;
        EXPECT_EQ(cache.get_asset({1}), original);
        EXPECT_EQ(cache.size(), 1u);
        delete value;
    });
    EXPECT_THROW(cache.publish({2, true}, std::move(candidate)), CE::Exceptions::failed_operation);
    EXPECT_TRUE(released);
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_EQ(cache.get_asset({1}), original);
}

TEST(
    asset_cache,
    node_and_rehash_failures_release_candidates_after_unlock_and_do_not_commit_metadata
) {
    using Failure = AllocationControl::Failure;
    for (const auto phase : {Failure::Node, Failure::Buckets}) {
        SCOPED_TRACE(static_cast<int>(phase));
        auto control = std::make_shared<AllocationControl>();
        {
            AllocationCache cache(control);
            auto original = cache.publish({1}, std::make_shared<const int>(10));
            bool committed = false;
            bool released = false;
            auto candidate = std::shared_ptr<const int>(new int{20}, [&](const int* value) {
                control->failure = Failure::None;
                released = true;
                EXPECT_FALSE(committed);
                EXPECT_EQ(cache.get_asset({1}), original);
                EXPECT_EQ(cache.size(), 1u);
                cache.publish({3}, std::make_shared<const int>(30));
                delete value;
            });
            cache.require_growth();
            control->failure = phase;
            EXPECT_THROW(cache.publish({2}, std::move(candidate), [&]() noexcept { committed = true; }), CE::Exceptions::bad_alloc);
            EXPECT_TRUE(released);
            EXPECT_FALSE(committed);
            EXPECT_EQ(control->rejected, 1u);
            EXPECT_FALSE(cache.contains({2}));
            EXPECT_EQ(cache.get_asset({1}), original);
            EXPECT_EQ(*cache.get_asset({3}), 30);
            auto recovered = cache.publish({2}, std::make_shared<const int>(40), [&]() noexcept { committed = true; });
            EXPECT_TRUE(committed);
            EXPECT_EQ(cache.get_asset({2}), recovered);
            cache.clear_assets();
            EXPECT_EQ(cache.size(), 0u);
            EXPECT_EQ(*original, 10); // External owners survive failed publication and clear.
            EXPECT_EQ(*recovered, 40);
        }
        EXPECT_EQ(control->outstanding, 0u);
    }
}

TEST(
    asset_cache,
    failed_replacement_insertion_preserves_existing_generation_and_allows_deleter_reentry
) {
    using Failure = AllocationControl::Failure;
    for (const auto phase : {Failure::Node, Failure::Buckets}) {
        SCOPED_TRACE(static_cast<int>(phase));
        auto control = std::make_shared<AllocationControl>();
        {
            AllocationCache cache(control);
            auto original = cache.publish({1}, std::make_shared<const int>(10));
            bool released = false;
            auto candidate = std::shared_ptr<const int>(new int{20}, [&](const int* value) {
                control->failure = Failure::None;
                released = true;
                EXPECT_EQ(cache.get_asset({1}), original);
                EXPECT_EQ(cache.size(), 1u);
                cache.replace({3}, std::make_shared<const int>(30));
                delete value;
            });
            cache.require_growth();
            control->failure = phase;
            EXPECT_THROW(cache.replace({2}, std::move(candidate)), CE::Exceptions::bad_alloc);
            EXPECT_TRUE(released);
            EXPECT_EQ(control->rejected, 1u);
            EXPECT_FALSE(cache.contains({2}));
            EXPECT_EQ(cache.get_asset({1}), original);
            EXPECT_EQ(*cache.get_asset({3}), 30);
            cache.clear_assets();
        }
        EXPECT_EQ(control->outstanding, 0u);
    }
}

TEST(
    asset_cache,
    stateful_allocator_clear_releases_values_outside_the_lock_and_keeps_allocator_identity
) {
    auto control = std::make_shared<AllocationControl>();
    {
        AllocationCache cache(control);
        bool released = false;
        auto value = std::shared_ptr<const int>(new int{10}, [&](const int* pointer) {
            released = true;
            EXPECT_EQ(cache.size(), 0u);
            delete pointer;
        });
        cache.publish({1}, std::move(value));
        cache.clear_assets();
        EXPECT_TRUE(released);
        cache.publish({2}, std::make_shared<const int>(20));
        cache.clear_assets();
    }
    EXPECT_EQ(control->outstanding, 0u);
}
