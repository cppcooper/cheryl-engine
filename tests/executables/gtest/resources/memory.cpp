#include <gtest/gtest.h>
#include <resources/memory.h>
#include <testing/block.h>
#include <internals/macros/int-literals.h>
#include <random>

inline bool is_po2(size_t idx) {
    for(uint64_t i = 11; i <= 63; ++i) {
        if (1ull << i == idx) [[unlikely]] {
            return true;
        }
    }
    return false;
}



TEST(memory, manager) {
    using namespace CE;
    constexpr int samples = 12288;
    std::uniform_int_distribution<> rd(64,512);
    std::bernoulli_distribution bd(0.17);
    std::random_device rng;
    std::size_t returned_blks = 0;

    using MM = Mem::CacheMMgr;
    MM::get().preallocate(1, 1024*1024,6144, 1.0, std::align_val_t{128});
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
            const auto sz = std::max(UZ(0), memory.size()-1);
            const auto lo = std::min(std::max(UZ(0),sz-1),std::max(UZ(0),sz-4));
            auto idx = std::uniform_int_distribution<>(lo, sz)(rng);
            auto iter = memory.begin() + idx;
            if (idx > memory.size()) continue;
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

    // todo: highly probable to fail
    ASSERT_TRUE(checkContiguousBlocksInPool(MM_bm));
    ASSERT_TRUE(checkPoolInRegistryAlsoInStale(MM_bm));
}
