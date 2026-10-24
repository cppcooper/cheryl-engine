#pragma once
/** @def MTRACE()
 * @brief Legacy TRACE logging shorthand for the memory category; follows the underlying logging macro policy.
 */
#define MTRACE() UTRACE(::CE::Logging::Categories::memory)
/** @def MDEBUG()
 * @brief Legacy DEBUG logging shorthand for the memory category; follows the underlying logging macro policy.
 */
#define MDEBUG() UDEBUG(::CE::Logging::Categories::memory)
/** @def MINFO()
 * @brief Legacy INFO logging shorthand for the memory category; follows the underlying logging macro policy.
 */
#define MINFO() UINFO(::CE::Logging::Categories::memory)
/** @def MWARN()
 * @brief Legacy WARN logging shorthand for the memory category; follows the underlying logging macro policy.
 */
#define MWARN() UWARN(::CE::Logging::Categories::memory)
/** @def MERROR()
 * @brief Legacy ERROR logging shorthand for the memory category; follows the underlying logging macro policy.
 */
#define MERROR() UERROR(::CE::Logging::Categories::memory)
/** @def MFATAL()
 * @brief Legacy FATAL logging shorthand for the memory category; follows the underlying logging macro policy.
 */
#define MFATAL() UFATAL(::CE::Logging::Categories::memory)
#include <cemath.h>
#include <internals/compile-time-logging.hpp>
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
#include <memory_resource>
#include <cstdint>
#include <type_traits>

class Test_iManage;

namespace CE::Memory {

    /* Header: block.h
 *
 * includes
 * CE::Memory::Block<T>
 * CE::Memory::OptionalBlock<T>
 * operator==
 *
 * // inline functions
 * swap(CE::Memory::Block<T>&,CE::Memory::Block<T>&)
 * is_contiguous(CE::Memory::Block<T>,CE::Memory::Block<T>)
 *
 * // sorting CE::Memory::Block<T>
 * namespace CE::Memory::Ordering
 *  CE::Memory::Ordering::ByteHeadOrder
 *  CE::Memory::Ordering::ByteRegistryOrder
 *  CE::Memory::Ordering::BytePoolOrder
 *
 * // management classes
 * CE::Memory::BlockManagement<T>
 * CE::Memory::BlockManagerInterface<T>
 * CE::Memory::AbstractBlockManager<T>
 *
 * std::hash<CE::Memory::Block<T>>
 * std::formatter<CE::Memory::Block<T>>
 */

    /**
 * A contiguous range within one backing allocation. owner keeps that allocation
 * alive; head may point into it after a split. length counts T objects (bytes for void).
 * Splitting produces ranges with the same owner without transferring storage.
 * Range mutation requires caller synchronization and valid representable backing
 * bounds. Compound registry/pool transitions use CE::Memory::BlockTransactions instead.
 */
    template <typename T> struct Block {
        /** Retained pointer; owner and head may alias different positions in one backing allocation. */
        using spointer = std::shared_ptr<T>;
        /** Optional range result; no value means no legal split/match. */
        using OBlock = std::optional<Block>;
        spointer owner = {nullptr};   ///< Backing-allocation root owner shared by every split range.
        spointer head = {nullptr};    ///< First element/byte, normally an alias retaining owner.
        std::align_val_t alignment{}; ///< Known alignment of head, expressed in bytes; valid splitting requires nonzero bounds.
        std::size_t length = 1;       ///< Count of T objects, or bytes when T is void; not a constructed-object count.
        OBlock split_exactly(std::size_t idx);
        OBlock split_at(std::size_t idx);
        bool contains(void* p) const;

        // Independent element control blocks, not aliases of owner. The supplied
        // deleter must retain/release backing storage and manage constructed lifetimes;
        // this helper neither constructs elements nor keeps the CE::Memory::Block's owner itself.
        std::vector<spointer> vector(std::function<void(T*)> d) {
            std::vector<spointer> ret;
            ret.reserve(length);
            for (std::size_t i = 0; i < length; ++i) {
                ret.push_back(spointer(head.get() + i, d));
            }
            return ret;
        }
    };

    /** Optional typed range; copying a present value retains its backing owner. */
    template <typename T> using OptionalBlock = typename Block<T>::OBlock;

    /** Split this range at an exact element/byte offset, retaining common backing ownership.
     * @param idx Positive offset strictly below length; null head or invalid offset returns nullopt unchanged.
     * @return Right remainder; this Block becomes the left range. No allocation bytes
     * move, no T objects are constructed/destroyed, and no pool records are updated.
     * Caller supplies valid representable backing bounds and excludes concurrent edits.
     */
    template <typename T> typename Block<T>::OBlock Block<T>::split_exactly(std::size_t idx) {
        using namespace CE;
        // Keep the left range in place and give the right range an alias to the
        // same owner; splitting bookkeeping must not free or copy the allocation.
        if (idx == 0 || idx >= length || !head.get()) {
            return std::nullopt;
        }
        T* p;
        if constexpr (std::is_void_v<T>) {
            p = ::CE::Memory::Pointers::add_offset<T>(head.get(), idx);
        } else {
            p = head.get() + idx;
        }
        auto av = ::CE::Memory::Pointers::calculate_alignment(p);
        // Byte alignment is a valid remainder. This operation can run under memory
        // transaction locks, so diagnostics belong to an unlocked caller snapshot.
        Block R{owner, std::shared_ptr<T>(owner, p), av, length - idx};
        length = idx;
        return std::make_optional(R);
    }

