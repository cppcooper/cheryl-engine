#include <gtest/gtest.h>
#include <templates/block.h>
#include <math/pointers.h>
#include <core/resources/memory/mem-mgr.h>
#include <testing/block.h>
#include <set>
#include <shared_mutex>
#include <tuple>
#include <cstdint>


TEST(templates_block, block_methods) {
    // All split heads must remain offsets into one allocation; an out-of-range
    // split leaves the original range unchanged.
    constexpr std::size_t len = 2048;
    char* p_raw = new char[len];
    std::shared_ptr<char> ptr(p_raw, [](const char* p){ delete[] p; });
    auto align_val = CE::ptr::calculate_alignment(p_raw);
    constexpr std::size_t i1 = 12;
    constexpr std::size_t i2 = 20;
    constexpr std::size_t i3 = 107;
    constexpr std::size_t i4 = 4;
    constexpr std::size_t i5 = 1024;
    Block<char> b {ptr, ptr, align_val, len};
    ASSERT_TRUE(b.contains(p_raw+i5));
    ASSERT_TRUE(!b.contains(p_raw+len));
    auto b2 = b.split_exactly(i1);
    ASSERT_TRUE(b2.has_value());
    ASSERT_EQ(b2->head.get(), p_raw+i1);
    auto b3 = b2->split_exactly(i2);
    ASSERT_TRUE(b3.has_value());
    ASSERT_EQ(b3->head.get(), p_raw+i1+i2);
    auto b4 = b3->split_exactly(i3);
    ASSERT_TRUE(b4.has_value());
    ASSERT_EQ(b4->head.get(), p_raw+i1+i2+i3);
    auto b5 = b.split_exactly(i4);
    ASSERT_TRUE(b5.has_value());
    ASSERT_EQ(b5->head.get(), p_raw+i4);
    auto b6 = b.split_exactly(i3);
    ASSERT_TRUE(!b6.has_value());
    ASSERT_TRUE(b4->contains(p_raw+i5));
}

TEST(templates_block, typed_split_preserves_element_ranges) {
    // Typed lengths are element counts, while contains() tests byte addresses
    // through the last element's end.
    struct Value { std::uint64_t data; };
    auto backing = std::shared_ptr<Value>(new Value[4], [](Value* p) { delete[] p; });
    Block<Value> block{backing, backing, CE::ptr::calculate_alignment(backing.get()), 4};

    auto unchanged = block;
    EXPECT_FALSE(unchanged.split_exactly(0).has_value());
    EXPECT_EQ(unchanged.length, 4);
    EXPECT_EQ(CE::ptr::get_alignment_offset(backing.get(), std::align_val_t{alignof(Value)}), 0);
    EXPECT_TRUE(block.contains(reinterpret_cast<unsigned char*>(backing.get() + 4) - 1));
    EXPECT_FALSE(block.contains(backing.get() + 4));

    auto rest = block.split_exactly(1);
    ASSERT_TRUE(rest.has_value());
    EXPECT_EQ(rest->head.get(), backing.get() + 1);
    EXPECT_EQ(block.length, 1);
    EXPECT_TRUE(block.contains(backing.get()));
    EXPECT_FALSE(block.contains(backing.get() + 1));
    EXPECT_TRUE(BlockHelpers::is_contiguous(block, *rest));

    auto last = rest->split_exactly(2);
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->head.get(), backing.get() + 3);
    EXPECT_EQ(last->length, 1);
    EXPECT_TRUE(BlockHelpers::is_contiguous(*rest, *last));
    auto aligned = unchanged.split_at(3);
    ASSERT_TRUE(aligned.has_value());
    EXPECT_EQ(aligned->head.get(), backing.get() + 3);
    EXPECT_EQ(aligned->length, 1);
}

struct CullProbeItem { char value; };

struct CullProbe : AbstractManager<CullProbeItem> {
    using AbstractManager<CullProbeItem>::record_new;
    using AbstractManager<CullProbeItem>::merge_into_pool;
};

