#include <gtest/gtest.h>
#include <resources/memory.h>
#include <testing/block.h>
#include <internal/macros/int-literals.h>
#include <random>

inline bool is_po2(size_t idx) {
    for(uint64_t i = 0; i <= 63; ++i) {
        if (1ull << i == idx) [[unlikely]] {
            return true;
        }
    }
    return false;
}



TEST(memory, manager) {
    using namespace CE;
    constexpr int samples = 12288;
    std::uniform_int_distribution<> rd(64,256);
    std::bernoulli_distribution bd(0.07);
    std::random_device rng;
    std::size_t returned_blks = 0;

    using MM = Mem::CacheMMgr;
    Mem::ExactMMgr::get().preallocate(64,1024);
    Mem::Manager<2.5, 6144>::get().preallocate(512, 6144);
    std::vector<Mem::Block> memory;
    BlockManagement<void> MM_bm;
    ASSERT_TRUE(checkOwnerEqualsHeadInRegistry(MM_bm));
    ASSERT_TRUE(checkSectionsAndRegistryAreDisjoint(MM_bm));
    ASSERT_TRUE(checkStaleAndReleaseInRegistry(MM_bm));
    ASSERT_TRUE(checkPoolInSectionsOrInRegistry(MM_bm));
    ASSERT_TRUE(checkContiguousBlocksInPool(MM_bm));
    ASSERT_TRUE(checkPoolInRegistryAlsoInStale(MM_bm));
    for(int i = 0; i < samples; ++i) {
        memory.push_back(MM::get().checkout_chunk(rd(rng)));
        if(!memory.empty() && bd(rng)) {
            std::cout << "returning..";
            returned_blks++;
            const auto sz = memory.size()-1;
            const auto lo = std::min(sz-1,std::max(sz-4, UZ(0)));
            auto idx = std::uniform_int_distribution<>(lo, sz)(rng);
            auto iter = memory.begin() + idx;
            auto a = memory.at(idx);
            MM::get().return_chunk(a);
            memory.erase(iter);
        }
        if (is_po2(i)) {
            std::cout << MM::get().stats();
        }
    }
    std::cout << std::endl;
    ASSERT_TRUE(checkOwnerEqualsHeadInRegistry(MM_bm));
    ASSERT_TRUE(checkSectionsAndRegistryAreDisjoint(MM_bm));
    ASSERT_TRUE(checkStaleAndReleaseInRegistry(MM_bm));
    ASSERT_TRUE(checkPoolInSectionsOrInRegistry(MM_bm));
    ASSERT_TRUE(checkContiguousBlocksInPool(MM_bm));
    ASSERT_TRUE(checkPoolInRegistryAlsoInStale(MM_bm));
    for(int i = 0; i < samples; ++i) {
        memory.push_back(MM::get().checkout_chunk(rd(rng)));
        if(!memory.empty() && bd(rng)) {
            std::cout << "returning..";
            returned_blks++;
            const auto sz = memory.size()-1;
            const auto lo = std::min(sz-1,std::max(sz-4, UZ(0)));
            auto idx = std::uniform_int_distribution<>(lo, sz)(rng);
            auto iter = memory.begin() + idx;
            auto a = memory.at(idx);
            MM::get().return_chunk(a);
            memory.erase(iter);
        }
    }
    std::cout << std::endl;
    ASSERT_TRUE(checkOwnerEqualsHeadInRegistry(MM_bm));
    ASSERT_TRUE(checkSectionsAndRegistryAreDisjoint(MM_bm));
    ASSERT_TRUE(checkStaleAndReleaseInRegistry(MM_bm));
    ASSERT_TRUE(checkPoolInSectionsOrInRegistry(MM_bm));
    ASSERT_TRUE(checkContiguousBlocksInPool(MM_bm));
    ASSERT_TRUE(checkPoolInRegistryAlsoInStale(MM_bm));
    for(int i = 0; i < samples; ++i) {
        memory.push_back(MM::get().checkout_chunk(rd(rng)));
        if(!memory.empty() && bd(rng)) {
            std::cout << "returning..";
            returned_blks++;
            const auto sz = memory.size()-1;
            const auto lo = std::min(sz-1,std::max(sz-4, UZ(0)));
            auto idx = std::uniform_int_distribution<>(lo, sz)(rng);
            auto iter = memory.begin() + idx;
            auto a = memory.at(idx);
            MM::get().return_chunk(a);
            memory.erase(iter);
        }
    }
    std::cout << std::endl;
    ASSERT_TRUE(checkOwnerEqualsHeadInRegistry(MM_bm));
    ASSERT_TRUE(checkSectionsAndRegistryAreDisjoint(MM_bm));
    ASSERT_TRUE(checkStaleAndReleaseInRegistry(MM_bm));
    ASSERT_TRUE(checkPoolInSectionsOrInRegistry(MM_bm));
    ASSERT_TRUE(checkContiguousBlocksInPool(MM_bm));
    ASSERT_TRUE(checkPoolInRegistryAlsoInStale(MM_bm));
}