    /** Split typed ranges exactly, or seek a preferred aligned right tail for void-byte ranges.
     * @param idx Requested element/byte offset; the byte left range may absorb alignment padding.
     * @return Right alias or nullopt when no legal nonempty remainder exists.
     * No backing storage/object lifetime or pool bookkeeping changes beyond this
     * Block's local length. Valid nonzero alignment/representable bounds and caller
     * synchronization are prerequisites.
     */
    template <typename T> typename Block<T>::OBlock Block<T>::split_at(std::size_t idx) {
        using namespace CE;
        if constexpr (!std::is_void_v<T>) {
            return split_exactly(idx);
        } else {
            // Byte blocks seek an aligned start for the right-hand remainder. If the
            // requested alignment leaves no room, retry with progressively smaller bounds.
            // The left range absorbs padding between the requested offset and the split.
            constexpr auto av64 = std::align_val_t{64};
            constexpr auto av128 = std::align_val_t{128};
            const auto av1 = alignment >= av128 ? alignment : av128;
            const auto av2 = (alignment > av64 && alignment < av128) ? alignment : av64;
            const auto av3 = alignment <= av64 ? alignment : av64;

            const auto cidx = idx;
            idx = ::CE::Memory::Pointers::align_offset(head.get(), cidx, av1);
            if (idx >= length) {
                idx = ::CE::Memory::Pointers::align_offset(head.get(), cidx, av2);
                if (idx >= length) {
                    idx = ::CE::Memory::Pointers::align_offset(head.get(), cidx, av3);
                    if (idx >= length) {
                        return std::nullopt;
                    }
                }
            }
            return split_exactly(idx);
        }
    }

    /** Test a half-open address interval using length elements (bytes for void).
     * @param p Candidate address, compared numerically with this valid representable range.
     * @return Whether p lies at/after head and before its computed end.
     * This does not prove a live T object, allocation validity, checkout status or
     * thread safety. Caller ensures length*sizeof(T) and derived end are representable.
     */
    template <typename T> bool Block<T>::contains(void* p) const {
        using namespace CE;
        constexpr auto element_size = [] {
            if constexpr (std::is_void_v<T>)
                return std::size_t{1};
            else
                return sizeof(T);
        }();
        return ::CE::Memory::Pointers::is_in_range(
            reinterpret_cast<std::uintptr_t>(head.get()), ::CE::Memory::Pointers::offset_address(head.get(), length * element_size),
            reinterpret_cast<std::uintptr_t>(p)
        );
    }

    /** Compare owner/head addresses, alignment and length across Block element types.
     * This is metadata equality, not equality of stored objects or shared_ptr control
     * block identity. No allocation, ownership transfer or synchronization occurs.
     */
    template <typename T, typename U> bool operator==(const Block<U>& lhs, const Block<T>& rhs) {
        return static_cast<void*>(lhs.owner.get()) == static_cast<void*>(rhs.owner.get()) &&
               static_cast<void*>(lhs.head.get()) == static_cast<void*>(rhs.head.get()) && lhs.alignment == rhs.alignment &&
               lhs.length == rhs.length;
    }

}

namespace CE::Memory {
    using namespace CE;

    /** Exchange local metadata/shared owners without copying backing bytes or pool records.
     * Callers exclude concurrent access; this operation does not update manager collections.
     */
    template <typename T> void swap(Block<T>& A, Block<T>& B) noexcept {
        const Block<T> C = A;
        A = B;
        B = C;
    }

    /** Test whether distinct same-owner ranges meet exactly at one end, in either order.
     * @return False for equal metadata or different owner addresses; true only for
     * adjoining valid ranges. T lengths count objects, void lengths bytes.
     * Caller provides comparable representable backing intervals; this neither
     * merges ranges nor checks their current pool/constructed-object state.
     */
    template <typename T> bool is_contiguous(Block<T> A, Block<T> B) {
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
            return ::CE::Memory::Pointers::add_offset<void>(A.head.get(), A.length) == B.head.get();
        } else {
            return ::CE::Memory::Pointers::add_offset<T>(A.head.get(), sizeof(T) * A.length) == B.head.get();
        }
    }
}

namespace CE::Memory::Ordering {
    /** Address-first strict ordering, then length/alignment; assumes valid comparable range metadata. */
    template <typename T> struct HeadOrder {
        bool operator()(const ::CE::Memory::Block<T>& lhs, const ::CE::Memory::Block<T>& rhs) const noexcept {
            if (lhs.head.get() != rhs.head.get()) {
                return lhs.head.get() < rhs.head.get();
            }
            if (lhs.length != rhs.length) {
                return lhs.length < rhs.length;
            }
            return lhs.alignment < rhs.alignment;
        }
    };

