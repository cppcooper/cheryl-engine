#pragma once
#define MTRACE() UTRACE(CE::memlog)
#define MDEBUG() UDEBUG(CE::memlog)
#define MINFO() UINFO(CE::memlog)
#define MWARN() UWARN(CE::memlog)
#define MERROR() UERROR(CE::memlog)
#define MFATAL() UFATAL(CE::memlog)
#include <cemath.h>
#include <internals/compile-time-logging.hpp>
#undef CTWriteMask
#define CTWriteMask 0
#include <core/logging/logger.h>
#include <functional>
#include <algorithm>
#include <memory>
#include <chrono>
#include <set>
#include <vector>
#include <tuple>
#include <shared_mutex>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <type_traits>

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

/* Block<T>
 * A contiguous range within one backing allocation. owner keeps that allocation
 * alive; head may point into it after a split. length counts T objects (bytes for void).
 * Splitting produces ranges with the same owner without transferring storage.
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
    if (idx == 0 || idx >= length || !head.get()) {
        return std::nullopt;
    }
    T* p;
    if constexpr (std::is_void_v<T>) {
        p = ptr::add_offset<T>(head.get(), idx);
    } else {
        p = head.get() + idx;
    }
    auto av = ptr::calculate_alignment(p);
    if(av == std::align_val_t{1}) {
        MWARN() << "We are going to have a 1 alignment memory pooling issue. We at least need to resplit";
    }
    Block R{
        owner,
        std::shared_ptr<T>(owner, p),
        av,
        length - idx
    };
    length = idx;
    return std::make_optional(R);
}

template<typename T>
typename Block<T>::OBlock Block<T>::split_at(std::size_t idx) {
    using namespace CE;
    if constexpr (!std::is_void_v<T>) {
        return split_exactly(idx);
    } else {
        constexpr auto av64 = std::align_val_t{64};
        constexpr auto av128 = std::align_val_t{128};
        const auto av1 = alignment >= av128 ? alignment : av128;
        const auto av2 = (alignment > av64 && alignment < av128) ? alignment : av64;
        const auto av3 = alignment <= av64 ? alignment : av64;

        const auto cidx = idx;
        idx = ptr::align_offset(head.get(), cidx, av1);
        if (idx >= length) {
            idx = ptr::align_offset(head.get(), cidx, av2);
            if (idx >= length) {
                idx = ptr::align_offset(head.get(), cidx, av3);
                if (idx >= length) {
                    return std::nullopt;
                }
            }
        }
        return split_exactly(idx);
    }
}

template<typename T>
bool Block<T>::contains(void* p) const {
    using namespace CE;
    constexpr auto element_size = [] {
        if constexpr (std::is_void_v<T>) return std::size_t{1};
        else return sizeof(T);
    }();
    return ptr::is_in_range(reinterpret_cast<std::uintptr_t>(head.get()),
                            ptr::offset_address(head.get(), length * element_size),
                            reinterpret_cast<std::uintptr_t>(p));
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

/* BlockManagement<T>
 * Bookkeeping shared by managers of the same T: registry owns full allocations,
 * sections partitions split allocations, and pool records reusable ranges.
 * stale timestamps complete free owners; release queues them for culling.
 * A retained State keeps this bookkeeping alive after a manager facade dies.
 */
template<typename T>
struct BlockManagement {
    using clock = std::chrono::steady_clock;
    using tpoint = std::chrono::time_point<clock>;
    struct State {
        std::tuple<std::shared_mutex, std::set<Block<T>, compare::PoolOrder<T>>> pool;
        std::tuple<std::shared_mutex, std::set<Block<T>, compare::HeadOrder<T>>> sections;
        std::tuple<std::shared_mutex, std::set<Block<T>, compare::RegistryOrder<T>>> registry;
        std::tuple<std::shared_mutex, std::unordered_map<Block<T>, tpoint>> stale;
        std::tuple<std::shared_mutex, std::set<Block<T>, compare::RegistryOrder<T>>> release;
    };

