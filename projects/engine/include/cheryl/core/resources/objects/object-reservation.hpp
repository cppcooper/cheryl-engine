#pragma once

#include <core/resources/memory/allocators/object-pool-allocator.hpp>
#include <internals/exceptions.h>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace CE::Memory::Objects {
    /**
     * Manager interface for reserving blocks and retaining a context capable
     * of returning their individual ranges.
     */
    template <typename Manager, typename T>
    concept ReservableManager = requires(Manager& manager, std::size_t count, T* ptr) {
        typename Manager::release_context_type;
        requires std::derived_from<typename Manager::release_context_type, ::CE::Memory::AbstractBlockManager<T>>;
        { manager.retrieve_block(count) } -> std::same_as<::CE::Memory::Block<T>>;
        { manager.release_context() } -> std::same_as<std::shared_ptr<typename Manager::release_context_type>>;
        { std::declval<typename Manager::release_context_type&>().return_objects(ptr, count) } -> std::same_as<void>;
        { std::declval<typename Manager::release_context_type&>().release_owned(ptr, count) } -> std::same_as<void>;
    };

    /**
     * Allocator interface accepted by CE::Memory::Objects::Reservation: the allocator must
     * identify a ReservableManager and expose its retained release context.
     * Ordinary allocate/deallocate still operate on whole allocations.
     */
    template <typename Allocator, typename T>
    concept ReservationAllocator = requires(Allocator& allocator, std::size_t count, T* ptr) {
        typename Allocator::value_type;
        typename Allocator::manager_type;
        requires std::same_as<typename Allocator::value_type, T>;
        requires ReservableManager<typename Allocator::manager_type, T>;
        { allocator.context() } -> std::same_as<std::shared_ptr<typename Allocator::manager_type::release_context_type>>;
        { allocator.allocate(count) } -> std::same_as<T*>;
        { allocator.deallocate(ptr, count) } -> std::same_as<void>;
    };

    /**
     * Reserves one CE::Memory::Block and tracks only its unclaimed ranges. emplace() splits
     * out a slot and gives its handle the release context; the handle may outlive
     * this reservation. Failed construction leaves the slot reserved, while the
     * destructor returns remaining contiguous ranges to the manager.
     * Move construction transfers the reservation; copying and both assignment forms are deleted.
     */
    template <typename T, typename Allocator = ::CE::Memory::Allocators::ObjectPoolAllocator<T>>
        requires ReservationAllocator<Allocator, T>
    class Reservation {
        using context_type = typename Allocator::manager_type::release_context_type;
        /**
         * Controls the constructed slot behind an aliasing shared_ptr<T>.
         * Its final release destroys T and returns that one slot.
         */
        struct Claimed {
            std::shared_ptr<context_type> context;
            T* pointer;
            bool constructed = false;

            ~Claimed() noexcept {
                if (constructed) {
                    std::destroy_at(pointer);
                    context->release_owned(pointer, 1);
                }
            }
        };

        std::shared_ptr<context_type> context_;
        std::vector<::CE::Memory::Block<T>> remaining_;
        T* base_ = nullptr;
        std::size_t count_ = 0;

    public:
        explicit Reservation(std::size_t count, Allocator allocator = {})
        : context_(allocator.context()), count_(count) {
            if (count) {
                remaining_.reserve(1);
                auto block = context_->retrieve_block(count);
                base_ = block.head.get();
                remaining_.push_back(std::move(block));
            }
        }
        Reservation(const Reservation&) = delete;
        Reservation& operator=(const Reservation&) = delete;
        Reservation(Reservation&& other) noexcept
        : context_(std::move(other.context_)), base_(std::exchange(other.base_, nullptr)), count_(std::exchange(other.count_, 0)) {
            remaining_.swap(other.remaining_);
        }
        Reservation& operator=(Reservation&&) = delete;
        ~Reservation() noexcept {
            for (const auto& block : remaining_) {
                context_->release_owned(block.head.get(), block.length);
            }
        }

        [[nodiscard]] const std::vector<::CE::Memory::Block<T>>& remaining_ranges() const noexcept { return remaining_; }
        [[nodiscard]] std::size_t size() const noexcept { return count_; }

        template <typename... Args> std::shared_ptr<T> emplace(std::size_t index, Args&&... args) {
            if (index >= count_)
                throw ::CE::Exceptions::bad_request(CE_HERE, "Object reservation index is out of range.");
            T* pointer = base_ + index;
            // Search the current unclaimed partition, not the original block:
            // a previously handed-out slot must never be constructed again.
            std::size_t range_index = 0;
            for (; range_index < remaining_.size(); ++range_index) {
                if (remaining_[range_index].contains(pointer))
                    break;
            }
            if (range_index == remaining_.size())
                throw ::CE::Exceptions::bad_request(CE_HERE, "Object reservation slot has already been claimed.");

            // Split a local copy so the recorded range survives any failure
            // before construction. The original owner remains in every piece.
            auto slot = remaining_[range_index];
            std::optional<::CE::Memory::Block<T>> before;
            std::optional<::CE::Memory::Block<T>> after;
            const auto offset = static_cast<std::size_t>(pointer - slot.head.get());
            if (offset) {
                auto tail = slot.split_exactly(offset);
                before = std::move(slot);
                slot = std::move(*tail);
            }
            if (slot.length > 1)
                after = slot.split_exactly(1);

            // Complete all allocations before constructing T. The commit below only
            // moves Blocks and cannot fail after a successful constructor.
            remaining_.reserve(remaining_.size() + 1);
            auto claim = std::make_shared<Claimed>(context_, pointer);
            auto object = std::shared_ptr<T>(claim, pointer);
            std::construct_at(pointer, std::forward<Args>(args)...);
            claim->constructed = true;

            // Transfer the claimed slot out of the reservation. Only the
            // unclaimed left and right pieces remain for its destructor.
            auto it = remaining_.begin() + range_index;
            if (before) {
                *it = std::move(*before);
                if (after)
                    remaining_.insert(it + 1, std::move(*after));
            } else if (after) {
                *it = std::move(*after);
            } else {
                remaining_.erase(it);
            }
            return object;
        }
    };
}

