#pragma once
#include <resources/objects/pool.h>
#include <resources/objects/object-construction.hpp>

namespace CE::Mem {
    template<class T>
    struct ObjectPoolAllocator final : std::allocator<T> {
        using Base = std::allocator<T>;
        using value_type = T;

        // Override the allocate function
        T* allocate(std::size_t N) {
            auto b = Obj::Pool<T>::get().retrieve_block(N);
            return static_cast<T*>(b.head.get());
        }

        // Override the deallocate function
        void deallocate(T* ptr, std::size_t N) {
            Obj::Pool<T>::get().return_objects(ptr, N);
        }
    };
}

template<typename T>
struct std::allocator_traits<CE::Mem::ObjectPoolAllocator<T>> : std::allocator_traits<std::allocator<T>> {
    using allocator_type = CE::Mem::ObjectPoolAllocator<T>;
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
    using propagate_on_container_copy_assignment = std::true_type;

    // Allocate memory for n objects
    static pointer allocate(allocator_type& alloc, size_type n) {
        return alloc.allocate(n);
    }

    // Allocate memory for n objects
    static pointer allocate(size_type n) {
        allocator_type alloc;
        return alloc.allocate(n);
    }

    // Deallocate memory for n objects
    static void deallocate(allocator_type& alloc, pointer p, size_type n) {
        alloc.deallocate(p, n);
    }

    // Deallocate memory for n objects
    static void deallocate(pointer p, size_type n) {
        allocator_type alloc;
        alloc.deallocate(p, n);
    }

    // Construct N objects of type T at the given location
    template<typename... Args>
    static void construct(T* p, std::size_t N, Args&&... args) {
        CE::Obj::ObjCtor<T>::construct(p, N, std::forward<Args>(args)...);
    }

    // Construct an object of type T at the given location
    template<typename... Args>
    static void construct(allocator_type& alloc, T* p, Args&&... args) {
        CE::Obj::ObjCtor<T>::construct(p, 1ull, std::forward<Args>(args)...);
    }

    // Construct an object of type T at the given location
    template<typename... Args>
    static void construct(T* p, Args&&... args) {
        CE::Obj::ObjCtor<T>::construct(p, 1ull, std::forward<Args>(args)...);
    }

    // Destroy an object of type T at the given location
    static void destroy(T* p, std::size_t N) {
        CE::Obj::ObjCtor<T>::destroy(p, N);
    }

    // Destroy an object of type T at the given location
    static void destroy(allocator_type& alloc, T* p) {
        CE::Obj::ObjCtor<T>::destroy(p);
    }

    // Destroy an object of type T at the given location
    static void destroy(T* p) {
        CE::Obj::ObjCtor<T>::destroy(p);
    }
};
