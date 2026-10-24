#pragma once

/** @file
 * @brief Standard-container adapter for the shared CE byte domain.
 * @see [CPU memory lifetime](cheryl-guide:resources/resource-lifetime)
 */
#include <core/resources/memory/mem-mgr.h>
#include "allocator-count.hpp"

namespace CE::Memory::Allocators {
    /** Raw byte storage with ordinary, untracked object construction.
     * Greedy checkout retains the existing type-sized growth policy.
     * All T specializations share void-block bookkeeping, so storage returns are
     * interchangeable within this adapter family. No independent release context
     * is retained: finish users and deallocation before manager facade teardown.
     * @tparam T Complete non-void object type, including scalar types.
     */
    template <class T> struct ObjectAllocator {
        using value_type = T;                                          ///< Object type; allocation counts T slots.
        using manager_type = ::CE::Memory::ObjectMemoryManager<T>;     ///< Existing byte service; lengths remain bytes.
        using is_always_equal = std::true_type;                        ///< Same-family instances share the byte domain.
        using propagate_on_container_copy_assignment = std::true_type; ///< Preserve the established copy policy.
        using propagate_on_container_move_assignment = std::true_type; ///< Transfer the allocator with storage.
        using propagate_on_container_swap = std::true_type;            ///< Swap allocators with storage.
        template <class U> struct rebind {
            using other = ObjectAllocator<U>;
        };

        ObjectAllocator() = default;
        template <class U> ObjectAllocator(const ObjectAllocator<U>&) noexcept {}
        [[nodiscard]] ObjectAllocator select_on_container_copy_construction() const { return *this; }
        [[nodiscard]] std::size_t max_size() const noexcept { return Detail::allocator_max_count<T>(); }

        /** Positive checked slot count; zero/unaddressable products throw bad_request before checkout.
         * Alignment is at least alignof(T); allocation/service exceptions propagate.
         */
        [[nodiscard]] T* allocate(std::size_t count) {
            Detail::check_allocator_count<T>(count);
            auto block = manager_type::get().checkout_chunk(sizeof(T) * count, alignof(T), ::CE::Memory::greedy);
            return static_cast<T*>(block.head.get());
        }
        /** End all lifetimes first. Return the entire granted byte section; count is ignored.
         * Null/unknown/already-pooled pointers follow Manager::return_ptr's no-op policy.
         */
        void deallocate(T* pointer, std::size_t /*count*/) { manager_type::get().return_ptr(pointer); }
        template <class U, class... Args> void construct(U* pointer, Args&&... args) {
            std::construct_at(pointer, std::forward<Args>(args)...);
        }
        template <class U> void destroy(U* pointer) { std::destroy_at(pointer); }
        template <class U> bool operator==(const ObjectAllocator<U>&) const noexcept { return true; }
    };
}