TEST(templates_block, cull_only_reclaims_complete_stale_owners) {
    // Two fully returned owners become stale; the age threshold decides when
    // they enter the release queue, and release_culled removes their records.
    CullProbe manager;
    BlockManagement<CullProbeItem> bm;
    auto make_owner = [] {
        auto p = std::shared_ptr<CullProbeItem>(new CullProbeItem[64], [](CullProbeItem* p) { delete[] p; });
        return Block<CullProbeItem>{p, p, CE::ptr::calculate_alignment(p.get()), 64};
    };
    const auto first = make_owner();
    const auto second = make_owner();
    manager.record_new(first);
    manager.record_new(second);
    manager.merge_into_pool(first);
    manager.merge_into_pool(second);

    manager.cull(std::chrono::minutes(1));
    EXPECT_TRUE(std::get<1>(bm.release).empty());
    manager.cull(std::chrono::minutes(0));
    EXPECT_EQ(std::get<1>(bm.release).size(), 2);
    EXPECT_TRUE(std::get<1>(bm.stale).empty());
    manager.release_culled();
    EXPECT_TRUE(std::get<1>(bm.release).empty());
    EXPECT_TRUE(std::get<1>(bm.pool).empty());
    EXPECT_TRUE(std::get<1>(bm.registry).empty());
}

struct AdjacentBlockProbe : AbstractManager<char> {
    using AbstractManager<char>::adjacent_left;
    using AbstractManager<char>::search_left;
    using AbstractManager<char>::search_right;
};

TEST(templates_block, adjacent_left) {
    // Exercise predecessor lookup in two different set orderings before
    // checking address-based search within a shared owner.
    auto backing = std::shared_ptr<char>(new char[128], [](const char* p) { delete[] p; });
    auto secondHead = std::shared_ptr<char>(backing, backing.get() + 64);
    const Block<char> first{backing, backing, CE::ptr::calculate_alignment(backing.get()), 32};
    const Block<char> second{secondHead, secondHead, CE::ptr::calculate_alignment(secondHead.get()), 32};
    auto middleHead = std::shared_ptr<char>(backing, backing.get() + 48);
    const Block<char> middle{middleHead, middleHead, CE::ptr::calculate_alignment(middleHead.get()), 1};
    auto beyondHead = std::shared_ptr<char>(backing, backing.get() + 96);
    const Block<char> beyond{beyondHead, beyondHead, CE::ptr::calculate_alignment(beyondHead.get()), 1};

    std::tuple<std::shared_mutex, std::set<Block<char>, compare::HeadOrder<char>>> sections;
    std::tuple<std::shared_mutex, std::set<Block<char>, compare::RegistryOrder<char>>> registry;
    std::get<1>(sections).insert({first, second});
    std::get<1>(registry).insert({first, second});

    AdjacentBlockProbe manager;
    EXPECT_FALSE(manager.adjacent_left(first, sections).has_value());
    EXPECT_FALSE(manager.adjacent_left(first, registry).has_value());
    EXPECT_EQ(manager.adjacent_left(middle, sections), first);
    EXPECT_EQ(manager.adjacent_left(middle, registry), first);
    EXPECT_EQ(manager.adjacent_left(beyond, sections), second);
    EXPECT_EQ(manager.adjacent_left(beyond, registry), second);
    auto same_owner_second = second;
    same_owner_second.owner = backing;
    auto same_owner_beyond = beyond;
    same_owner_beyond.owner = backing;
    std::tuple<std::shared_mutex, std::set<Block<char>, compare::HeadOrder<char>>> owner_sections;
    std::get<1>(owner_sections).insert({first, same_owner_second});
    EXPECT_EQ(manager.search_left(same_owner_second, owner_sections), first);
    EXPECT_EQ(manager.search_left(same_owner_beyond, owner_sections), same_owner_second);
    EXPECT_FALSE(manager.search_left(first, owner_sections).has_value());
    EXPECT_FALSE(manager.search_right(same_owner_second, owner_sections).has_value());
}

struct MergeProbeItem { char value; };

struct MergeProbe : AbstractManager<MergeProbeItem> {
    using AbstractManager<MergeProbeItem>::merge_into_pool;
    using AbstractManager<MergeProbeItem>::record_new;
};

