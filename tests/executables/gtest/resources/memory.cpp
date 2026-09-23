#include <gtest/gtest.h>
#include <core/resources/memory.h>
#include <core/resources/objects/object-construction.hpp>
#include <testing/block.h>
#include <templates/asset-mgr.h>
#include <internals/macros/int-literals.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <random>
#include <set>

namespace {
    struct CheckedOut {
        CE::Mem::Block block;
        unsigned char pattern;
    };

    // Check the complete address partition for every owner, independent of the
    // manager's lookup and merge methods. The bookkeeping is shared by all void managers.
    ::testing::AssertionResult valid_partition(const std::vector<CheckedOut>& live) {
        BlockManagement<void> bm;
        std::shared_lock reg_lock(std::get<0>(bm.registry), std::defer_lock);
        std::shared_lock sec_lock(std::get<0>(bm.sections), std::defer_lock);
        std::shared_lock pool_lock(std::get<0>(bm.pool), std::defer_lock);
        std::shared_lock stale_lock(std::get<0>(bm.stale), std::defer_lock);
        std::shared_lock release_lock(std::get<0>(bm.release), std::defer_lock);
        std::lock(reg_lock, sec_lock, pool_lock, stale_lock, release_lock);
        const auto& registry = std::get<1>(bm.registry);
        const auto& sections = std::get<1>(bm.sections);
        const auto& pool = std::get<1>(bm.pool);
        const auto& stale = std::get<1>(bm.stale);
        const auto& release = std::get<1>(bm.release);
        auto exact = [](const auto& set, const auto& block) {
            const auto it = set.find(block);
            return it != set.end() && *it == block;
        };
        for (const auto& owner : registry) {
            if (owner.owner.get() != owner.head.get() || owner.length == 0) {
                return ::testing::AssertionFailure() << "Invalid owner " << owner;
            }
            const auto start = reinterpret_cast<std::uintptr_t>(owner.head.get());
            const auto end = start + owner.length;
            std::vector<CE::Mem::Block> pieces;
            for (const auto& section : sections) {
                if (section.owner.get() == owner.owner.get()) pieces.push_back(section);
            }
            std::sort(pieces.begin(), pieces.end(), [](const auto& a, const auto& b) {
                return a.head.get() < b.head.get();
            });
            if (pieces.empty()) {
                if (exact(pool, owner) != (stale.contains(owner) || exact(release, owner))) {
                    return ::testing::AssertionFailure() << "Owner pool/stale mismatch " << owner;
                }
            } else {
                if (exact(pool, owner)) return ::testing::AssertionFailure() << "Pooled owner has sections " << owner;
                auto cursor = start;
                for (const auto& piece : pieces) {
                    if (piece.length == 0 || reinterpret_cast<std::uintptr_t>(piece.head.get()) != cursor) {
                        return ::testing::AssertionFailure() << "Hole or overlap at " << piece;
                    }
                    cursor += piece.length;
                    if (cursor > end) return ::testing::AssertionFailure() << "Section exceeds owner " << piece;
                }
                if (cursor != end) return ::testing::AssertionFailure() << "Uncovered owner tail " << owner;
            }
        }
        for (const auto& block : pool) {
            if (exact(registry, block) == exact(sections, block)) {
                return ::testing::AssertionFailure() << "Pooled range has zero or two locations " << block;
            }
        }
        for (const auto& section : sections) {
            const auto owner = std::find_if(registry.begin(), registry.end(), [&](const auto& block) {
                return block.owner.get() == section.owner.get();
            });
            if (owner == registry.end()) {
                return ::testing::AssertionFailure() << "Section has no owner " << section;
            }
        }
        for (const auto& stale_entry : stale) {
            const auto& block = stale_entry.first;
            if (!exact(registry, block) || !exact(pool, block)) {
                return ::testing::AssertionFailure() << "Stale block is not a free owner " << block;
            }
        }
        for (const auto& block : release) {
            if (!exact(registry, block) || !exact(pool, block)) {
                return ::testing::AssertionFailure() << "Release block is not a free owner " << block;
            }
        }
        for (const auto& entry : live) {
            const auto& block = entry.block;
            if (!exact(registry, block) && !exact(sections, block)) {
                return ::testing::AssertionFailure() << "Live block is untracked " << block;
            }
            if (exact(pool, block)) return ::testing::AssertionFailure() << "Live block is pooled " << block;
            const auto first = reinterpret_cast<std::uintptr_t>(block.head.get());
            const auto last = first + block.length;
            for (const auto& free : pool) {
                if (free.owner.get() == block.owner.get()) {
                    const auto free_first = reinterpret_cast<std::uintptr_t>(free.head.get());
                    if (first < free_first + free.length && free_first < last) {
                        return ::testing::AssertionFailure() << "Live and free bytes overlap " << block;
                    }
                }
            }
        }
        return ::testing::AssertionSuccess();
    }
}

