#include <gtest/gtest.h>
#include <templates/block.h>
#include <math/pointers.h>
#include <core/resources/memory/mem-mgr.h>
#include <testing/block.h>


TEST(templates_block, block_methods) {
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

class Test_iManage {
    iManage<void>* mgr = &CE::Mem::ExactMMgr::get();
    Block<void> merge_me;
public:
    Block<void> make_block() {
        static std::shared_ptr<void> fake(reinterpret_cast<void*>(2000),[](const void* p){});
        static Block b{fake, fake,std::align_val_t{64},1000};
        return b;
    }
    void prepare() {
        auto b = make_block();
        mgr->record_new(b);
    }
    void add_section() {
        std::unique_lock lreg(std::get<0>(BlockManagement<void>::sections));
        auto b = make_block();
        auto b2 = b.split_exactly(100);
        auto &sec = std::get<1>(BlockManagement<void>::sections);
        sec.emplace(b);
        sec.emplace(*b2);
    }
    void add_pooled() {
        std::unique_lock lpool(std::get<0>(BlockManagement<void>::pool));
        std::unique_lock lsec(std::get<0>(BlockManagement<void>::sections));
        auto &pool = std::get<1>(BlockManagement<void>::pool);
        auto &sec = std::get<1>(BlockManagement<void>::sections);
        auto b = make_block();
        auto b2 = b.split_exactly(100);
        sec.erase(*b2);
        auto b3 = b2->split_exactly(500);
        sec.emplace(*b2);
        sec.emplace(*b3);
        pool.emplace(b);
        pool.emplace(*b3);
        merge_me = *b2;
    }
    void cleanup() {
        auto ob = mgr->find_owner(reinterpret_cast<void*>(2000));
        std::unique_lock lreg(std::get<0>(BlockManagement<void>::registry));
        std::unique_lock lpool(std::get<0>(BlockManagement<void>::pool));
        std::unique_lock lsec(std::get<0>(BlockManagement<void>::sections));
        std::unique_lock lstale(std::get<0>(BlockManagement<void>::stale));
        auto &reg = std::get<1>(BlockManagement<void>::registry);
        auto &pool = std::get<1>(BlockManagement<void>::pool);
        auto &sec = std::get<1>(BlockManagement<void>::sections);
        auto &stale = std::get<1>(BlockManagement<void>::stale);
        reg.erase(*ob);
        pool.erase(*ob);
        sec.erase(*ob);
        stale.erase(*ob);
    }
    [[nodiscard]] bool test1() {
        auto b = make_block();
        auto ob = mgr->find_owner(reinterpret_cast<void*>(2000));
        return ob.has_value() && *ob == b;
    }
    [[nodiscard]] bool test2() {
        auto b = make_block();
        auto ob = mgr->find_owner(reinterpret_cast<void*>(2002));
        return ob.has_value() && *ob == b;
    }
    [[nodiscard]] bool test3() {
        auto b = make_block();
        auto ob = mgr->find_owner(reinterpret_cast<void*>(4000));
        return ob.has_value() && *ob == b;
    }
    [[nodiscard]] bool test4() const {
        return mgr->find_section(reinterpret_cast<void*>(2100)).has_value();
    }
    [[nodiscard]] bool test5() const {
        return mgr->find_section(reinterpret_cast<void*>(2200)).has_value();
    }
    [[nodiscard]] bool test6() const {
        return mgr->find_section(reinterpret_cast<void*>(4000)).has_value();
    }
    [[nodiscard]] bool test7() const {
        auto ob = mgr->merge_into_pool(merge_me);
        auto rb = mgr->find_owner(reinterpret_cast<void*>(2000));
        return ob.has_value() && rb.has_value() && *ob == *rb;
    }
    [[nodiscard]] bool test8() const {
        auto ob = mgr->fill_request(1000);
        auto rb = mgr->find_owner(reinterpret_cast<void*>(2000));
        return ob.has_value() && rb.has_value() && *ob == *rb;
    }
}t;

TEST(templates_block, iManage) {
    const std::shared_ptr<void> p1 (reinterpret_cast<void*>(10), [](const void* p){});
    const std::shared_ptr<void> p2 (reinterpret_cast<void*>(10), [](const void* p){});

    ASSERT_EQ(p1,p2);
    // find_owner tests
    t.prepare();
    ASSERT_TRUE(t.test1());
    ASSERT_TRUE(t.test2());
    ASSERT_FALSE(t.test3());
    // find_section tests
    t.add_section();
    ASSERT_TRUE(t.test4());
    ASSERT_TRUE(t.test5());
    ASSERT_FALSE(t.test6());
    // merge_into_pool test
    t.add_pooled();
    ASSERT_TRUE(t.test7());
    // fill_request test
    ASSERT_TRUE(t.test8());
    t.cleanup();
}

TEST(templates_block, BlockManagementChecks) {
    using BMv = BlockManagement<void>;
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

    const Block<void> b0 {ptr, ptr, align_val, len};
    auto b1 = b0;
    auto b2 = *b1.split_exactly(i1);
    auto b3 = *b2.split_exactly(i2);
    auto b4 = *b3.split_exactly(i3);
    auto b5 = *b4.split_exactly(i4);

    // pool is not valid with contiguous blocks present (they should merge)
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
    b4_2.owner = std::shared_ptr<void>(nullptr); // invalid, but different.. good enough for testing
    lpool.lock();
    pool.emplace(b4_2);
    lpool.unlock();
    ASSERT_TRUE(checkContiguousBlocksInPool(bm)); // (b1, b4_2, b5)
    lpool.lock();
    pool.erase(b4_2);
    pool.emplace(b2);
    lpool.unlock(); // smallest blocks contiguous (back of set)
    ASSERT_FALSE(checkContiguousBlocksInPool(bm)); // (b1, b2, b5)

    // b2 is not the owner of the block, registry should be invalid with it
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

    // b0 in registry, and b1-5 in sections is valid
    lsec.lock();
    sec.emplace(b1);
    sec.emplace(b2);
    sec.emplace(b3);
    sec.emplace(b4);
    sec.emplace(b5);
    lsec.unlock();
    ASSERT_TRUE(checkSectionsAndRegistryAreDisjoint(bm));

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

    // for any block in pool, it must be in exactly one of registry or sections
    ASSERT_TRUE(checkPoolInSectionsOrInRegistry(bm));
    lsec.lock();
    sec.emplace(b0);
    lsec.unlock();
    ASSERT_FALSE(checkPoolInSectionsOrInRegistry(bm));
    lreg.lock();
    reg.erase(b0);
    lreg.unlock();
    ASSERT_TRUE(checkPoolInSectionsOrInRegistry(bm));
}