TEST(templates_block, pool_merge_at_owner_head) {
    auto backing = std::shared_ptr<MergeProbeItem>(new MergeProbeItem[128], [](MergeProbeItem* p) { delete[] p; });
    const Block<MergeProbeItem> owner{backing, backing, CE::ptr::calculate_alignment(backing.get()), 128};
    auto front = owner;
    auto middle = *front.split_exactly(32);
    auto back = *middle.split_exactly(32);

    MergeProbe manager;
    BlockManagement<MergeProbeItem> bm;
    auto& registry = std::get<1>(bm.registry);
    auto& sections = std::get<1>(bm.sections);
    auto& pool = std::get<1>(bm.pool);
    auto& stale = std::get<1>(bm.stale);
    manager.record_new(owner);
    sections.emplace(front);
    sections.emplace(middle);
    sections.emplace(back);
    pool.emplace(middle);

    // Returning the front merges it with the middle free section, but the
    // owner stays active while the back section has not been returned.
    const auto partial = manager.merge_into_pool(front);
    EXPECT_TRUE(partial.has_value());
    if (partial) {
        EXPECT_EQ(partial->length, 64);
        EXPECT_TRUE(sections.contains(*partial));
    }
    EXPECT_TRUE(checkPoolInSectionsOrInRegistry(bm));
    EXPECT_FALSE(stale.contains(owner));

    // The final section completes the original owner. Only now may it move
    // from sections to a stale, fully pooled registry entry.
    const auto complete = manager.merge_into_pool(back);
    EXPECT_TRUE(complete.has_value());
    if (complete) {
        EXPECT_EQ(*complete, owner);
    }
    EXPECT_TRUE(sections.empty());
    EXPECT_TRUE(pool.contains(owner));
    EXPECT_TRUE(stale.contains(owner));
    EXPECT_TRUE(checkPoolInSectionsOrInRegistry(bm));

    pool.clear();
    sections.clear();
    registry.clear();
    stale.clear();
}

struct ManageProbeItem { unsigned char value; };

// iManage grants this test access to the virtual interface. Use real owned
// allocations and a dedicated T so other managers cannot affect the result.
class Test_iManage {
    AbstractManager<ManageProbeItem> implementation_;
    iManage<ManageProbeItem>* mgr_ = &implementation_;
public:
    ~Test_iManage() {
        BlockManagement<ManageProbeItem> bm;
        std::scoped_lock lock(std::get<0>(bm.registry), std::get<0>(bm.sections),
                              std::get<0>(bm.pool), std::get<0>(bm.stale), std::get<0>(bm.release));
        std::get<1>(bm.registry).clear();
        std::get<1>(bm.sections).clear();
        std::get<1>(bm.pool).clear();
        std::get<1>(bm.stale).clear();
        std::get<1>(bm.release).clear();
    }
    void record(Block<ManageProbeItem> block) { mgr_->record_new(block); }
    OBlock<ManageProbeItem> owner(ManageProbeItem* ptr) { return mgr_->find_owner(ptr); }
    OBlock<ManageProbeItem> section(ManageProbeItem* ptr) { return mgr_->find_section(ptr); }
    OBlock<ManageProbeItem> merge(Block<ManageProbeItem> block) { return mgr_->merge_into_pool(block); }
    OBlock<ManageProbeItem> fill(std::size_t count, std::align_val_t alignment = std::align_val_t{0}) {
        return mgr_->fill_request(count, alignment);
    }
};

