#pragma once

/** @file
 * @brief Standard-container adapter retaining existing typed pool services.
 * @see [CPU memory lifetime](cheryl-guide:resources/resource-lifetime)
 */
#include <core/resources/objects/pool.h>
#include "allocator-count.hpp"

namespace CE::Memory::Allocators {
    /** Class-only slot storage retaining PoolState<T> and its construction context.
     * All contexts for the SAME T share block and Constructor<T> state, even across
     * facade recreation; context pointer identity is not domain identity. Different
     * T domains are not interchangeable. Rebind captures the destination class pool;
     * conversion back is equivalent. Construct allocators before facade teardown.
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
        std::shared_ptr<context_type> context_;

    public:
        ObjectPoolAllocator()
        : ObjectPoolAllocator(manager_type::get()) {}
        /** Capture a live facade; it need not outlive this allocator. */
        explicit ObjectPoolAllocator(manager_type& pool)
        : context_(pool.release_context()) {}
        ObjectPoolAllocator(const ObjectPoolAllocator&) = default;
        ObjectPoolAllocator& operator=(const ObjectPoolAllocator&) = default;
        template <class U>
        ObjectPoolAllocator(const ObjectPoolAllocator<U>&)
        : ObjectPoolAllocator() {}
        [[nodiscard]] ObjectPoolAllocator select_on_container_copy_construction() const { return *this; }
        [[nodiscard]] std::shared_ptr<context_type> context() const { return context_; }
        [[nodiscard]] std::size_t max_size() const noexcept { return Detail::allocator_max_count<T>(); }

        /** Positive addressable slots, checked before even a reusable-range lookup.
         * Zero/overflow throws bad_request; alignment is at least alignof(T).
         */
        [[nodiscard]] T* allocate(std::size_t count) {
            Detail::check_allocator_count<T>(count);
            return static_cast<T*>(context_->retrieve_block(count).head.get());
        }
        /** Return exactly count intended slots, without destruction. Unknown/zero/
         * already-free/out-of-bounds ranges reject; partial slot returns remain supported.
         */
        void deallocate(T* pointer, std::size_t count) { context_->return_objects(pointer, count); }

        /** T uses retained raw/live tracking. A DIFFERENT U is a value subobject
         * (e.g. pair inside a rebound map node), not a whole T slot: construct it
         * directly, untracked, and destroy through the matching generic hook.
         * This introduces no Pool<U> or static-tracker teardown dependency.
         * User exceptions propagate; no storage return or batch rollback occurs.
         */
        template <class U, class... Args> void construct(U* pointer, Args&&... args) {
            if constexpr (std::is_same_v<T, U>)
                context_->construction_context()->construct(pointer, 1, std::forward<Args>(args)...);
            else
                std::construct_at(pointer, std::forward<Args>(args)...);
        }
        template <class U> void destroy(U* pointer) {
            if constexpr (std::is_same_v<T, U>)
                context_->construction_context()->destroy(pointer);
            else
                std::destroy_at(pointer);
        }
        template <class U> bool operator==(const ObjectPoolAllocator<U>&) const noexcept { return std::is_same_v<T, U>; }
    };
}
