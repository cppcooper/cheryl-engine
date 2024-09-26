#pragma once
#ifndef BLOCK_TEMPLATE_H
#define BLOCK_TEMPLATE_H
#include <cemath.h>
#include <functional>
#include <memory>
#include <chrono>
#include <set>
#include <vector>
#include <tuple>
#include <shared_mutex>
#include <unordered_map>
#include <mutex>

/* Header: block.h
 *
 * includes
 * Block<T>
 * OBlock<T>
 * operator==
 *
 * // inline functions
 * swap(Block<T>&,Block<T>&)
 * is_contiguous(Block<T>,Block<T>)
 *
 * // sorting Block<T>
 * namespace compare
 *  HeadOrder
 *  RegistryOrder
 *  PoolOrder
 *
 * // management classes
 * BlockPool<T>
 * BlockManagement<T>
 * iManage<T>
 * AbstractManager<T>
 *
 * std::hash<Block<T>>
 * std::formatter<Block<T>>
 */

template<typename T>
struct Block {
    using spointer = std::shared_ptr<T>;
    using OBlock = std::optional<Block>;
    spointer owner = {nullptr};
    spointer head = {nullptr};
    std::align_val_t alignment{};
    std::size_t length = 1;
    OBlock split_exactly(std::size_t idx);
    OBlock split_at(std::size_t idx);
    bool contains(void* p) const;
    std::vector<spointer> vector(std::function<void(T*)> d) {
        std::vector<spointer> ret;
        ret.reserve(length);
        for(std::size_t i = 0; i < length; ++i) {
            ret.push_back(spointer(head.get()+i, d));
        }
        return ret;
    }
};
template<typename T>
using OBlock = typename Block<T>::OBlock;

template<typename T>
typename Block<T>::OBlock Block<T>::split_exactly(std::size_t idx) {
    using namespace CE;
    if (length == 0 || idx >= length - 1 || !head.get()) {
        return std::nullopt;
    }
    T* p = ptr::add_offset<T>(head.get(), idx);
    Block R{
        owner,
        std::shared_ptr<T>(owner, p),
        ptr::calculate_alignment(p),
        length - idx
    };
    length = idx;
    return std::make_optional(R);
}

template<typename T>
typename Block<T>::OBlock Block<T>::split_at(std::size_t idx) {
    using namespace CE;
    constexpr auto av64 = std::align_val_t{64};
    constexpr auto av128 = std::align_val_t{128};
    const auto av1 = alignment >= av128 ? alignment : av128;
    const auto av2 = (alignment > av64 && alignment < av128) ? alignment : av64;
    const auto av3 = alignment <= av64 ? alignment : av64;

    const auto cidx = idx;
    idx = ptr::align_offset(head.get(), cidx, av1);
    if (idx >= length - 1) {
        idx = ptr::align_offset(head.get(), cidx, av2);
        if (idx >= length - 1) {
            idx = ptr::align_offset(head.get(), cidx, av3);
            if (idx >= length - 1) {
                return std::nullopt;
            }
        }
    }
    return split_exactly(idx);
}

template<typename T>
bool Block<T>::contains(void* p) const {
    using namespace CE;
    return ptr::is_in_range(head.get(), ptr::add_offset<T>(head.get(), length), p);
}

template<typename T, typename U>
bool operator==(const Block<U>& lhs, const Block<T>& rhs) {
    return static_cast<void*>(lhs.owner.get()) == static_cast<void*>(rhs.owner.get())
    && static_cast<void*>(lhs.head.get()) == static_cast<void*>(rhs.head.get())
    && lhs.alignment == rhs.alignment
    && lhs.length == rhs.length;
}

namespace BlockHelpers {
    using namespace CE;
    template<typename T>
    void swap(Block<T>& A, Block<T>& B) noexcept {
        const Block<T> C = A;
        A = B;
        B = C;
    }

    template<typename T>
    bool is_contiguous(Block<T> A, Block<T> B) {
        if (A == B) {
            return false;
        }
        if (A.owner != B.owner) {
            return false;
        }
        if (A.head.get() > B.head.get()) {
            swap(A, B);
        }
        if constexpr (std::is_same_v<T, void>) {
            return ptr::add_offset<void>(A.head.get(), A.length) == B.head.get();
        } else {
            return ptr::add_offset<T>(A.head.get(), sizeof(T) * A.length) == B.head.get();
        }
    }
}

