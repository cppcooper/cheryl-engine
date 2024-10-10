#include <gtest/gtest.h>
#include <resources/memory.h>
#include <testing/block.h>
#include <internals/macros/int-literals.h>
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
    CE::Logger<CE::memlog>::get().set_level_logger(spdlog::level::trace);
    CE::Logger<CE::memlog>::get().set_level_stdsink(spdlog::level::info);
    CE::Logger<CE::memlog>::get().set_level_filesink(spdlog::level::trace);
    using namespace CE;
    constexpr int samples = 12288;
    std::uniform_int_distribution<> rd(64,256);
    std::bernoulli_distribution bd(0.5);
    std::random_device rng;

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
    for(int j = 0; j < samples; ++j) {
        auto new_block = MM::get().checkout_chunk(rd(rng));
        MTRACE() << "Checked out chunk: " << new_block;
        memory.push_back(new_block);
        if(memory.size() > 4 && bd(rng)) {
            const auto sz = memory.size()-1;
            const auto lo = std::min(sz-1,std::max(sz-4, UZ(0)));
            auto idx = std::uniform_int_distribution<>(lo, sz)(rng);
            auto iter = memory.begin() + idx;
            auto a = memory.at(idx);
            MTRACE() << "Returning block: " << a;
            MM::get().return_chunk(a);
            memory.erase(iter);
        }
        if(j > 400) {
            MINFO() << "iteration: " << j;
            ASSERT_TRUE(checkPoolNotInUse(MM_bm, memory));
            ASSERT_TRUE(checkOwnerEqualsHeadInRegistry(MM_bm));
            ASSERT_TRUE(checkSectionsAndRegistryAreDisjoint(MM_bm));
            ASSERT_TRUE(checkStaleAndReleaseInRegistry(MM_bm));
            ASSERT_TRUE(checkPoolInSectionsOrInRegistry(MM_bm));
            ASSERT_TRUE(checkContiguousBlocksInPool(MM_bm));
            ASSERT_TRUE(checkPoolInRegistryAlsoInStale(MM_bm));
        }
    }
    MWARN() << MM::get().stats();
    MWARN() << MM::get().debug_info();
}
