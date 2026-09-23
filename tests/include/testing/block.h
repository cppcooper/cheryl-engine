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
        const auto owner = reg.find(staleBlock);
        if (owner == reg.end() || *owner != staleBlock) {
            MERROR() << "Unable to find stale block " << staleBlock << " in registry.";
            return false;
        }
    }

    for (const auto& releasedBlock : std::get<1>(bm.release)) {
        const auto owner = reg.find(releasedBlock);
        if (owner == reg.end() || *owner != releasedBlock) {
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
        const auto registryBlock = reg.find(poolBlock);
        const auto sectionBlock = sec.find(poolBlock);
        const bool inRegistry = registryBlock != reg.end() && *registryBlock == poolBlock;
        const bool inSections = sectionBlock != sec.end() && *sectionBlock == poolBlock;
        if (inRegistry == inSections) {
            MERROR() << "Found pool block " << poolBlock << " in both or neither of the registry and sections.";
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
        const auto registryBlock = reg.find(poolBlock);
        if (registryBlock != reg.end() && *registryBlock == poolBlock && !stale.contains(poolBlock)) {
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
    constexpr std::size_t element_size = [] {
        if constexpr (std::is_void_v<T>) return std::size_t{1};
        else return sizeof(T);
    }();
    for (auto first = pool.begin(); first != pool.end(); ++first) {
        for (auto second = std::next(first); second != pool.end(); ++second) {
            if (first->owner.get() != second->owner.get()) continue;
            const auto a = reinterpret_cast<std::uintptr_t>(first->head.get());
            const auto b = reinterpret_cast<std::uintptr_t>(second->head.get());
            const auto a_end = a + first->length * element_size;
            const auto b_end = b + second->length * element_size;
            if (a <= b_end && b <= a_end) {
                return false; // two free ranges overlap or should have been merged
            }
        }
    }
    return true;
}
