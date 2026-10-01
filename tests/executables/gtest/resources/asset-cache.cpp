#include <templates/asset-mgr.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <utility>

namespace {
    struct CacheKey {
        int number;
        bool reject_hash = false;
        bool operator==(const CacheKey&) const = default;
    };
}

namespace std {
    template <>
    struct hash<CacheKey> {
        std::size_t operator()(const CacheKey& key) const {
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
}

TEST(asset_cache, a_duplicate_candidate_can_release_and_publish_another_key_after_lookup) {
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

TEST(asset_cache, a_rejected_insertion_releases_its_candidate_after_the_write_lock_unwinds) {
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