TEST(templates_block, iManage) {
    Test_iManage test;
    auto make_owner = [] {
        auto memory = std::shared_ptr<ManageProbeItem>(new ManageProbeItem[128],
                                                        [](ManageProbeItem* p) { delete[] p; });
        return Block<ManageProbeItem>{memory, memory, CE::ptr::calculate_alignment(memory.get()), 128};
    };
    auto first = make_owner();
    auto second = make_owner();
    if (std::less<ManageProbeItem*>{}(second.head.get(), first.head.get())) {
        std::swap(first, second);
    }
    test.record(first);
    EXPECT_EQ(test.owner(first.head.get()), first);
    EXPECT_EQ(test.owner(first.head.get() + 17), first);
    EXPECT_FALSE(test.owner(second.head.get()).has_value());
    test.record(second);
    EXPECT_EQ(test.owner(first.head.get() + 17), first);
    EXPECT_EQ(test.owner(second.head.get() + 17), second);

    // Split the first owner into three address ranges; queries must find the
    // containing section even though both owners remain in the registry.
    auto front = first;
    auto middle = *front.split_exactly(32);
    auto back = *middle.split_exactly(32);
    BlockManagement<ManageProbeItem> bm;
    {
        std::unique_lock lock(std::get<0>(bm.sections));
        auto& sections = std::get<1>(bm.sections);
        sections.emplace(front);
        sections.emplace(middle);
        sections.emplace(back);
        sections.emplace(second);
    }
    EXPECT_EQ(test.section(first.head.get() + 40), middle);
    EXPECT_EQ(test.section(first.head.get() + 95), back);
    EXPECT_EQ(test.section(second.head.get() + 40), second);
    {
        std::unique_lock lock(std::get<0>(bm.pool));
        std::get<1>(bm.pool).emplace(middle);
    }
    const auto partial = test.merge(front);
    ASSERT_TRUE(partial.has_value());
    EXPECT_EQ(partial->length, 64);
    EXPECT_FALSE(std::get<1>(bm.stale).contains(first));
    // Joining the remaining end restores the full owner. A fill request
    // then respects alignment before reusing that complete pooled block.
    const auto whole = test.merge(back);
    ASSERT_TRUE(whole.has_value());
    EXPECT_EQ(*whole, first);
    EXPECT_TRUE(std::get<1>(bm.stale).contains(first));
    if (first.alignment < std::align_val_t{128}) {
        EXPECT_FALSE(test.fill(128, std::align_val_t{128}).has_value());
    }
    EXPECT_EQ(test.fill(128), first);
}