TEST(memory, seeded_checkout_return_preserves_bytes_and_owner_partitions) {
    auto& manager = CE::Mem::ExactMMgr::get();
    constexpr unsigned seed = 0xB10C42u;
    std::mt19937 rng(seed);
    std::vector<CheckedOut> live;
    constexpr std::array<std::size_t, 4> alignments{0, 64, 128, 256};

    for (int step = 0; step < 500; ++step) {
        SCOPED_TRACE(::testing::Message() << "seed=" << seed << " step=" << step);
        if (live.empty() || (live.size() < 40 && rng() % 3 != 0)) {
            const auto length = 1 + rng() % 288;
            const auto alignment = alignments[rng() % alignments.size()];
            auto block = manager.checkout_chunk(length, alignment);
            ASSERT_GE(block.length, length);
            ASSERT_EQ(reinterpret_cast<std::uintptr_t>(block.head.get()) % std::max(alignment, std::size_t{64}), 0);
            const auto pattern = static_cast<unsigned char>(1 + step % 250);
            std::memset(block.head.get(), pattern, block.length);
            live.push_back({block, pattern});
        } else {
            const auto index = rng() % live.size();
            manager.return_chunk(live[index].block);
            live.erase(live.begin() + index);
        }
        for (const auto& entry : live) {
            const auto* bytes = static_cast<const unsigned char*>(entry.block.head.get());
            for (std::size_t i = 0; i < entry.block.length; ++i) {
                ASSERT_EQ(bytes[i], entry.pattern) << "Live block changed at byte " << i;
            }
        }
        if (step % 13 == 0) {
            ASSERT_TRUE(valid_partition(live));
            ASSERT_TRUE(checkContiguousBlocksInPool(BlockManagement<void>{}));
        }
    }
    for (const auto& entry : live) manager.return_chunk(entry.block);
    EXPECT_TRUE(valid_partition({}));
}

TEST(memory, partial_return_keeps_other_bytes_in_use) {
    using namespace CE;
    auto& manager = Mem::ExactMMgr::get();
    auto block = manager.checkout_chunk(256, 64);
    ASSERT_GE(block.length, 256);
    auto* data = static_cast<unsigned char*>(block.head.get());
    std::memset(data, 0x19, 32);
    std::memset(data + 96, 0x37, 64);

    manager.return_portion(data + 32, 64);
    EXPECT_THROW(manager.return_portion(data + 32, 64), Exceptions::bad_request);
    EXPECT_THROW(manager.return_portion(data + 96, block.length), Exceptions::bad_request);
    EXPECT_EQ(data[0], 0x19);
    EXPECT_EQ(data[31], 0x19);
    EXPECT_EQ(data[96], 0x37);
    EXPECT_EQ(data[159], 0x37);
    bool returned_middle = false;
    for (const auto& free : std::get<1>(BlockManagement<void>::pool)) {
        if (free.head.get() == data + 32 && free.length == 64) {
            returned_middle = true;
        }
    }
    EXPECT_TRUE(returned_middle);

    manager.return_portion(data, 32);
    manager.return_portion(data + 96, block.length - 96);
    BlockManagement<void> bm;
    EXPECT_TRUE(checkPoolInSectionsOrInRegistry(bm));
    EXPECT_TRUE(checkPoolInRegistryAlsoInStale(bm));
}