    static std::shared_ptr<State> shared_state() {
        static auto state = std::make_shared<State>();
        return state;
    }

    // Keep the established access points while making the storage's lifetime
    // independent of the singleton facades that use it.
    inline static auto& pool = shared_state()->pool;
    inline static auto& sections = shared_state()->sections;
    inline static auto& registry = shared_state()->registry;
    inline static auto& stale = shared_state()->stale;
    inline static auto& release = shared_state()->release;

protected:
    std::shared_ptr<State> state_ = shared_state();
};

template<typename T>
struct iManage {
    friend class Test_iManage;
protected:
    virtual void record_new(Block<T>) = 0;
    virtual void mark_stale(Block<T>) = 0;
    virtual OBlock<T> merge_into_pool(Block<T>) = 0;
    virtual OBlock<T> find_section(T*) = 0;
    virtual OBlock<T> find_owner(T*) = 0;
    virtual OBlock<T> fill_request(std::size_t, std::align_val_t minimum_alignment = std::align_val_t{0}) = 0;
public:
    virtual ~iManage() = default;
    virtual void cull(std::chrono::minutes age) = 0;
    virtual void release_culled() = 0;
};

/* AbstractManager<T>
 * Implements block lookup, merging, and culling over BlockManagement<T>'s
 * shared sets. Derived managers decide how blocks are acquired and returned.
 */
template<typename T>
struct AbstractManager : BlockManagement<T>, iManage<T> {
    AbstractManager() = default;
    ~AbstractManager() override = default;
    using clock = typename BlockManagement<T>::clock;
    using tpoint = typename BlockManagement<T>::tpoint;
    void cull(std::chrono::minutes age) override {
        const auto now = clock::now();
        std::scoped_lock lock(std::get<0>(this->stale), std::get<0>(this->release));
        auto& stale_memory = std::get<1>(this->stale);
        auto& pending = std::get<1>(this->release);
        for (auto it = stale_memory.begin(); it != stale_memory.end();) {
            if (now - it->second >= age) {
                pending.emplace(it->first);
                it = stale_memory.erase(it);
            } else {
                ++it;
            }
        }
    }
    void release_culled() override {
        std::scoped_lock lock(std::get<0>(this->registry), std::get<0>(this->sections),
                              std::get<0>(this->pool), std::get<0>(this->stale),
                              std::get<0>(this->release));
        auto& registry = std::get<1>(this->registry);
        auto& sections = std::get<1>(this->sections);
        auto& pool = std::get<1>(this->pool);
        auto& stale = std::get<1>(this->stale);
        auto& pending = std::get<1>(this->release);
        for (const auto& block : pending) {
            const auto owner = registry.find(block);
            const auto available = pool.find(block);
            if (owner == registry.end() || *owner != block ||
                available == pool.end() || *available != block) {
                continue;
            }
            bool has_active_sections = false;
            for (const auto& section : sections) {
                if (section.owner.get() == block.owner.get()) {
                    has_active_sections = true;
                    break;
                }
            }
            if (!has_active_sections) {
                pool.erase(available);
                registry.erase(owner);
                stale.erase(block);
            }
        }
        pending.clear();
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
            const auto& set = std::get<1>(pair);
            const auto found = set.find(b);
            return found != set.end() && *found == b;
        };