/**
 * @fn template<typename T, typename Allocator> CE::Memory::Objects::Reservation<T, Allocator>::Reservation(std::size_t count, Allocator allocator)
 * @brief Reserve unconstructed storage and retain its pool release context.
 * @tparam T Reserved object type; its final destruction must not throw.
 * @tparam Allocator ReservationAllocator exposing a retained context for T.
 * @param count Logical slot count; zero creates an empty reservation without checkout.
 * @param allocator Copied only to capture the retained context.
 * Allocation/checkout errors propagate without constructing T objects. The reservation is caller-owned and unsynchronized; do not race emplace(), move or destruction.
 */

/**
 * @fn template<typename T, typename Allocator> CE::Memory::Objects::Reservation<T, Allocator>::Reservation(Reservation&& other) noexcept
 * @brief Transfer context and unclaimed ranges, leaving other empty with size zero. Previously claimed object handles remain independent; caller excludes concurrent operations.
 */

/**
 * @fn template<typename T, typename Allocator> CE::Memory::Objects::Reservation<T, Allocator>::~Reservation() noexcept
 * @brief Return remaining unclaimed contiguous ranges without touching already claimed objects.
 * Claimed handles retain the release context and may outlive this reservation/facade. Invalid final-return bookkeeping terminates under release_owned() rules; this is not a destructor for unconstructed slots.
 */

/**
 * @fn template<typename T, typename Allocator> auto CE::Memory::Objects::Reservation<T, Allocator>::remaining_ranges() const noexcept -> const std::vector<CE::Memory::Block<T>>&
 * @return Borrowed unclaimed partition; emplace()/move/destruction can invalidate references/iterators or change its contents. Retain and exclusively own the reservation while inspecting it.
 */

/**
 * @fn template<typename T, typename Allocator> std::size_t CE::Memory::Objects::Reservation<T, Allocator>::size() const noexcept
 * @return Original logical slot count, not remaining unclaimed slots; moved-from reservations report zero.
 */

/**
 * @fn template<typename T, typename Allocator> template<typename... Args> std::shared_ptr<T> CE::Memory::Objects::Reservation<T, Allocator>::emplace(std::size_t index, Args&&... args)
 * @brief Construct one unclaimed slot and transfer it to an independently retained owner.
 * @param index Zero-based index below size(), not an index into remaining_ranges().
 * @param args Perfect-forwarded constructor arguments used only for this object.
 * @return Aliasing shared_ptr whose final release destroys T once and returns exactly one slot through the retained context.
 * Out-of-range/already-claimed indices throw bad_request. Allocation or constructor failure leaves that slot reserved and the partition unchanged; retry is permitted. No other slot is constructed. Returned objects may outlive the reservation/pool facade; caller synchronizes object access and supplies nonthrowing destruction.
 * @see [CPU memory lifetime](cheryl-guide:resources/resource-lifetime)
 */