    /** Backing-owner address first, then descending head address; comparator equivalence is not complete Block equality. */
    template <typename T> struct RegistryOrder {
        bool operator()(const ::CE::Memory::Block<T>& a, const ::CE::Memory::Block<T>& b) const noexcept {
            if (a.owner.get() != b.owner.get()) {
                return a.owner.get() < b.owner.get();
            }
            return a.head.get() > b.head.get();
        }
    };

    /** Reuse ordering by descending alignment, length, head and owner addresses; not an allocation policy itself. */
    template <typename T> struct PoolOrder {
        bool operator()(const ::CE::Memory::Block<T>& lhs, const ::CE::Memory::Block<T>& rhs) const noexcept {
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
     * set<CE::Memory::Block<T>, cmp>::contains() depends on them to properly check equality
     */
}

namespace CE::Memory {

    /**
 * Bookkeeping shared by managers of the same T: registry owns full allocations,
 * sections partitions split allocations, and pool records reusable ranges.
 * stale timestamps complete free owners; release queues them for culling.
 * A retained State keeps this bookkeeping alive after a manager facade dies.
 */
    template <typename T> struct BlockManagement {
        /** Monotonic source for whole-free-owner staleness. */
        using clock = std::chrono::steady_clock;
        /** Timestamp in the bookkeeping clock domain. */
        using tpoint = std::chrono::time_point<clock>;

        /** All pool, partition, and culling records retained for one T specialization. */
        struct State {
            const ::CE::Diagnostics::DomainId domain = ::CE::Diagnostics::next_domain_id(); ///< Shared per-T bookkeeping identity.
            using PoolRecords = std::pmr::set<Block<T>, Ordering::PoolOrder<T>>;
            using SectionRecords = std::pmr::set<Block<T>, Ordering::HeadOrder<T>>;
            using OwnerRecords = std::pmr::set<Block<T>, Ordering::RegistryOrder<T>>;
            using StaleRecords = std::pmr::unordered_map<Block<T>, tpoint>;
            std::tuple<std::shared_mutex, PoolRecords> pool;
            std::tuple<std::shared_mutex, SectionRecords> sections;
            std::tuple<std::shared_mutex, OwnerRecords> registry;
            std::tuple<std::shared_mutex, StaleRecords> stale;
            std::tuple<std::shared_mutex, OwnerRecords> release;
            // A node_handle exposes allocator_type without allocator-aware move
            // construction; wrap it so PMR vector does not inject an allocator.
            template <class Node> struct ReturnNode {
                Node node;
                ReturnNode(Node&& value) noexcept
                : node(std::move(value)) {}
                ReturnNode(ReturnNode&&) = default;
                ReturnNode& operator=(ReturnNode&&) = default;
            };
            // Empty preallocated nodes. No backing owner is retained in a spare.
            // Checkout funds future whole/range returns before publishing storage.
            std::pmr::vector<ReturnNode<typename PoolRecords::node_type>> pool_spares;
            std::pmr::vector<ReturnNode<typename SectionRecords::node_type>> section_spares;
            std::pmr::vector<ReturnNode<typename StaleRecords::node_type>> stale_spares;
            std::size_t return_units = 0;

            // The resource is metadata-only and must outlive this State. Normal
            // Engine uses new_delete_resource explicitly, never global default PMR.
            explicit State(std::pmr::memory_resource* metadata = std::pmr::new_delete_resource())
            : pool(std::allocator_arg, std::pmr::polymorphic_allocator<>(metadata)),
              sections(std::allocator_arg, std::pmr::polymorphic_allocator<>(metadata)),
              registry(std::allocator_arg, std::pmr::polymorphic_allocator<>(metadata)),
              stale(std::allocator_arg, std::pmr::polymorphic_allocator<>(metadata)),
              release(std::allocator_arg, std::pmr::polymorphic_allocator<>(metadata)),
              pool_spares(metadata),
              section_spares(metadata),
              stale_spares(metadata) {}
        };

        static std::shared_ptr<State> shared_state() {
            static auto state = std::make_shared<State>();
            return state;
        }

        // Keep the established access points while making the storage's lifetime
        // independent of the singleton facades that use it.
        inline static auto& pool = shared_state()->pool; ///< Legacy reference into retained shared state; obey its lock/invariants.
        inline static auto& sections =
            shared_state()->sections; ///< Legacy partition reference; compound edits require transactions/quiescence.
        inline static auto& registry = shared_state()->registry; ///< Legacy backing-owner reference; not a separately owned domain.
        inline static auto& stale = shared_state()->stale;       ///< Legacy stale-time map reference protected by its mutex.
        inline static auto& release = shared_state()->release;   ///< Legacy pending-release reference protected by its mutex.

    protected:
        std::shared_ptr<State> state_ = shared_state(); ///< Retained per-T collections independent of a manager facade's lifetime.
    };

    /** Virtual lookup, merge, and culling interface for block managers. */
    template <typename T> struct BlockManagerInterface {
        friend class ::Test_iManage;

    protected:
        virtual void record_new(::CE::Memory::Block<T>) = 0;
        virtual void mark_stale(::CE::Memory::Block<T>) = 0;
        virtual ::CE::Memory::OptionalBlock<T> merge_into_pool(::CE::Memory::Block<T>) = 0;
        virtual ::CE::Memory::OptionalBlock<T> find_section(T*) = 0;
        virtual ::CE::Memory::OptionalBlock<T> find_owner(T*) = 0;
        virtual ::CE::Memory::OptionalBlock<T> fill_request(std::size_t, std::align_val_t minimum_alignment = std::align_val_t{0}) = 0;

    public:
        virtual ~BlockManagerInterface() = default;
        virtual void cull(std::chrono::minutes age) = 0;
        virtual void release_culled() = 0;
    };

    template <typename T> struct BlockTransactions;

    /**
 * Implements block lookup, merging, and culling over CE::Memory::BlockManagement<T>'s
 * shared sets. Derived managers decide how blocks are acquired and returned.
 */
    template <typename T>
    struct AbstractBlockManager : BlockManagement<T>,
                                  BlockManagerInterface<T> {
        AbstractBlockManager() = default;
        ~AbstractBlockManager() override = default;
        /** Same monotonic bookkeeping clock as the shared T domain. */
        using clock = typename ::CE::Memory::BlockManagement<T>::clock;
        /** Timestamp in the shared T domain's monotonic clock. */
        using tpoint = typename ::CE::Memory::BlockManagement<T>::tpoint;

        void cull(std::chrono::minutes age) override { ::CE::Memory::BlockTransactions<T>::cull(this->state_, age); }

        void release_culled() override { ::CE::Memory::BlockTransactions<T>::release_culled(this->state_); }

    protected:
        template <typename Tuple> static std::shared_mutex& get_mutex(Tuple& tuple) { return std::get<0>(tuple); }

        template <typename... Tuples> static void read_lock(Tuples&... tuples) {
            // Lambda to lock each shared mutex in shared mode
            auto lock_shared_mutex = [](auto& tuple) { std::get<0>(tuple).lock_shared(); };

            // Apply the lambda to each tuple
            (lock_shared_mutex(tuples), ...);
        }

        template <typename... Tuples> static void read_unlock(Tuples&... tuples) {
            // Lambda to lock each shared mutex in shared mode
            auto lock_shared_mutex = [](auto& tuple) { std::get<0>(tuple).unlock_shared(); };

            // Apply the lambda to each tuple
            (lock_shared_mutex(tuples), ...);
        }

        template <typename... Tuples> static void emplace(::CE::Memory::Block<T> b, Tuples&... tuples) {
            // Update each bookkeeping set under its own mutex. The sets are not
            // changed as one atomic transaction across these separate locks.
            auto lock_set = [](::CE::Memory::Block<T> b, auto&& pair) {
                std::unique_lock<std::shared_mutex> lock(std::get<0>(pair));
                std::get<1>(pair).emplace(b);
            };

            // Apply the same insertion to each selected bookkeeping set.
            (lock_set(b, std::forward<Tuples>(tuples)), ...);
        }

        template <typename... Tuples> static void erase(::CE::Memory::Block<T> b, Tuples&... tuples) {
            // Erase each selected record under that set's own mutex.
            auto lock_set = [](::CE::Memory::Block<T> b, auto&& pair) {
                std::unique_lock<std::remove_reference_t<decltype(std::get<0>(pair))>> lock(std::get<0>(pair));
                std::get<1>(pair).erase(b);
            };

            // Remove the record from every selected bookkeeping set.
            (lock_set(b, std::forward<Tuples>(tuples)), ...);
        }

        template <typename... Tuples> static bool contains(::CE::Memory::Block<T> b, Tuples&... tuples) {
            // Require an exact block match: some set comparators equate records
            // with the same address even when their lengths differ.
            auto share_set = [](::CE::Memory::Block<T> b, auto&& pair) {
                std::shared_lock<std::shared_mutex> lock(std::get<0>(pair));
                const auto& set = std::get<1>(pair);
                const auto found = set.find(b);
                return found != set.end() && *found == b;
            };

            // Every selected set must still contain that complete block.
            return (share_set(b, std::forward<Tuples>(tuples)) && ...);
        }

        template <typename Tuple>::CE::Memory::OptionalBlock<T> search_right(::CE::Memory::Block<T> block, Tuple& tuple) {
            // lower_bound can land on this block or another range at its start;
            // advance until the next higher address within the same owner.
            std::shared_lock<std::shared_mutex> lock(std::get<0>(tuple));
            auto& set = std::get<1>(tuple);
            auto iter = set.lower_bound(block);
            while (iter != set.end() && block.owner == iter->owner && iter->head.get() <= block.head.get()) {
                iter = std::next(iter);
            }
            if (iter != set.end() && block.owner == iter->owner && iter->head.get() > block.head.get()) {
                return {*iter};
            }
            return {std::nullopt};
        }

        template <typename Tuple>::CE::Memory::OptionalBlock<T> search_left(::CE::Memory::Block<T> block, Tuple& tuple) {
            // Walk backward past the lower bound to find an earlier address,
            // stopping when the ordered records belong to a different owner.
            std::shared_lock<std::shared_mutex> lock(std::get<0>(tuple));
            auto& set = std::get<1>(tuple);
            auto iter = set.lower_bound(block);
            while (iter != set.begin()) {
                --iter;
                if (iter->head.get() < block.head.get()) {
                    if (block.owner == iter->owner)
                        return {*iter};
                    break;
                }
            }
            return {std::nullopt};
        }

        template <typename Tuple>::CE::Memory::OptionalBlock<T> contiguous_right(::CE::Memory::Block<T> block, Tuple& tuple) {
            auto ob = search_right(block, tuple);
            if (ob.has_value() && ::CE::Memory::is_contiguous(block, *ob)) {
                return {*ob};
            }
            return {std::nullopt};
        }

        template <typename Tuple>::CE::Memory::OptionalBlock<T> contiguous_left(::CE::Memory::Block<T> block, Tuple& tuple) {
            auto ob = search_left(block, tuple);
            if (ob.has_value() && ::CE::Memory::is_contiguous(block, *ob)) {
                return {*ob};
            }
            return {std::nullopt};
        }

        template <typename Tuple>::CE::Memory::OptionalBlock<T> adjacent_right(::CE::Memory::Block<T> block, Tuple& tuple) {
            std::shared_lock<std::shared_mutex> lock(std::get<0>(tuple));
            auto& set = std::get<1>(tuple);
            auto iter = set.lower_bound(block);
            if (iter != set.end()) {
                if (*iter == block) {
                    iter = std::next(iter);
                }
            }
            if (iter != set.end()) {
                return {*iter};
            }
            return {std::nullopt};
        }

        // Returns the predecessor in the set's ordering, which may differ from address order.
        template <typename Tuple>::CE::Memory::OptionalBlock<T> adjacent_left(::CE::Memory::Block<T> block, Tuple& tuple) {
            std::shared_lock<std::shared_mutex> lock(std::get<0>(tuple));
            auto& set = std::get<1>(tuple);
            const auto next = set.lower_bound(block);
            if (next == set.begin()) {
                return {std::nullopt};
            }
            const auto previous = std::prev(next);
            return {*previous};
        }

        // CE::Memory::BlockManagerInterface interface
        /////////////////////
        void record_new(::CE::Memory::Block<T> block) override { ::CE::Memory::BlockTransactions<T>::record_new(this->state_, block); }

        void mark_stale(::CE::Memory::Block<T> block) override { ::CE::Memory::BlockTransactions<T>::mark_stale(this->state_, block); }

        ::CE::Memory::OptionalBlock<T> merge_into_pool(::CE::Memory::Block<T> block) override {
            return ::CE::Memory::BlockTransactions<T>::return_block(this->state_, block);
        }

        ::CE::Memory::OptionalBlock<T> find_section(T* ptr) override {
            return ::CE::Memory::BlockTransactions<T>::find_section(this->state_, ptr);
        }

        ::CE::Memory::OptionalBlock<T> find_owner(T* ptr) override {
            return ::CE::Memory::BlockTransactions<T>::find_owner(this->state_, ptr);
        }

        ::CE::Memory::OptionalBlock<T> fill_request(std::size_t N, std::align_val_t minimum_alignment = std::align_val_t{0}) override {
            constexpr auto alignment = [] {
                if constexpr (std::is_void_v<T>)
                    return std::align_val_t{64};
                else
                    return std::align_val_t{alignof(T)};
            }();
            // This interface claims one existing range. Compound checkout/splitting
            // uses checkout() instead so a partial partition is never published.
            return ::CE::Memory::BlockTransactions<T>::fill_request(this->state_, N, std::max(alignment, minimum_alignment));
        }
    };

}

namespace std {
    /** Hash complete Block address/alignment/length metadata consistently with equality.
     * The value is process/lifetime dependent and is not a persistent resource ID.
     */
    template <typename T> struct hash<::CE::Memory::Block<T>> {
        std::size_t operator()(const ::CE::Memory::Block<T>& k) const noexcept {
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

    /** Format a Block's head address, byte alignment and element count without dereferencing storage.
     * No custom format specifier semantics are added. Caller keeps metadata stable.
     */
    template <typename T> struct formatter<::CE::Memory::Block<T>> {
        constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

        auto format(const ::CE::Memory::Block<T>& a, std::format_context& ctx) const {
            return std::format_to(
                ctx.out(), "{} [alignment: {}, length: {}]", static_cast<void*>(a.head.get()), static_cast<std::size_t>(a.alignment),
                a.length
            );
        }
    };

}

namespace CE::Memory {
    /** Write owned range metadata to a compatible stream without reading backing bytes.
     * @return The supplied stream; formatting/stream errors propagate.
     * The constrained fallback avoids overriding a stream's own Block member overload.
     */
    template <typename S, typename T>
        requires(!requires(S& stream, const Block<T>& value) { stream.operator<<(value); })
    S& operator<<(S& os, const Block<T>& block) {
        //uint32_t owner = std::get<uint32_t>(CE::Memory::Pointers::pointer_to_hash(block.owner.get(),4));
        auto owner = static_cast<void*>(block.owner.get());
        os << std::format(
            "{} [alignment: {}, length: {}({}), owner: {}]", static_cast<void*>(block.head.get()),
            static_cast<std::size_t>(block.alignment), (void*)block.length, block.length, owner
        );
        return os;
    }

    /** Write an alignment value as its byte count and return the supplied stream.
     * This formats metadata only; it does not validate an allocation's alignment.
     */
    template <typename S> S& operator<<(S& os, const std::align_val_t alignment) {
        os << std::format("{}", static_cast<std::size_t>(alignment));
        return os;
    }
}

#include "block-transactions.h"

/**
 * @fn template<typename T> std::vector<typename CE::Memory::Block<T>::spointer> CE::Memory::Block<T>::vector(std::function<void(T*)> d)
 * @brief Create separate shared_ptr control blocks for each element of a nonvoid range.
 * @param d Copied per-element final deleter responsible for constructed lifetime and retaining/releasing backing storage.
 * @return Independent element handles; they are not aliases of owner and this helper does not itself retain that owner.
 * Requires addressable contiguous T storage for length elements and external lifetime/synchronization. No objects are constructed; allocation failure releases already made handles through their deleters.
 */

/**
 * @fn template<typename T> std::shared_ptr<typename CE::Memory::BlockManagement<T>::State> CE::Memory::BlockManagement<T>::shared_state()
 * @return Retained shared collection state for this exact T specialization. All corresponding manager facades share it; retaining it pins bookkeeping/backing owners, not a facade or synchronized access to user objects.
 */

/**
 * @fn template<typename T> CE::Memory::BlockManagerInterface<T>::~BlockManagerInterface()
 * @brief Release the polymorphic facade; retained state/release contexts can outlive it under their own contracts.
 */

/**
 * @fn template<typename T> void CE::Memory::BlockManagerInterface<T>::record_new(CE::Memory::Block<T> block)
 * @brief Record a complete backing owner in a derived manager's registry.
 * @param block Valid complete owner metadata. Derived managers/transactions own locking and partition invariants; this interface alone does not allocate or construct objects.
 */

/**
 * @fn template<typename T> void CE::Memory::BlockManagerInterface<T>::mark_stale(CE::Memory::Block<T> block)
 * @brief Mark a complete free owner eligible for age-based culling.
 * @param block Exact current owner metadata; derived managers enforce whether it is fully free.
 */

/**
 * @fn template<typename T> CE::Memory::OptionalBlock<T> CE::Memory::BlockManagerInterface<T>::merge_into_pool(CE::Memory::Block<T> block)
 * @brief Return a current range and merge compatible free neighbors.
 * @param block Checked-out range whose user/object lifetime has ended.
 * @return Merged free range or no value when rejected; concrete manager policy determines errors and locking.
 */

/**
 * @fn template<typename T> CE::Memory::OptionalBlock<T> CE::Memory::BlockManagerInterface<T>::find_section(T* pointer)
 * @brief Find range metadata containing a pointer in recorded split sections.
 * @param pointer Address in candidate backing storage.
 * @return Retained range metadata or no match; observation does not reserve the range against concurrent checkout/return.
 */

/**
 * @fn template<typename T> CE::Memory::OptionalBlock<T> CE::Memory::BlockManagerInterface<T>::find_owner(T* pointer)
 * @brief Find complete backing-allocation metadata containing a pointer.
 * @param pointer Candidate address.
 * @return Retained owner metadata or no match; this is not ownership transfer or a live-object check.
 */

/**
 * @fn template<typename T> CE::Memory::OptionalBlock<T> CE::Memory::BlockManagerInterface<T>::fill_request(std::size_t count, std::align_val_t minimum_alignment)
 * @brief Claim an existing compatible free range without allocating new backing.
 * @param count Required elements, or bytes for void.
 * @param minimum_alignment Minimum head alignment in bytes; concrete typed/void defaults may impose a stronger minimum.
 * @return Claimed whole range or no match. Compound split checkout belongs to BlockTransactions::checkout().
 */

/**
 * @fn template<typename T> void CE::Memory::BlockManagerInterface<T>::cull(std::chrono::minutes age)
 * @brief Queue sufficiently old complete free owners for later release.
 * @param age Minimum staleness duration in minutes; this does not force live ranges free.
 * Concrete implementations define collection synchronization; final owner destruction belongs to release_culled().
 */

/**
 * @fn template<typename T> void CE::Memory::BlockManagerInterface<T>::release_culled()
 * @brief Release still-eligible queued whole owners; do not destroy final backing under collection locks. Exact validation and lifecycle policy belong to the concrete manager.
 */

/**
 * @fn template<typename T> CE::Memory::AbstractBlockManager<T>::AbstractBlockManager()
 * @brief Retain the shared T bookkeeping state without allocating object backing or acquiring a range.
 */

/**
 * @fn template<typename T> CE::Memory::AbstractBlockManager<T>::~AbstractBlockManager()
 * @brief Release the facade's state reference; other retained contexts/owners preserve shared bookkeeping.
 */

/**
 * @fn template<typename T> void CE::Memory::AbstractBlockManager<T>::cull(std::chrono::minutes age)
 * @brief Queue complete free owners old enough for release through BlockTransactions.
 * @param age Minimum duration since a whole owner became free; negative values make current free owners immediately eligible.
 * Does not release backing yet or invalidate checked-out ranges.
 */

/**
 * @fn template<typename T> void CE::Memory::AbstractBlockManager<T>::release_culled()
 * @brief Transactionally remove eligible complete free owners, then release final backing after unlocking. Owners that became checked out/split again are not forcibly freed.
 */

/**
 * @fn template<typename T> template<typename Tuple> std::shared_mutex& CE::Memory::AbstractBlockManager<T>::get_mutex(Tuple& tuple)
 * @return Borrowed mutex at tuple index zero; caller keeps the collection tuple alive and follows its lock-order policy.
 */

/**
 * @fn template<typename T> template<typename... Tuples> void CE::Memory::AbstractBlockManager<T>::read_lock(Tuples&... tuples)
 * @brief Acquire shared mutexes sequentially in argument order.
 * @param tuples Live collection tuples whose element zero is a shared_mutex.
 * Low-level non-RAII helper: caller avoids inconsistent lock order/reentry and releases acquired locks, including cleanup if a later lock throws. It does not atomically lock all collections.
 */

/**
 * @fn template<typename T> template<typename... Tuples> void CE::Memory::AbstractBlockManager<T>::read_unlock(Tuples&... tuples)
 * @brief Release each previously held shared mutex in argument order.
 * @param tuples Exactly the collection tuples locked by the calling thread.
 * No ownership validation, transaction or automatic exception cleanup is supplied.
 */

/**
 * @fn template<typename T> template<typename... Tuples> void CE::Memory::AbstractBlockManager<T>::emplace(CE::Memory::Block<T> b, Tuples&... tuples)
 * @brief Insert range metadata into each selected collection under separate write locks.
 * @param b Valid metadata copied into each collection.
 * @param tuples Live bookkeeping tuple/set pairs.
 * This is not a cross-collection transaction: a later insertion failure can leave earlier collections changed. Use BlockTransactions for invariant-preserving compound operations.
 */

/**
 * @fn template<typename T> template<typename... Tuples> void CE::Memory::AbstractBlockManager<T>::erase(CE::Memory::Block<T> b, Tuples&... tuples)
 * @brief Erase comparator-matching metadata from each selected set under its own write lock.
 * @param b Lookup metadata.
 * @param tuples Live collection tuples.
 * Separate lock scopes do not form an atomic multi-collection transition; callers maintain invariants/quiescence when using this primitive.
 */

/**
 * @fn template<typename T> template<typename... Tuples> bool CE::Memory::AbstractBlockManager<T>::contains(CE::Memory::Block<T> b, Tuples&... tuples)
 * @brief Check a complete Block equality match in every selected collection under separate read locks.
 * @param b Full owner/head/alignment/length metadata.
 * @param tuples Live collection tuples.
 * @return True when each individual observation found exact metadata; this is not one atomic snapshot across all sets.
 */

/**
 * @fn template<typename T> template<typename Tuple> CE::Memory::OptionalBlock<T> CE::Memory::AbstractBlockManager<T>::search_right(CE::Memory::Block<T> block, Tuple& tuple)
 * @brief Observe the next higher-address range of the same backing owner.
 * @param block Starting metadata.
 * @param tuple Ordered collection borrowed under its read lock.
 * @return Copied range or no match; does not reserve or verify contiguity.
 */

/**
 * @fn template<typename T> template<typename Tuple> CE::Memory::OptionalBlock<T> CE::Memory::AbstractBlockManager<T>::search_left(CE::Memory::Block<T> block, Tuple& tuple)
 * @brief Observe an earlier-address range of the same backing owner.
 * @param block Starting metadata.
 * @param tuple Ordered collection borrowed under its read lock.
 * @return Copied range or no match; no checkout or contiguity guarantee follows.
 */

/**
 * @fn template<typename T> template<typename Tuple> CE::Memory::OptionalBlock<T> CE::Memory::AbstractBlockManager<T>::contiguous_right(CE::Memory::Block<T> block, Tuple& tuple)
 * @brief Find a same-owner right neighbor whose head meets this range's end.
 * @param block Stable valid starting metadata.
 * @param tuple Ordered collection used for the observation.
 * @return Retained neighbor metadata or no match; concurrent transitions require the transactional API.
 */

/**
 * @fn template<typename T> template<typename Tuple> CE::Memory::OptionalBlock<T> CE::Memory::AbstractBlockManager<T>::contiguous_left(CE::Memory::Block<T> block, Tuple& tuple)
 * @brief Find a same-owner left neighbor whose end meets this range's head.
 * @param block Stable valid starting metadata.
 * @param tuple Ordered collection used for the observation.
 * @return Retained neighbor metadata or no match; no range is claimed.
 */

/**
 * @fn template<typename T> template<typename Tuple> CE::Memory::OptionalBlock<T> CE::Memory::AbstractBlockManager<T>::adjacent_right(CE::Memory::Block<T> block, Tuple& tuple)
 * @brief Observe the successor under the set's ordering, which may differ from address order.
 * @param block Comparator search key.
 * @param tuple Ordered collection borrowed under a shared lock.
 * @return Copied successor or no value, without owner/contiguity validation.
 */

/**
 * @fn template<typename T> template<typename Tuple> CE::Memory::OptionalBlock<T> CE::Memory::AbstractBlockManager<T>::adjacent_left(CE::Memory::Block<T> block, Tuple& tuple)
 * @brief Observe the predecessor under the set's ordering, which may differ from address order.
 * @param block Comparator search key.
 * @param tuple Ordered collection borrowed under a shared lock.
 * @return Copied predecessor or no value, without claiming or validating contiguity.
 */

/**
 * @fn template<typename T> void CE::Memory::AbstractBlockManager<T>::record_new(CE::Memory::Block<T> block)
 * @brief Record complete backing metadata through the shared-domain transaction owner.
 * @param block Valid owner range; allocation of collection records may throw. Does not construct T or make it a free pool range.
 */

/**
 * @fn template<typename T> void CE::Memory::AbstractBlockManager<T>::mark_stale(CE::Memory::Block<T> block)
 * @brief Timestamp an exact complete free owner under coordinated bookkeeping locks.
 * @param block Valid complete owner; absent/nonfree metadata is ignored.
 */

/**
 * @fn template<typename T> CE::Memory::OptionalBlock<T> CE::Memory::AbstractBlockManager<T>::merge_into_pool(CE::Memory::Block<T> block)
 * @brief Return a current checked-out range through BlockTransactions, merging free neighbors.
 * @param block Range whose live users/objects have ended.
 * @return Merged free metadata, or no value for invalid/already-pooled input. Allocation failures preserve the prior logical partition.
 */

/**
 * @fn template<typename T> CE::Memory::OptionalBlock<T> CE::Memory::AbstractBlockManager<T>::find_section(T* ptr)
 * @brief Copy split-section metadata containing ptr under the section read lock.
 * @param ptr Candidate address in retained storage.
 * @return Matching range or no value; observation is not an exclusive reservation.
 */

/**
 * @fn template<typename T> CE::Memory::OptionalBlock<T> CE::Memory::AbstractBlockManager<T>::find_owner(T* ptr)
 * @brief Copy complete backing-owner metadata containing ptr under the registry read lock.
 * @param ptr Candidate address.
 * @return Matching owner or no value; not a live-object or checked-out-state guarantee.
 */

/**
 * @fn template<typename T> CE::Memory::OptionalBlock<T> CE::Memory::AbstractBlockManager<T>::fill_request(std::size_t N, std::align_val_t minimum_alignment)
 * @brief Claim one existing free range with sufficient count and alignment.
 * @param N Minimum elements, or bytes for void; callers supply a meaningful positive request.
 * @param minimum_alignment Additional alignment requirement; effective minimum is alignof(T), or 64 for void.
 * @return Whole compatible range or no value; no new allocation or split occurs.
 * @see [Block transactions](cheryl-guide:resources/resource-lifetime#block-transactions)
 */

/**
 * @fn template<typename T> bool CE::Memory::Ordering::HeadOrder<T>::operator()(const CE::Memory::Block<T>& lhs, const CE::Memory::Block<T>& rhs) const noexcept
 * @brief Order by increasing head address, then length, then alignment. Valid comparable metadata is required; backing bytes are never read.
 */

/**
 * @fn template<typename T> bool CE::Memory::Ordering::RegistryOrder<T>::operator()(const CE::Memory::Block<T>& a, const CE::Memory::Block<T>& b) const noexcept
 * @brief Order by owner address then descending head; ranges sharing those addresses can be comparator-equivalent despite different lengths/alignment.
 */

/**
 * @fn template<typename T> bool CE::Memory::Ordering::PoolOrder<T>::operator()(const CE::Memory::Block<T>& lhs, const CE::Memory::Block<T>& rhs) const noexcept
 * @brief Order reusable candidates by descending alignment, length, head, then owner. This compares metadata and does not check out or reserve a range.
 */

/**
 * @fn template<typename T> std::size_t std::hash<::CE::Memory::Block<T> >::operator()(const CE::Memory::Block<T>& k) const noexcept
 * @return Combined address/alignment/length hash consistent with Block metadata equality, without dereferencing or retaining additional storage.
 */

/**
 * @fn template<typename T> auto std::formatter<::CE::Memory::Block<T> >::parse(std::format_parse_context& ctx)
 * @brief Accept the default formatting position without consuming custom specifiers.
 * @param ctx Borrowed format parser context.
 * @return Its current iterator; standard formatting validates remaining syntax.
 */

/**
 * @fn template<typename T> auto std::formatter<::CE::Memory::Block<T> >::format(const CE::Memory::Block<T>& a, std::format_context& ctx) const
 * @brief Write head address, byte alignment and element length to the supplied format output.
 * @param a Stable metadata; no backing bytes are read.
 * @param ctx Borrowed output context.
 * @return Output iterator following the formatted representation; formatting/output errors propagate.
 */
