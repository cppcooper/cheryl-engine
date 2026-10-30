#pragma once

/** @file
 * @brief DefaultAllocator storage adapter and its allocator_traits specialization.
 * @see [CPU memory lifetime](cheryl-guide:resources/resource-lifetime)
 */
#include <core/resources/memory/mem-mgr.h>

namespace CE::Memory::Allocators {
    /**
     * Adapts the process-wide Manager<2.0, 4096> byte domain to unconstructed T storage. Exact checkout
     * requests do not add growth, although a reusable range may be larger. This allocator retains no
     * independent release context: finish allocation users and deallocation before its singleton manager
     * is destroyed.
     * @tparam T Complete non-void object type; its alignment and size define checkout.
     */
    template <class T> struct DefaultAllocator final : std::allocator<T> {
        using value_type = T; ///< Object type counted by allocate/deallocate.
        using MM = ::CE::Memory::Manager<2.0, 4096>; ///< Shared byte-manager specialization used by this adapter.
        using manager_type = MM; ///< Byte-domain owner; no object construction tracking.
        T* allocate(std::size_t N) {
            // Request the caller's exact byte count; the manager may still
            // serve it from an existing larger reusable range.
            auto b = MM::get().checkout_chunk(sizeof(T) * N, alignof(T), ::CE::Memory::exact);
            return static_cast<T*>(b.head.get());
        }
        void deallocate(T* ptr, std::size_t /*N*/) { MM::get().return_ptr(ptr); }
    };
}

/** DefaultAllocator traits: storage calls count objects; construction/destruction is separate from checkout/return.
 * The overloads accepting an allocator use that instance. Instance-free operations
 * obtain the corresponding singleton-backed adapter/context. Exceptions from the
 * underlying storage and user constructors/destructors propagate.
 */
template <typename T>
struct std::allocator_traits<::CE::Memory::Allocators::DefaultAllocator<T>> : std::allocator_traits<std::allocator<T>> {
    using allocator_type = ::CE::Memory::Allocators::DefaultAllocator<T>; ///< Adapter served by these traits.
    using value_type = typename allocator_type::value_type; ///< Allocated object type.
    using pointer = value_type*; ///< Pointer into unconstructed or caller-managed object storage.
    using const_pointer = const value_type*; ///< Read-only object-storage pointer.
    using void_pointer = void*; ///< Untyped mutable storage pointer.
    using const_void_pointer = const void*; ///< Untyped read-only storage pointer.
    using size_type = std::size_t; ///< Unsigned object count, not byte count.
    using difference_type = std::ptrdiff_t; ///< Signed pointer-distance type.

    using is_always_equal = std::true_type; ///< Traits declare allocator instances interchangeable within their shared domain.
    using propagate_on_container_swap = std::true_type; ///< Container swaps propagate the allocator.
    using propagate_on_container_move_assignment = std::true_type; ///< Container move assignment propagates the allocator.
    using propagate_on_container_copy_assignment = std::false_type; ///< Container copy assignment retains its allocator.

    static pointer allocate(allocator_type& alloc, size_type n) { return alloc.allocate(n); }

    static void deallocate(allocator_type& alloc, pointer p, size_type n) { alloc.deallocate(p, n); }

    static pointer allocate(size_type n) {
        allocator_type alloc;
        return alloc.allocate(n);
    }

    static void deallocate(pointer p, size_type n) {
        allocator_type alloc;
        alloc.deallocate(p, n);
    }

    template <typename... Args> static void construct(allocator_type& alloc, T* p, Args&&... args) {
        std::construct_at(p, std::forward<Args>(args)...);
    }

    template <typename... Args> static void construct(T* p, Args&&... args) { std::construct_at(p, std::forward<Args>(args)...); }

    static void destroy(allocator_type& alloc, T* p) { std::destroy_at(p); }

    static void destroy(T* p) { std::destroy_at(p); }
};

/**
 * @fn template<typename T> T* CE::Memory::Allocators::DefaultAllocator<T>::allocate(std::size_t N)
 * @brief Check out raw aligned bytes for N objects without constructing T.
 * @param N Positive object count; sizeof(T)*N must be representable in size_t.
 * @return Pointer to at least N unconstructed T slots; underlying manager alignment is at least 64 bytes.
 * Zero requests throw bad_request; underlying growth/allocation errors propagate. Multiplication overflow is not checked by this adapter.
 */