namespace compare {
    // head pointers in ascending order
    template<typename T>
    struct HeadOrder {
        bool operator()(const Block<T>& lhs, const Block<T>& rhs) const {
            if (lhs.head.get() != rhs.head.get()) {
                return lhs.head.get() < rhs.head.get();
            }
            if (lhs.length != rhs.length) {
                return lhs.length < rhs.length;
            }
            return lhs.alignment < rhs.alignment;
        }
    };
    // sub-block < master-block
    template<typename T>
    struct RegistryOrder {
        bool operator()(const Block<T>& a, const Block<T>& b) const {
            if (a.owner.get() != b.owner.get()) {
                return a.owner.get() < b.owner.get();
            }
            return a.head.get() > b.head.get();
        }
    };
    // most likely to fill request > least likely to fill request
    template<typename T>
    struct PoolOrder {
        bool operator()(const Block<T>& lhs, const Block<T>& rhs) const {
            if (lhs.alignment != rhs.alignment) {
                return lhs.alignment > rhs.alignment;
            }
            if (lhs.length != rhs.length) {
                return lhs.length > rhs.length;
            }
            if (lhs.head.get() != rhs.head.get()) {
                return lhs.head.get() > rhs.head.get();
            }
            return lhs.owner.get() > rhs.owner.get();
        }
    };

    /* don't remove the "useless" checks
     * set<Block<T>, cmp>::contains() depends on them to properly check equality
     */
}

template<typename T>
struct BlockManagement {
    using clock = std::chrono::steady_clock;
    using tpoint = std::chrono::time_point<clock>;
    // the management (i.e. data structures)
    static std::tuple<std::shared_mutex, std::set<Block<T>, compare::PoolOrder<T>>> pool; // order allocations according to length and alignment
    static std::tuple<std::shared_mutex, std::set<Block<T>, compare::HeadOrder<T>>> sections; // Blocks in order of address location
    static std::tuple<std::shared_mutex, std::set<Block<T>, compare::RegistryOrder<T>>> registry; // heap allocations in order of address location
    static std::tuple<std::shared_mutex, std::unordered_map<Block<T>, tpoint>> stale; // map unique blocks to time stamps
    static std::tuple<std::shared_mutex, std::set<Block<T>, compare::RegistryOrder<T>>> release; // unordered
};

template<typename T>
std::tuple<std::shared_mutex, std::set<Block<T>, compare::PoolOrder<T>>> BlockManagement<T>::pool;
template<typename T>
std::tuple<std::shared_mutex, std::set<Block<T>, compare::HeadOrder<T>>> BlockManagement<T>::sections;
template<typename T>
std::tuple<std::shared_mutex, std::set<Block<T>, compare::RegistryOrder<T>>> BlockManagement<T>::registry;
template<typename T>
std::tuple<std::shared_mutex, std::unordered_map<Block<T>, typename BlockManagement<T>::tpoint>> BlockManagement<T>::stale;
template<typename T>
std::tuple<std::shared_mutex, std::set<Block<T>, compare::RegistryOrder<T>>> BlockManagement<T>::release;

template<typename T>
struct iManage {
    friend class Test_iManage;
protected:
    virtual void record_new(Block<T>) = 0;
    virtual void mark_stale(Block<T>) = 0;
    virtual OBlock<T> merge_into_pool(Block<T>) = 0;
    virtual OBlock<T> find_section(T*) = 0;
    virtual OBlock<T> find_owner(T*) = 0;
    virtual OBlock<T> fill_request(std::size_t) = 0;
public:
    virtual ~iManage() = default;
    virtual void cull(std::chrono::minutes age) = 0;
    virtual void release_culled() = 0;
};