TEST(memory, rejects_duplicate_returns) {
    using namespace CE;
    auto& manager = Mem::ExactMMgr::get();
    const auto block = manager.checkout_chunk(64, 64);
    manager.return_chunk(block);
    EXPECT_THROW(manager.return_chunk(block), Exceptions::failed_operation);
}

TEST(memory, checks_alignment_when_reusing_pooled_blocks) {
    auto& manager = CE::Mem::ExactMMgr::get();
    EXPECT_THROW(manager.checkout_chunk(0), CE::Exceptions::bad_request);
    auto lower = manager.checkout_chunk(77, 64);
    manager.return_chunk(lower);
    auto higher = manager.checkout_chunk(77, 256);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(higher.head.get()) % 256, 0);
    EXPECT_GE(higher.alignment, std::align_val_t{256});
    manager.return_chunk(higher);
    EXPECT_TRUE(valid_partition({}));
}

TEST(memory, preallocation_respects_explicit_growth_and_alignment) {
    auto& manager = CE::Mem::ExactMMgr::get();
    BlockManagement<void> bm;
    std::set<void*> before;
    {
        std::shared_lock lock(std::get<0>(bm.registry));
        for (const auto& owner : std::get<1>(bm.registry)) before.emplace(owner.head.get());
    }
    manager.preallocate(1, 32, 16, 2.0, std::align_val_t{128});
    std::vector<CE::Mem::Block> added;
    {
        std::shared_lock lock(std::get<0>(bm.registry));
        for (const auto& owner : std::get<1>(bm.registry)) {
            if (!before.contains(owner.head.get())) added.push_back(owner);
        }
    }
    ASSERT_EQ(added.size(), 1);
    EXPECT_EQ(added.front().length, 80);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(added.front().head.get()) % 128, 0);
    EXPECT_TRUE(valid_partition({}));
}

TEST(memory, typed_default_allocator_returns_all_bytes) {
    CE::Mem::DefaultAllocator<std::uint64_t> allocator;
    constexpr std::size_t count = 5;
    auto* ptr = allocator.allocate(count);
    ASSERT_NE(ptr, nullptr);
    std::size_t granted_bytes = 0;
    {
        BlockManagement<void> bm;
        std::shared_lock sec_lock(std::get<0>(bm.sections));
        for (const auto& section : std::get<1>(bm.sections)) {
            if (section.head.get() == ptr) granted_bytes = section.length;
        }
        if (granted_bytes == 0) {
            std::shared_lock reg_lock(std::get<0>(bm.registry));
            for (const auto& owner : std::get<1>(bm.registry)) {
                if (owner.head.get() == ptr) granted_bytes = owner.length;
            }
        }
    }
    ASSERT_GE(granted_bytes, sizeof(*ptr) * count);
    for (std::size_t i = 0; i < count; ++i) ptr[i] = i + 100;
    for (std::size_t i = 0; i < count; ++i) EXPECT_EQ(ptr[i], i + 100);
    allocator.deallocate(ptr, count);
    bool all_bytes_returned = false;
    {
        BlockManagement<void> bm;
        std::shared_lock lock(std::get<0>(bm.pool));
        const auto last = static_cast<void*>(reinterpret_cast<unsigned char*>(ptr) + granted_bytes - 1);
        for (const auto& free : std::get<1>(bm.pool)) {
            if (free.contains(ptr) && free.contains(last)) all_bytes_returned = true;
        }
    }
    EXPECT_TRUE(all_bytes_returned);
    EXPECT_TRUE(valid_partition({}));
}

