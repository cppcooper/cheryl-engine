#pragma once
#include <tuple>
#include <shared_mutex>
#include <templates/block.h>

template<typename Tuple>
static std::shared_mutex& get_mutex(Tuple& tuple) {
    return std::get<0>(tuple);
}

template<typename T>
bool checkPoolNotInUse(const BlockManagement<T>& bm, const std::vector<Block<T>>& in_use) {
    std::shared_lock poolLock(get_mutex(bm.pool));

    const auto& pool = std::get<1>(bm.pool);
    for(auto &b : in_use) {
        if(pool.contains(b)) {
            return false;
        }
    }
    return true;
}

template<typename T>
bool checkOwnerEqualsHeadInRegistry(const BlockManagement<T>& bm) {
    std::shared_lock registryLock(get_mutex(bm.registry));
    const auto& reg = std::get<1>(bm.registry);
    for (const auto& block : reg) {
        if (block.owner.get() != block.head.get()) {
            return false;
        }
    }

    return true;
}

template<typename T>
bool checkSectionsAndRegistryAreDisjoint(const BlockManagement<T>& bm) {
    std::shared_lock registryLock(get_mutex(bm.registry));
    std::shared_lock sectionsLock(get_mutex(bm.sections));
    const auto& reg = std::get<1>(bm.registry);
    const auto& sec = std::get<1>(bm.sections);
    for (auto &b : reg) {
        if (sec.contains(b)) {
            return false;
        }
    }
    for (auto &b : sec) {
        auto iter = reg.find(b);
        if (iter != reg.end() && *iter == b) {
            return false;
        }
    }
    return true;
}

template<typename T>
bool checkStaleAndReleaseInRegistry(const BlockManagement<T>& bm) {
    std::shared_lock staleLock(get_mutex(bm.stale));
    std::shared_lock releaseLock(get_mutex(bm.release));
    std::shared_lock registryLock(get_mutex(bm.registry));

    const auto& reg = std::get<1>(bm.registry);

    for (const auto& [staleBlock,time] : std::get<1>(bm.stale)) {
        if (reg.find(staleBlock) == reg.end()) {
            MERROR() << "Unable to find stale block " << staleBlock << " in registry.";
            return false;
        }
    }

    for (const auto& releasedBlock : std::get<1>(bm.release)) {
        if (reg.find(releasedBlock) == reg.end()) {
            MERROR() << "Unable to find released block " << releasedBlock << " in registry.";
            return false;
        }
    }

    return true;
}


template<typename T>
bool checkPoolInSectionsOrInRegistry(const BlockManagement<T>& bm) {
    std::shared_lock poolLock(get_mutex(bm.pool));
    std::shared_lock registryLock(get_mutex(bm.registry));
    std::shared_lock sectionsLock(get_mutex(bm.sections));

    const auto& reg = std::get<1>(bm.registry);
    const auto& sec = std::get<1>(bm.sections);

    for (const auto& poolBlock : std::get<1>(bm.pool)) {
        if (!reg.contains(poolBlock) && !sec.contains(poolBlock)) {
            MERROR() << "Found pool block " << poolBlock << " in neither the registry or sections.";
            return false;
        }
    }
    return true;
}

template<typename T>
bool checkPoolInRegistryAlsoInStale(const BlockManagement<T>& bm) {
    std::shared_lock poolLock(get_mutex(bm.pool));
    std::shared_lock registryLock(get_mutex(bm.registry));
    std::shared_lock staleLock(get_mutex(bm.stale));

    const auto& reg = std::get<1>(bm.registry);
    const auto& stale = std::get<1>(bm.stale);

    for (const auto& poolBlock : std::get<1>(bm.pool)) {
        if (reg.contains(poolBlock) && !stale.contains(poolBlock)) {
            MERROR() << "Found pool block " << poolBlock << " in the registry but not in stale.";
            return false;
        }
    }

    return true;
}

template<typename T>
bool checkContiguousBlocksInPool(const BlockManagement<T>& bm) {
    std::shared_lock poolLock(get_mutex(bm.pool));

    const auto& pool = std::get<1>(bm.pool);
    if (pool.size() < 2) return true; // No need to check if fewer than two blocks

    // Outer loop iterates through each block
    for (auto it1 = pool.begin(); it1 != pool.end(); ++it1) {
        auto nextIt = std::next(it1);
        // Inner loop compares current block with every subsequent block
        while (nextIt != pool.end()) {
            const auto& currentBlock = *nextIt;
            const auto& prevBlock = *it1;

            // Check if the blocks are contiguous (prevBlock's end equals currentBlock's start)
            if (BlockHelpers::is_contiguous(prevBlock, currentBlock)) {
                // Contiguous blocks cannot have the same owner
                auto sec = std::get<1>(bm.sections);
                auto s1 = sec.lower_bound(prevBlock);
                auto s2 = sec.lower_bound(currentBlock);
                if (prevBlock.owner.get() == currentBlock.owner.get()) {
                    MERROR() << "Found contiguous blocks in Pool";
                    MTRACE() << "prev: " << prevBlock;
                    MTRACE() << "current: " << currentBlock;
                    if (prevBlock.head.get() < currentBlock.head.get()) {
                        MDEBUG() << prevBlock << " is less than " << currentBlock;
                    } else {
                        MDEBUG() << prevBlock << " is greater than or equal to " << currentBlock;
                    }
                    return false;
                }
            }
            ++nextIt;
        }
    }
    return true;
}


