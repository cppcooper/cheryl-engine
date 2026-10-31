#pragma once
#include <core/resources/objects/pool.h>
#include <core/resources/objects/object-construction.hpp>
#include <limits>
#include <new>

namespace CE::Memory::Allocators {
    /** Class-slot allocator retaining actual Pool state across facade teardown.
     * Primary allocator_traits rebinds class nodes to their matching typed pool.
     * All contexts for the same T share bookkeeping, so same-type instances are
     * interchangeable. T construction/destruction uses the retained tracker;
     * construction of a different U subobject uses ordinary C++ lifetimes.
     * Final storage return reports and terminates on bookkeeping failure.
     */
    template <class T> class ObjectPoolAllocator {
        using pool_type = ::CE::Memory::Objects::Pool<T>;
        std::shared_ptr<typename pool_type::release_context_type> context_;

    public:
        using value_type = T;
        using manager_type = pool_type;
        using context_type = typename manager_type::release_context_type;
        using is_always_equal = std::true_type;
        using propagate_on_container_swap = std::true_type;
        using propagate_on_container_move_assignment = std::true_type;
        using propagate_on_container_copy_assignment = std::true_type;
        template <class U> struct rebind { using other = ObjectPoolAllocator<U>; };

        ObjectPoolAllocator() : context_(manager_type::get().release_context()) {}
        explicit ObjectPoolAllocator(std::shared_ptr<context_type> context) : context_(std::move(context)) {
            if (!context_)
                throw ::CE::Exceptions::bad_request(CE_HERE, "ObjectPoolAllocator requires a retained pool context.");
        }
        ObjectPoolAllocator(const ObjectPoolAllocator&) noexcept = default;
        ObjectPoolAllocator& operator=(const ObjectPoolAllocator&) noexcept = default;
        template <class U> ObjectPoolAllocator(const ObjectPoolAllocator<U>&) : ObjectPoolAllocator() {}
        [[nodiscard]] std::shared_ptr<context_type> context() const { return context_; }
        [[nodiscard]] constexpr std::size_t max_size() const noexcept {
            return std::numeric_limits<std::size_t>::max() / sizeof(T);
        }
        [[nodiscard]] T* allocate(std::size_t count) {
            if (count > max_size()) throw std::bad_array_new_length();
            if (!count) return nullptr;
            return context_->retrieve_block(count).head.get();
        }
        void deallocate(T* pointer, std::size_t count) noexcept {
            if (pointer) context_->release_owned(pointer, count);
        }
        template <class U, class... Args> void construct(U* pointer, Args&&... args) {
            if constexpr (std::is_same_v<U, T>) context_->construction_context()->construct(pointer, 1, std::forward<Args>(args)...);
            else std::construct_at(pointer, std::forward<Args>(args)...);
        }
        template <class U> void destroy(U* pointer) {
            if constexpr (std::is_same_v<U, T>) context_->construction_context()->destroy(pointer);
            else std::destroy_at(pointer);
        }
        template <class U> bool operator==(const ObjectPoolAllocator<U>&) const noexcept { return true; }
    };
}