TEST(templates_block, BlockManagementChecks) {
    // Use a separate specialization so memory manager tests cannot affect these checks.
    using BMv = BlockManagement<char>;
    std::unique_lock lreg(std::get<0>(BMv::registry));
    std::unique_lock lsec(std::get<0>(BMv::sections));
    std::unique_lock lpool(std::get<0>(BMv::pool));
    std::unique_lock lstale(std::get<0>(BMv::stale));
    std::unique_lock lrelease(std::get<0>(BMv::release));
    lreg.unlock();
    lsec.unlock();
    lpool.unlock();
    lstale.unlock();
    lrelease.unlock();

    auto bm = BMv();
    auto &reg = std::get<1>(bm.registry);
    auto &sec = std::get<1>(bm.sections);
    auto &pool = std::get<1>(bm.pool);
    auto &release = std::get<1>(bm.release);
    auto &stale = std::get<1>(bm.stale);

    constexpr std::size_t len = 2048;
    char* p_raw = new char[len];
    std::shared_ptr<char> ptr(p_raw, [](const char* p){ delete[] p; });
    auto align_val = CE::ptr::calculate_alignment(p_raw);
    constexpr std::size_t i1 = 64;
    constexpr std::size_t i2 = 128;
    constexpr std::size_t i3 = 256;
    constexpr std::size_t i4 = 512;

    const Block<char> b0 {ptr, ptr, align_val, len};
    auto b1 = b0;
    auto b2 = *b1.split_exactly(i1);
    auto b3 = *b2.split_exactly(i2);
    auto b4 = *b3.split_exactly(i3);
    auto b5 = *b4.split_exactly(i4);

    // Seed inconsistent free ranges: adjacent pieces with one owner must
    // merge, whereas adjacent ranges with distinct owners need not.
    lpool.lock();
    pool.emplace(b1);
    pool.emplace(b4);
    pool.emplace(b5);
    lpool.unlock(); // largest blocks contiguous (front of set)
    ASSERT_FALSE(checkContiguousBlocksInPool(bm)); // (b1,b4,b5)
    lpool.lock();
    pool.erase(b4);
    lpool.unlock();
    ASSERT_TRUE(checkContiguousBlocksInPool(bm)); // (b1,b5)
    // in the unlikely event that two contiguous blocks have different owners.. that's fine
    auto b4_2 = b4;
    b4_2.owner = std::shared_ptr<char>(nullptr); // invalid, but different.. good enough for testing
    lpool.lock();
    pool.emplace(b4_2);
    lpool.unlock();
    ASSERT_TRUE(checkContiguousBlocksInPool(bm)); // (b1, b4_2, b5)
    lpool.lock();
    pool.erase(b4_2);
    pool.emplace(b2);
    lpool.unlock(); // smallest blocks contiguous (back of set)
    ASSERT_FALSE(checkContiguousBlocksInPool(bm)); // (b1, b2, b5)

    // A registry entry must describe the complete allocation. Stale and
    // release entries must also refer to an owner present in that registry.
    lreg.lock();
    reg.emplace(b2);
    lreg.unlock();
    ASSERT_FALSE(checkOwnerEqualsHeadInRegistry(bm));

    // b0 being in stale and release but not registry means bm should not be valid
    lstale.lock();
    stale.emplace(b0, BMv::clock::now());
    lstale.unlock();
    lrelease.lock();
    release.emplace(b0);
    lrelease.unlock();
    ASSERT_FALSE(checkStaleAndReleaseInRegistry(bm));

    lreg.lock();
    reg.erase(b2);
    // b0 is the owner of the block, registry should be valid with it
    reg.emplace(b0);
    lreg.unlock();
    ASSERT_TRUE(checkOwnerEqualsHeadInRegistry(bm));

    // b0 now also being in registry means all three are valid
    ASSERT_TRUE(checkStaleAndReleaseInRegistry(bm));

    // The split pieces may share an owner with b0 without being the same
    // block. Exact section identity matters at the owner's starting address.
    lsec.lock();
    sec.emplace(b1);
    sec.emplace(b2);
    sec.emplace(b3);
    sec.emplace(b4);
    sec.emplace(b5);
    lsec.unlock();
    ASSERT_TRUE(checkSectionsAndRegistryAreDisjoint(bm));
    // b1 starts at b0's head but is shorter, so only the exact section matches.
    ASSERT_TRUE(checkPoolInSectionsOrInRegistry(bm));
    ASSERT_TRUE(checkPoolInRegistryAlsoInStale(bm));

    // if there is a Block in both registry and sections, then bm is invalid... b0 for example
    lsec.lock();
    sec.emplace(b0);
    lsec.unlock();
    ASSERT_FALSE(checkSectionsAndRegistryAreDisjoint(bm));
    lsec.lock();
    sec.erase(b0);
    sec.erase(b1);
    sec.erase(b2);
    sec.erase(b3);
    sec.erase(b4);
    sec.erase(b5);
    lsec.unlock();

    lpool.lock();
    pool.erase(b1);
    pool.erase(b2);
    pool.erase(b5);
    // if a block is in registry, and pool then it must be in stale as well
    pool.emplace(b0);
    lpool.unlock();
    ASSERT_TRUE(checkPoolInRegistryAlsoInStale(bm));
    lstale.lock();
    stale.erase(b0);
    lstale.unlock();
    ASSERT_FALSE(checkPoolInRegistryAlsoInStale(bm));

    // Move b0 between registry and sections to test both invalid cases:
    // membership in both sets, then membership in neither set.
    ASSERT_TRUE(checkPoolInSectionsOrInRegistry(bm));
    lsec.lock();
    sec.emplace(b0);
    lsec.unlock();
    ASSERT_FALSE(checkPoolInSectionsOrInRegistry(bm));
    lreg.lock();
    reg.erase(b0);
    lreg.unlock();
    ASSERT_TRUE(checkPoolInSectionsOrInRegistry(bm));

    lsec.lock();
    sec.erase(b0);
    lsec.unlock();
    ASSERT_FALSE(checkPoolInSectionsOrInRegistry(bm));

    // Remove all state belonging to this test; the management sets are static.
    lpool.lock();
    pool.erase(b0);
    lpool.unlock();
    lrelease.lock();
    release.erase(b0);
    lrelease.unlock();
}