namespace {
    struct TrackedAssetBase {
        virtual ~TrackedAssetBase() = default;
        [[nodiscard]] virtual int value() const = 0;
    };

    struct TrackedAsset final : TrackedAssetBase {
        static inline int live = 0;
        static inline int destroyed = 0;
        explicit TrackedAsset(int value) : value_(value) { ++live; }
        ~TrackedAsset() override { --live; ++destroyed; }
        [[nodiscard]] int value() const override { return value_; }

    private:
        int value_;
    };

    struct AssetCacheProbe : CE::Assets::AssetMgr<TrackedAssetBase, int> {
        using AssetMgr::allocate;
        void retain(int key, const std::shared_ptr<TrackedAsset>& asset) { loaded_assets[key] = asset; }
    };
}

TEST(memory, asset_cache_keeps_each_object_alive_until_last_handle) {
    TrackedAsset::live = 0;
    TrackedAsset::destroyed = 0;
    std::shared_ptr<TrackedAssetBase> retained;
    {
        AssetCacheProbe cache;
        EXPECT_EQ(cache.get_asset(42), nullptr);
        EXPECT_TRUE(cache.allocate<TrackedAsset>(0).empty());
        auto assets = cache.allocate<TrackedAsset>(3);
        ASSERT_EQ(assets.size(), 3);
        std::set<TrackedAsset*> unique_addresses;
        for (int i = 0; i < 3; ++i) {
            unique_addresses.emplace(assets[i].get());
            CE::Obj::ObjCtor<TrackedAsset>::construct(assets[i].get(), 1, i + 10);
            EXPECT_EQ(assets[i]->value(), i + 10);
        }
        EXPECT_EQ(unique_addresses.size(), 3);
        EXPECT_EQ(TrackedAsset::live, 3);
        cache.retain(42, assets[1]);
        retained = cache.get_asset(42);
        ASSERT_EQ(retained.get(), assets[1].get());
        EXPECT_TRUE(cache.contains(42));
        EXPECT_EQ(cache.size(), 1);
        assets.clear();
        EXPECT_EQ(TrackedAsset::live, 1);
    }
    EXPECT_EQ(TrackedAsset::live, 1);
    EXPECT_EQ(retained->value(), 11);
    retained.reset();
    EXPECT_EQ(TrackedAsset::live, 0);
    EXPECT_EQ(TrackedAsset::destroyed, 3);
}

TEST(memory, object_pool_constructs_only_requested_objects) {
    TrackedAsset::live = 0;
    TrackedAsset::destroyed = 0;
    auto& pool = CE::Obj::Pool<TrackedAsset>::get();
    EXPECT_TRUE(pool.retrieve_objects(0, 21).empty());
    EXPECT_THROW(pool.retrieve_block(0), CE::Exceptions::bad_request);
    auto objects = pool.retrieve_objects(3, 21);
    ASSERT_EQ(objects.size(), 3);
    EXPECT_EQ(TrackedAsset::live, 3);
    for (const auto& object : objects) EXPECT_EQ(object->value(), 21);

    objects[1].reset();
    EXPECT_EQ(TrackedAsset::live, 2);
    EXPECT_EQ(objects[0]->value(), 21);
    EXPECT_EQ(objects[2]->value(), 21);
    objects.clear();
    EXPECT_EQ(TrackedAsset::live, 0);
    EXPECT_EQ(TrackedAsset::destroyed, 3);

    const auto raw = pool.retrieve_block(2);
    pool.return_block(raw);
    EXPECT_THROW(pool.return_block(raw), CE::Exceptions::failed_operation);
    EXPECT_THROW(pool.return_objects(raw.head.get(), 1), CE::Exceptions::bad_request);
    EXPECT_THROW(pool.return_objects(raw.head.get(), 0), CE::Exceptions::bad_request);
}