        // Apply the lock and emplace operation to each pair
        return (share_set(b, std::forward<Tuples>(tuples)) && ...);
    }
    template<typename Tuple>
    OBlock<T> search_right(Block<T> block, Tuple &tuple) {
        std::shared_lock<std::shared_mutex> lock(std::get<0>(tuple));
        auto &set = std::get<1>(tuple);
        MTRACE() << "Searching to the right from " << block;
        auto iter = set.lower_bound(block);
        if (iter != set.end()) {
            MTRACE() << "lower_bound: " << *iter;
        } else {
            MTRACE() << "lower_bound: end()";
        }
        while (iter != set.end() && block.owner == iter->owner && iter->head.get() <= block.head.get()) {
            iter = std::next(iter);
            if (iter != set.end()) MTRACE() << "next: " << *iter;
        }
        if (iter != set.end() && block.owner == iter->owner && iter->head.get() > block.head.get()) {
            return {*iter};
        }
        MWARN() << "search condition not found right, returning nullopt";
        return {std::nullopt};
    }
    template<typename Tuple>
    OBlock<T> search_left(Block<T> block, Tuple &tuple) {
        std::shared_lock<std::shared_mutex> lock(std::get<0>(tuple));
        auto &set = std::get<1>(tuple);
        MTRACE() << "Searching to the left from " << block;
        auto iter = set.lower_bound(block);
        while (iter != set.begin()) {
            --iter;
            if (iter->head.get() < block.head.get()) {
                if (block.owner == iter->owner) return {*iter};
                break;
            }
        }
        MWARN() << "search condition not found left, returning nullopt";
        return {std::nullopt};
    }
    template<typename Tuple>
    OBlock<T> contiguous_right(Block<T> block, Tuple &tuple) {
        MDEBUG() << "Looking for contiguous right..";
        auto ob = search_right(block, tuple);
        if (ob.has_value() && BlockHelpers::is_contiguous(block, *ob)) {
            MDEBUG() << "contiguous right found: " << *ob;
            return {*ob};
        }
        MWARN() << "not contiguous, returning nullopt";
        return {std::nullopt};
    }
    template<typename Tuple>
    OBlock<T> contiguous_left(Block<T> block, Tuple &tuple) {
        MDEBUG() << "Looking for contiguous left..";
        auto ob = search_left(block, tuple);
        if (ob.has_value() && BlockHelpers::is_contiguous(block, *ob)) {
            MDEBUG() << "contiguous left found: " << *ob;
            return {*ob};
        }
        MWARN() << "not contiguous, returning nullopt";
        return {std::nullopt};
    }
    template<typename Tuple>
    OBlock<T> adjacent_right(Block<T> block, Tuple &tuple) {
        MDEBUG() << "Looking for adjacent right..";
        std::shared_lock<std::shared_mutex> lock(std::get<0>(tuple));
        auto &set = std::get<1>(tuple);
        auto iter = set.lower_bound(block);
        if (iter != set.end()) {
            if (*iter == block) {
                iter = std::next(iter);
            }
        }
        if (iter != set.end()) {
            MDEBUG() << "adjacent right found: " << *iter;
            return {*iter};
        }
        MDEBUG() << "no adjacent, returning nullopt";
        return {std::nullopt};
    }
    // Returns the predecessor in the set's ordering, which may differ from address order.
    template<typename Tuple>
    OBlock<T> adjacent_left(Block<T> block, Tuple &tuple) {
        MDEBUG() << "Looking for adjacent left..";
        std::shared_lock<std::shared_mutex> lock(std::get<0>(tuple));
        auto &set = std::get<1>(tuple);
        const auto next = set.lower_bound(block);
        if (next == set.begin()) {
            MDEBUG() << "no adjacent, returning nullopt";
            return {std::nullopt};
        }
        const auto previous = std::prev(next);
        MDEBUG() << "adjacent left found: " << *previous;
        return {*previous};
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
        MTRACE() << "Merging " << block << " into pool.";
        const auto og = block;
        auto left = contiguous_left(block, this->sections);
        if (left.has_value()) {
            if (contains(*left, this->pool)) {
                erase(*left, this->pool, this->sections);
            } else {
                left = {std::nullopt};
            }
        }
        auto right = contiguous_right(block, this->sections);
        if (right.has_value()) {
            if (contains(*right, this->pool)) {
                erase(*right, this->pool, this->sections);
            } else {
                right = {std::nullopt};
            }
        }
        // merge properties
        if (left.has_value()) {
            block.head = left->head;
            block.alignment = left->alignment;
            block.length += left->length;
            MINFO() << "contiguous left merged.";
        }
        if (right.has_value()) {
            block.length += right->length;
            MINFO() << "contiguous right merged.";
        }

        // RegistryOrder identifies an allocation by owner and head, so find()
        // also matches a shorter section at the allocation's starting address.
        // Only the complete owner block may leave sections and become stale.
        bool is_owner_block = false;
        {
            std::shared_lock lock(std::get<0>(this->registry));
            const auto& reg = std::get<1>(this->registry);
            const auto owner = reg.find(block);
            is_owner_block = owner != reg.end() && *owner == block;
        }
        if (is_owner_block) {
            erase(og, this->sections);
            MTRACE() << "Merged block is in the registry. Marking stale.";
            mark_stale(block);
            emplace(block, this->pool);
        } else if (og != block) {
            erase(og, this->sections);
            emplace(block, this->pool, this->sections);
        } else {
            emplace(block, this->pool);
        }
        MDEBUG() << "Merged " << block << " into pool.";
        return {block};
    }
    OBlock<T> find_section(T* ptr) override {
        const auto av = CE::ptr::calculate_alignment(ptr);
        Block<T> faux_block {nullptr, std::shared_ptr<T>(ptr, [](const T* p){}), av, 0};
        if (auto left = adjacent_left(faux_block, this->sections); left.has_value() && left->contains(ptr)) {
            return left;
        }
        std::shared_lock<std::shared_mutex> lock(std::get<0>(this->sections));
        auto &sec = std::get<1>(this->sections);
        const auto next = sec.lower_bound(faux_block);
        if (next != sec.end() && next->contains(ptr)) {
            return *next;
        }
        return {std::nullopt};
    }
    OBlock<T> find_owner(T* ptr) override {
        std::shared_lock<std::shared_mutex> lock(std::get<0>(this->registry));
        auto fake = std::shared_ptr<T>(ptr, [](const T* p){});
        Block<T> faux_block {fake, fake, {}, 0};
        auto &reg = std::get<1>(this->registry);
        auto next = reg.upper_bound(faux_block);
        if (next != reg.begin()) {
            const auto owner = std::prev(next);
            if (owner->contains(ptr)) {
                return *owner;
            }
        }
        return {std::nullopt};
    }
    OBlock<T> fill_request(std::size_t N, std::align_val_t minimum_alignment = std::align_val_t{0}) override {
        std::unique_lock lock(std::get<0>(this->pool));
        auto av = []() {
            if constexpr (std::is_same_v<T,void>) {
                return std::align_val_t{64};
            } else {
                return std::align_val_t{alignof(T)};
            }
        };
        const auto Talignval = std::max(av(), minimum_alignment);
        auto &pool_set = std::get<1>(this->pool);
        MINFO() << "Pool received a request for " << N << " slices("<< ctti::nameof<T>() <<") of " << Talignval << " aligned memory.";
        // our pool Blocks are sorted alignment, length, head, owner all in ascending order
        // search all iter with large enough alignment
        for (auto iter = pool_set.begin(); iter != pool_set.end() && iter->alignment >= Talignval; ++iter) {
            // if it also possesses the length required, we can return that block
            if (iter->length >= N) {
                OBlock<T> ob {*iter};
                pool_set.erase(iter);
                MTRACE() << "result: " << ob.value();
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

    template <typename S, typename T>
    S& operator<<(S& os, const Block<T>& block) {
        //uint32_t owner = std::get<uint32_t>(CE::ptr::pointer_to_hash(block.owner.get(),4));
        auto owner = static_cast<void*>(block.owner.get());
        os << std::format("{} [alignment: {}, length: {}({}), owner: {}]",
                          static_cast<void*>(block.head.get()),
                          static_cast<std::size_t>(block.alignment),
                          (void*)block.length, block.length, owner);
        return os;
    }

    template <typename S>
    S& operator<<(S& os, const std::align_val_t alignment) {
        os << std::format("{}", static_cast<std::size_t>(alignment));
        return os;
    }
}
