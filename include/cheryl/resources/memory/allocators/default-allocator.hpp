#pragma once
#include <resources/memory/mem-mgr.h>

namespace CE::Mem {
    template<class T>
    struct DefaultAllocator final : std::allocator<T> {
        using value_type = T;
        using MM = Manager<2.0,4096>;
        T* allocate(std::size_t N) {
            auto b = MM::get().checkout_chunk(sizeof(T)*N, alignof(T), Enum::exact);
            return static_cast<T*>(b.head.get());
        }
        void deallocate(T *ptr, std::size_t N) {
            MM::get().return_portion(ptr, N);
        }
    };
}

template<typename T>
struct std::allocator_traits<CE::Mem::DefaultAllocator<T>> : std::allocator_traits<std::allocator<T>> {
    using allocator_type = CE::Mem::DefaultAllocator<T>;
    using value_type = typename allocator_type::value_type;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using void_pointer = void*;
    using const_void_pointer = const void*;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using is_always_equal = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_copy_assignment = std::false_type;

    // Allocate memory for n objects
    static pointer allocate(allocator_type& alloc, size_type n) {
        return alloc.allocate(n);
    }

    // Deallocate memory for n objects
    static void deallocate(allocator_type& alloc, pointer p, size_type n) {
        alloc.deallocate(p, n);
    }

    // Allocate memory for n objects
    static pointer allocate(size_type n) {
        allocator_type alloc;
        return alloc.allocate(n);
    }

    // Deallocate memory for n objects
    static void deallocate(pointer p, size_type n) {
        allocator_type alloc;
        alloc.deallocate(p, n);
    }

    // Construct an object of type T at the given location
    template<typename... Args>
    static void construct(allocator_type& alloc, T* p, Args&&... args) {
        std::construct_at(p, std::forward<Args>(args)...);
    }

    // Construct an object of type T at the given location
    template<typename... Args>
    static void construct(T* p, Args&&... args) {
        std::construct_at(p, std::forward<Args>(args)...);
    }

    // Destroy an object of type T at the given location
    static void destroy(allocator_type& alloc, T* p) {
        std::destroy_at(p);
    }

    // Destroy an object of type T at the given location
    static void destroy(T* p) {
        std::destroy_at(p);
    }
};
