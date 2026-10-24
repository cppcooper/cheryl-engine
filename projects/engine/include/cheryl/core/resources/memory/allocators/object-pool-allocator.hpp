#pragma once

/** @file
 * @brief Standard-container adapter retaining existing typed pool services.
 * @see [CPU memory lifetime](cheryl-guide:resources/resource-lifetime)
 */
#include <core/resources/objects/pool.h>
#include "allocator-count.hpp"

namespace CE::Memory::Allocators {
    namespace Detail {
        // One retained context directory for the existing globally shared typed
        // domains. Conversions only copy its handle; first type setup belongs to
        // allocation/context acquisition, never converting construction or return.
        struct PoolAllocatorContexts {
            std::mutex mutex;
            std::unordered_map<const void*, std::shared_ptr<void>> types;
            template <class T> static const void* key() noexcept {
                static const char tag{};
                return &tag;
            }
            template <class T> void retain(const std::shared_ptr<::CE::Memory::Objects::PoolState<T>>& state) {
                std::lock_guard lock(mutex);
                types.try_emplace(key<T>(), state);
            }
            template <class T> auto acquire() {
                std::lock_guard lock(mutex);
                auto it = types.find(key<T>());
                if (it == types.end()) {
                    auto state = ::CE::Memory::Objects::Pool<T>::get().release_context();
                    it = types.try_emplace(key<T>(), std::move(state)).first;
                }
                return std::static_pointer_cast<::CE::Memory::Objects::PoolState<T>>(it->second);
            }
            template <class T> auto retained() noexcept {
                std::lock_guard lock(mutex);
                // A valid allocation registered T before storage publication.
                return std::static_pointer_cast<::CE::Memory::Objects::PoolState<T>>(types.find(key<T>())->second);
            }
        };
        inline std::shared_ptr<PoolAllocatorContexts> pool_allocator_contexts() {
            static const auto state = std::make_shared<PoolAllocatorContexts>();
            return state;
        }
    }

    /** Class-only slot storage retaining PoolState<T> and its construction context.
     * All contexts for the SAME T share block and Constructor<T> state, even across
     * facade recreation; context pointer identity is not domain identity. Cross-type
     * comparisons also remain equal: equality identifies the retained service family,
     * not permission to return a U pointer as T. Rebind copies the directory without
     * allocation; the destination pool is acquired on first allocate/context use.
     * Copies (including moves) keep their source usable and retain the same context.
     * Allocations/returns and T construction/destruction then outlive the facade.
     * Tracking serializes slot transitions, never user accesses to object contents.
     * @tparam T Complete class type. Use byte adapters for char/double storage.
     */
    template <class T> struct ObjectPoolAllocator {
        static_assert(std::is_class_v<T>, "ObjectPoolAllocator requires a class slot type");
        using value_type = T;                                             ///< Counted typed slot, not byte count.
        using manager_type = ::CE::Memory::Objects::Pool<T>;              ///< Existing per-class pool.
        using context_type = typename manager_type::release_context_type; ///< Retained backing/tracker context.
        using is_always_equal = std::true_type;                           ///< Every same-T instance shares storage/tracking domains.
        using propagate_on_container_copy_assignment = std::true_type;    ///< Retain the copied context.
        using propagate_on_container_move_assignment = std::true_type;    ///< Transfer context with storage.
        using propagate_on_container_swap = std::true_type;               ///< Swap context with storage.
        template <class U> struct rebind {
            using other = ObjectPoolAllocator<U>;
        };

    private:
        template <class> friend struct ObjectPoolAllocator;
        std::shared_ptr<Detail::PoolAllocatorContexts> family_;
        std::shared_ptr<context_type> context_;

    public:
        ObjectPoolAllocator()
        : ObjectPoolAllocator(manager_type::get()) {}
        /** Capture a live facade; it need not outlive this allocator. */
        explicit ObjectPoolAllocator(manager_type& pool)
        : family_(Detail::pool_allocator_contexts()), context_(pool.release_context()) {
            family_->template retain<T>(context_);
        }
        ObjectPoolAllocator(const ObjectPoolAllocator&) = default;
        ObjectPoolAllocator& operator=(const ObjectPoolAllocator&) = default;
        template <class U>
        ObjectPoolAllocator(const ObjectPoolAllocator<U>& other) noexcept
        : family_(other.family_) {}
        [[nodiscard]] ObjectPoolAllocator select_on_container_copy_construction() const { return *this; }
        [[nodiscard]] std::shared_ptr<context_type> context() const { return context_ ? context_ : family_->template acquire<T>(); }
        [[nodiscard]] std::size_t max_size() const noexcept { return Detail::allocator_max_count<T>(); }

        /** Positive addressable slots, checked before even a reusable-range lookup.
         * Zero/overflow throws bad_request; alignment is at least alignof(T).
         */
        [[nodiscard]] T* allocate(std::size_t count) {
            Detail::check_allocator_count<T>(count);
            return static_cast<T*>(context()->retrieve_block(count).head.get());
        }
        /** Return exactly count intended slots, without destruction or metadata allocation.
         * Requires a valid allocation/range. Invalid allocator deallocation violates
         * its precondition; use context()->return_objects for checked service returns.
         */
        void deallocate(T* pointer, std::size_t count) noexcept { family_->template retained<T>()->return_objects(pointer, count); }

        /** T uses retained raw/live tracking. A DIFFERENT U is a value subobject
         * (e.g. pair inside a rebound map node), not a whole T slot: construct it
         * directly, untracked, and destroy through the matching generic hook.
         * This introduces no Pool<U> or static-tracker teardown dependency.
         * User exceptions propagate; no storage return or batch rollback occurs.
         */
        template <class U, class... Args> void construct(U* pointer, Args&&... args) {
            if constexpr (std::is_same_v<T, U>)
                context()->construction_context()->construct(pointer, 1, std::forward<Args>(args)...);
            else
                std::construct_at(pointer, std::forward<Args>(args)...);
        }
        template <class U> void destroy(U* pointer) {
            if constexpr (std::is_same_v<T, U>)
                context()->construction_context()->destroy(pointer);
            else
                std::destroy_at(pointer);
        }
        template <class U> bool operator==(const ObjectPoolAllocator<U>&) const noexcept { return true; }
    };
}