template<typename T>
struct AbstractManager : BlockManagement<T>, iManage<T> {
    AbstractManager() = default;
    ~AbstractManager() override = default;
    using clock = typename BlockManagement<T>::clock;
    using tpoint = typename BlockManagement<T>::tpoint;
    void cull(std::chrono::minutes age) override {
        static tpoint last;
        // This was run early if not even a minute has passed.
        if (const tpoint now = clock::now(); mcast(now - last) >= std::chrono::minutes(1)) {
            //CELog::info("Monitoring Block allocations.");
            auto &stale_memory = std::get<1>(this->stale);
            last = now;
            std::shared_lock lock(std::get<0>(this->stale));
            for (auto &[a,then] : stale_memory) {
                auto elapsed = mcast(now - then);
                if (elapsed >= age) {
                    std::unique_lock ulock(std::get<0>(this->release));
                    std::get<1>(this->release).emplace(a);
                }
            }
            lock.unlock();
            std::unique_lock ulock(std::get<0>(this->stale));
            for (auto &[a,then] : stale_memory) {
                stale_memory.erase(a);
            }
        }
    }
    void release_culled() override {
        std::unique_lock lock(std::get<0>(this->release));
        if (std::get<1>(this->release).empty()) {
            return;
        }
        auto &memory_to_release = std::get<1>(this->release);
        auto iter = memory_to_release.begin();
        do {
            erase(*iter, this->registry, this->pool, this->stale, this->release);
            iter = memory_to_release.begin();
        } while(iter != memory_to_release.end());
    }
protected:
    template<typename Tuple>
    static std::shared_mutex& get_mutex(Tuple& tuple) {
        return std::get<0>(tuple);
    }
    template<typename... Tuples>
    static void read_lock(Tuples&... tuples) {
        // Lambda to lock each shared mutex in shared mode
        auto lock_shared_mutex = [](auto& tuple) {
            std::get<0>(tuple).lock_shared();
        };

        // Apply the lambda to each tuple
        (lock_shared_mutex(tuples), ...);
    }
    template<typename... Tuples>
    static void read_unlock(Tuples&... tuples) {
        // Lambda to lock each shared mutex in shared mode
        auto lock_shared_mutex = [](auto& tuple) {
            std::get<0>(tuple).unlock_shared();
        };

        // Apply the lambda to each tuple
        (lock_shared_mutex(tuples), ...);
    }
    template<typename... Tuples>
    static void emplace(Block<T> b, Tuples&... tuples) {
        // Lock each mutex associated with its set
        auto lock_set = [](Block<T> b, auto&& pair) {
            std::unique_lock<std::shared_mutex> lock(std::get<0>(pair));
            std::get<1>(pair).emplace(b);
        };

        // Apply the lock and emplace operation to each pair
        (lock_set(b, std::forward<Tuples>(tuples)), ...);
    }
    template<typename... Tuples>
    static void erase(Block<T> b, Tuples&... tuples) {
        // Lock each mutex associated with its set
        auto lock_set = [](Block<T> b, auto&& pair) {
            std::unique_lock<std::remove_reference_t<decltype(std::get<0>(pair))>> lock(std::get<0>(pair));
            std::get<1>(pair).erase(b);
        };

        // Apply the lock and emplace operation to each pair
        (lock_set(b, std::forward<Tuples>(tuples)), ...);
    }
    template<typename... Tuples>
    static bool contains(Block<T> b, Tuples&... tuples) {
        // Lock each mutex associated with its set
        auto share_set = [](Block<T> b, auto&& pair) {
            std::shared_lock<std::shared_mutex> lock(std::get<0>(pair));
            return std::get<1>(pair).contains(b);
        };

        // Apply the lock and emplace operation to each pair
        return (share_set(b, std::forward<Tuples>(tuples)) && ...);
    }
    template<typename Tuple>
    OBlock<T> adjacent_right(Block<T> block, Tuple &tuple) {
        std::shared_lock<std::shared_mutex> lock(std::get<0>(tuple));
        auto &set = std::get<1>(tuple);
        auto iter = set.lower_bound(block);
        if (iter != set.end()) {
            if (*iter == block) {
                iter = std::next(iter);
                if (iter == set.end()) {
                    return {std::nullopt};
                }
            }
            return {*iter};
        }
        return {std::nullopt};
    }
    template<typename Tuple>
    OBlock<T> adjacent_left(Block<T> block, Tuple &tuple) {
        std::shared_lock<std::shared_mutex> lock(std::get<0>(tuple));
        auto &set = std::get<1>(tuple);
        auto iter = set.lower_bound(block);
        if (iter != set.begin() && *iter == block) {
            iter = std::prev(iter);
        }
        if (iter == set.end()) {
            return {std::nullopt};
        }
        return {*iter};
    }
    // iManage interface
    /////////////////////
    void record_new(Block<T> block) override {
        emplace(block, this->registry);
    }
    void mark_stale(Block<T> block) override {
        if (!contains(block, this->registry)) {
            return;
        }
        auto now = clock::now();
        std::unique_lock<std::shared_mutex> lock(std::get<0>(this->stale));
        std::get<1>(this->stale).emplace(block, now);
    }
    OBlock<T> merge_into_pool(Block<T> block) override {
        auto left = adjacent_left(block, this->sections);
        auto right = adjacent_right(block, this->sections);
        if (left.has_value() || right.has_value()) {
            erase(block, this->sections);
        }
        // merge properties
        if (left.has_value() && BlockHelpers::is_contiguous(*left, block) && std::get<1>(this->pool).contains(*left)) {
            erase(*left, this->pool, this->sections);
            block.head = left->head;
            block.alignment = left->alignment;
            block.length += left->length;
        }
        if (right.has_value() && BlockHelpers::is_contiguous(block, *right) && std::get<1>(this->pool).contains(*right)) {
            erase(*right, this->pool, this->sections);
            block.length += right->length;
        }
        // record block
        if (left.has_value() || right.has_value()) {
            if (contains(block, this->registry)) {
                mark_stale(block);
            } else {
                emplace(block, this->sections);
            }
        }
        emplace(block, this->pool);
        return {block};
    }
    OBlock<T> find_section(T* ptr) override {
        std::shared_lock<std::shared_mutex> lock(std::get<0>(this->sections));
        const auto av = CE::ptr::calculate_alignment(ptr);
        Block<T> faux_block {nullptr, std::shared_ptr<T>(ptr, [](const T* p){}), av, 0};
        // finds
        if (auto adjacent = adjacent_left(faux_block, this->sections); adjacent.has_value() && adjacent->contains(ptr)) {
            return adjacent;
        }
        auto &sec = std::get<1>(this->sections);
        if (auto lb = sec.lower_bound(faux_block); lb != sec.end() && lb->contains(ptr)) {
            return *lb;
        }
        return {std::nullopt};
    }
    OBlock<T> find_owner(T* ptr) override {
        std::shared_lock<std::shared_mutex> lock(std::get<0>(this->registry));
        auto fake = std::shared_ptr<T>(ptr, [](const T* p){});
        Block<T> faux_block {fake, fake, {}, 0};
        if (contains(faux_block, this->registry)) {
            auto &reg = std::get<1>(this->registry);
            auto iter = reg.find(faux_block);
            return {*iter};
        }
        if (auto adjacent = adjacent_left(faux_block, this->registry); adjacent.has_value() && adjacent->contains(ptr)) {
            return adjacent;
        }
        return {std::nullopt};
    }
    OBlock<T> fill_request(std::size_t N) override {
        std::unique_lock lock(std::get<0>(this->pool));
        auto av = []() {
            if constexpr (std::is_same_v<T,void>) {
                return std::align_val_t{64};
            } else {
                return std::align_val_t{alignof(T)};
            }
        };
        constexpr auto Talignval = av();
        auto &pool_set = std::get<1>(this->pool);
        // our pool Blocks are sorted alignment, length, head, owner all in ascending order
        // search all iter with large enough alignment
        for (auto iter = pool_set.begin(); iter != pool_set.end() && iter->alignment >= Talignval; ++iter) {
            // if it also possesses the length required, we can return that block
            if (iter->length >= N) {
                OBlock<T> ob {*iter};
                pool_set.erase(iter);
                return ob;
            }
        }
        return {std::nullopt};
    }
};

namespace std {
    template<typename T>
    struct hash<Block<T>> {
        std::size_t operator()(const Block<T>& k) const noexcept {
            constexpr std::hash<std::shared_ptr<T>> hash_ptr;
            constexpr std::hash<std::align_val_t> hash_align;
            constexpr std::hash<size_t> hash_szt;
            std::size_t combined_hash = hash_ptr(k.owner);
            combined_hash ^= hash_ptr(k.head) + 0x9e3779b9 + (combined_hash << 6) + (combined_hash >> 2);
            combined_hash ^= hash_align(k.alignment) + 0x9e3779b9 + (combined_hash << 6) + (combined_hash >> 2);
            combined_hash ^= hash_szt(k.length) + 0x9e3779b9 + (combined_hash << 6) + (combined_hash >> 2);
            return combined_hash;
        }
    };

    template <typename T>
    struct formatter<Block<T>> {
        constexpr auto parse(std::format_parse_context& ctx) {
            return ctx.begin();
        }

        auto format(const Block<T>& a, std::format_context& ctx) const {
            return std::format_to(ctx.out(), "{} [alignment: {}, length: {}]",
                static_cast<void*>(a.head.get()),
                static_cast<std::size_t>(a.alignment),
                a.length);
        }
    };
}
#endif