/**
 * @fn template<typename T> void CE::Memory::Allocators::DefaultAllocator<T>::deallocate(T* ptr, std::size_t)
 * @brief Return the complete checked-out byte section containing ptr; no T destructor runs.
 * @param ptr Pointer in the active allocation after all its users/lifetimes finish.
 * The size argument is ignored; this adapter returns the complete containing section, including any grown tail.
 * Unknown or already-pooled pointers are ignored by return_ptr(). Caller must not use this operation for independent subrange returns.
 */

/**
 * @fn template<typename T> typename std::allocator_traits<::CE::Memory::Allocators::DefaultAllocator<T> >::pointer std::allocator_traits<::CE::Memory::Allocators::DefaultAllocator<T> >::allocate(allocator_type& alloc, size_type n)
 * @brief Delegate raw-storage checkout to alloc.allocate(n).
 * @param alloc Adapter whose domain/context serves the request.
 * @param n Positive T count, with the adapter's size restrictions.
 * @return Unconstructed storage; underlying checkout failures propagate.
 */

/**
 * @fn template<typename T> typename std::allocator_traits<::CE::Memory::Allocators::DefaultAllocator<T> >::pointer std::allocator_traits<::CE::Memory::Allocators::DefaultAllocator<T> >::allocate(size_type n)
 * @brief Default-construct an adapter and check out n unconstructed objects' storage.
 * @param n Positive T count; the adapter's size and failure rules apply.
 * @return Raw aligned pointer; this operation does not construct T.
 */

/**
 * @fn template<typename T> void std::allocator_traits<::CE::Memory::Allocators::DefaultAllocator<T> >::deallocate(allocator_type& alloc, pointer p, size_type n)
 * @brief Delegate storage return to alloc.deallocate(p, n) after object lifetimes end.
 * @param alloc Adapter compatible with the allocation's domain/context.
 * @param p Original allocation or a subrange allowed by that adapter.
 * @param n Object count; ignored by byte adapters, exact for ObjectPoolAllocator.
 * No object destruction occurs; the adapter's return validation and exceptions apply.
 */

/**
 * @fn template<typename T> void std::allocator_traits<::CE::Memory::Allocators::DefaultAllocator<T> >::deallocate(pointer p, size_type n)
 * @brief Return storage through a default-constructed adapter.
 * @param p Pointer allocated in that same singleton-backed domain.
 * @param n Object count interpreted by the adapter.
 * End all object lifetimes/users first. Byte adapters return the whole section; the pool adapter returns exactly n slots. The underlying return may throw.
 */

/**
 * @fn template<typename T> template<typename... Args> void std::allocator_traits<::CE::Memory::Allocators::DefaultAllocator<T> >::construct(allocator_type& alloc, T* p, Args&&... args)
 * @brief Construct one T directly at p using std::construct_at.
 * @param alloc Unused adapter; construction does not consult its manager.
 * @param p Aligned writable storage retained throughout the operation.
 * @param args Constructor arguments forwarded to T; construction exceptions propagate.
 * Storage is not allocated or returned by this operation.
 * The destination must not already contain a live T requiring destruction; no engine lifetime tracking is added.
 */

/**
 * @fn template<typename T> template<typename... Args> void std::allocator_traits<::CE::Memory::Allocators::DefaultAllocator<T> >::construct(T* p, Args&&... args)
 * @brief Construct one T directly at p using std::construct_at.
 * @param p Aligned writable storage retained throughout the operation.
 * @param args Constructor arguments forwarded to T; construction exceptions propagate.
 * Storage is not allocated or returned by this operation.
 * The destination must be available for a new lifetime; no engine lifetime tracking is added.
 */

/**
 * @fn template<typename T> void std::allocator_traits<::CE::Memory::Allocators::DefaultAllocator<T> >::destroy(allocator_type& alloc, T* p)
 * @brief Destroy one live T directly using std::destroy_at.
 * @param alloc Unused adapter.
 * @param p Object storage retained throughout destruction.
 * Does not deallocate backing storage. User destructor exceptions propagate.
 * The caller supplies a valid live object; no engine lifetime tracker prevents double destruction.
 */

/**
 * @fn template<typename T> void std::allocator_traits<::CE::Memory::Allocators::DefaultAllocator<T> >::destroy(T* p)
 * @brief Destroy one live T directly using std::destroy_at.
 * @param p Object storage retained throughout destruction.
 * Does not deallocate backing storage. User destructor exceptions propagate.
 * The caller supplies a valid live object; no engine lifetime tracker prevents double destruction.
 */
