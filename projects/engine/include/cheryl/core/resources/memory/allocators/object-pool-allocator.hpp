#pragma once

/** @file
 * @brief ObjectPoolAllocator storage adapter and its allocator_traits specialization.
 * @see [CPU memory lifetime](cheryl-guide:resources/resource-lifetime)
 */
#include <core/resources/objects/pool.h>
#include <core/resources/objects/object-construction.hpp>

namespace CE::Memory::Allocators {
    /**
     * Adapts Pool<T> to an allocator and retains the PoolState captured at
     * construction. Standard deallocate receives the original allocation and
     * count; reservation handles return individual slots through that context.
     * Capturing the context keeps bookkeeping/backing alive across facade teardown;
     * callers still synchronize object access and finish lifetimes before returning slots.
     * @tparam T Complete non-void object type, counted in slots rather than bytes.
     */
    template <class T> struct ObjectPoolAllocator final : std::allocator<T> {
        using Base = std::allocator<T>; ///< Standard allocator base; overridden storage operations use the retained pool.
        using value_type = T; ///< Object type counted by allocate/deallocate.
        using manager_type = ::CE::Memory::Objects::Pool<T>; ///< Typed pool facade whose retained state serves this allocator.
        using context_type = typename manager_type::release_context_type; ///< Retained pool state used without another singleton lookup.

        ObjectPoolAllocator()
        : context_(manager_type::get().release_context()) {}
        [[nodiscard]] std::shared_ptr<context_type> context() const { return context_; }

            T* allocate(std::size_t N) {
            auto b = context_->retrieve_block(N);
            return static_cast<T*>(b.head.get());
        }

            void deallocate(T* ptr, std::size_t N) { context_->return_objects(ptr, N); }

    private:
        std::shared_ptr<context_type> context_;
    };
}

/** ObjectPoolAllocator traits: storage calls count objects; construction/destruction is separate from checkout/return.
 * The overloads accepting an allocator use that instance. Instance-free operations
 * obtain the corresponding singleton-backed adapter/context. Exceptions from the
 * underlying storage and user constructors/destructors propagate.
 */
template <typename T>
struct std::allocator_traits<::CE::Memory::Allocators::ObjectPoolAllocator<T>> : std::allocator_traits<std::allocator<T>> {
    using allocator_type = ::CE::Memory::Allocators::ObjectPoolAllocator<T>; ///< Adapter served by these traits.
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
    using propagate_on_container_copy_assignment = std::true_type; ///< Container copy assignment propagates the allocator.

    static pointer allocate(allocator_type& alloc, size_type n) { return alloc.allocate(n); }

    static pointer allocate(size_type n) {
        allocator_type alloc;
        return alloc.allocate(n);
    }

    static void deallocate(allocator_type& alloc, pointer p, size_type n) { alloc.deallocate(p, n); }

    static void deallocate(pointer p, size_type n) {
        allocator_type alloc;
        alloc.deallocate(p, n);
    }

    template <typename... Args> static void construct(T* p, std::size_t N, Args&&... args) {
        ::CE::Memory::Objects::Constructor<T>::construct(p, N, std::forward<Args>(args)...);
    }

    template <typename... Args> static void construct(allocator_type& alloc, T* p, Args&&... args) {
        alloc.context()->construction_context()->construct(p, 1ull, std::forward<Args>(args)...);
    }

    template <typename... Args> static void construct(T* p, Args&&... args) {
        ::CE::Memory::Objects::Constructor<T>::construct(p, 1ull, std::forward<Args>(args)...);
    }

    static void destroy(T* p, std::size_t N) { ::CE::Memory::Objects::Constructor<T>::destroy(p, N); }

    static void destroy(allocator_type& alloc, T* p) { alloc.context()->construction_context()->destroy(p); }

    static void destroy(T* p) { ::CE::Memory::Objects::Constructor<T>::destroy(p); }
};

/**
 * @fn template<typename T> CE::Memory::Allocators::ObjectPoolAllocator<T>::ObjectPoolAllocator()
 * @brief Capture the current Pool<T> release context; allocation/initialization errors propagate. Copies retain that same context; it may outlive the facade.
 */

/**
 * @fn template<typename T> std::shared_ptr<typename CE::Memory::Allocators::ObjectPoolAllocator<T>::context_type> CE::Memory::Allocators::ObjectPoolAllocator<T>::context() const
 * @return Shared pool state retained independently of this allocator. It owns bookkeeping/backing, not a promise of synchronized object access.
 */

/**
 * @fn template<typename T> T* CE::Memory::Allocators::ObjectPoolAllocator<T>::allocate(std::size_t N)
 * @brief Check out raw storage from the retained typed pool.
 * @param N Positive object count; zero throws bad_request.
 * @return Aligned pointer to at least N unconstructed slots. Underlying growth may reserve more storage.
 * Size overflow or allocation failures propagate. Construct objects separately and return only the intended slot range.
 */

/**
 * @fn template<typename T> void CE::Memory::Allocators::ObjectPoolAllocator<T>::deallocate(T* ptr, std::size_t N)
 * @brief Return exactly N slots through the captured pool state.
 * @param ptr First slot in a current checked-out range, with all users and object lifetimes finished.
 * @param N Positive in-bounds object count.
 * Invalid, unknown or already-returned ranges throw bad_request; no destructor runs. A partial slot return preserves the rest of the range.
 */

/**
 * @fn template<typename T> typename std::allocator_traits<::CE::Memory::Allocators::ObjectPoolAllocator<T> >::pointer std::allocator_traits<::CE::Memory::Allocators::ObjectPoolAllocator<T> >::allocate(allocator_type& alloc, size_type n)
 * @brief Delegate raw-storage checkout to alloc.allocate(n).
 * @param alloc Adapter whose domain/context serves the request.
 * @param n Positive T count, with the adapter's size restrictions.
 * @return Unconstructed storage; underlying checkout failures propagate.
 */

/**
 * @fn template<typename T> typename std::allocator_traits<::CE::Memory::Allocators::ObjectPoolAllocator<T> >::pointer std::allocator_traits<::CE::Memory::Allocators::ObjectPoolAllocator<T> >::allocate(size_type n)
 * @brief Default-construct an adapter and check out n unconstructed objects' storage.
 * @param n Positive T count; the adapter's size and failure rules apply.
 * @return Raw aligned pointer; this operation does not construct T.
 */

/**
 * @fn template<typename T> void std::allocator_traits<::CE::Memory::Allocators::ObjectPoolAllocator<T> >::deallocate(allocator_type& alloc, pointer p, size_type n)
 * @brief Delegate storage return to alloc.deallocate(p, n) after object lifetimes end.
 * @param alloc Adapter compatible with the allocation's domain/context.
 * @param p Original allocation or a subrange allowed by that adapter.
 * @param n Object count; ignored by byte adapters, exact for ObjectPoolAllocator.
 * No object destruction occurs; the adapter's return validation and exceptions apply.
 */

/**
 * @fn template<typename T> void std::allocator_traits<::CE::Memory::Allocators::ObjectPoolAllocator<T> >::deallocate(pointer p, size_type n)
 * @brief Return storage through a default-constructed adapter.
 * @param p Pointer allocated in that same singleton-backed domain.
 * @param n Object count interpreted by the adapter.
 * End all object lifetimes/users first. Byte adapters return the whole section; the pool adapter returns exactly n slots. The underlying return may throw.
 */

/**
 * @fn template<typename T> template<typename... Args> void std::allocator_traits<::CE::Memory::Allocators::ObjectPoolAllocator<T> >::construct(T* p, std::size_t N, Args&&... args)
 * @brief Construct or replace N tracked slots through Constructor<T>.
 * @param p Contiguous storage for N T slots.
 * @param N Object count; zero does nothing.
 * @param args Repeatedly forwarded arguments, possibly moved-from for later elements.
 * Busy conflicts and user exceptions propagate. Earlier successful slots remain live; the failing slot is marked raw. No batch rollback or storage allocation occurs.
 */

/**
 * @fn template<typename T> void std::allocator_traits<::CE::Memory::Allocators::ObjectPoolAllocator<T> >::destroy(T* p, std::size_t N)
 * @brief Destroy N tracked slots through Constructor<T>.
 * @param p Contiguous storage retained until the call ends.
 * @param N Object count; zero does nothing.
 * Raw/untracked slots are skipped. Busy conflicts or user exceptions propagate after any completed earlier destructions; no batch rollback or storage return occurs.
 */

/**
 * @fn template<typename T> template<typename... Args> void std::allocator_traits<::CE::Memory::Allocators::ObjectPoolAllocator<T> >::construct(allocator_type& alloc, T* p, Args&&... args)
 * @brief Construct or replace one tracked slot using alloc's retained construction context.
 * @param alloc Allocator retaining the slot's pool context.
 * @param p Aligned writable storage retained throughout the operation.
 * @param args Constructor arguments forwarded to T; construction exceptions propagate.
 * Storage is not allocated or returned by this operation.
 * Busy-slot conflicts throw bad_request; constructor failure leaves the slot tracked as raw. Context operations synchronize lifetime bookkeeping, not object-data access.
 */

/**
 * @fn template<typename T> template<typename... Args> void std::allocator_traits<::CE::Memory::Allocators::ObjectPoolAllocator<T> >::construct(T* p, Args&&... args)
 * @brief Construct or replace one slot through Constructor<T>'s shared tracker.
 * @param p Aligned writable storage retained throughout the operation.
 * @param args Constructor arguments forwarded to T; construction exceptions propagate.
 * Storage is not allocated or returned by this operation.
 * A previous tracked live object is destroyed first. Busy conflicts throw bad_request; failure marks the slot raw.
 */

/**
 * @fn template<typename T> void std::allocator_traits<::CE::Memory::Allocators::ObjectPoolAllocator<T> >::destroy(allocator_type& alloc, T* p)
 * @brief Destroy a tracked live slot through alloc's retained construction context.
 * @param alloc Compatible allocator retaining lifetime bookkeeping.
 * @param p Object storage retained throughout destruction.
 * Does not deallocate backing storage. User destructor exceptions propagate.
 * Raw/untracked slots are skipped; a busy slot throws bad_request.
 */

/**
 * @fn template<typename T> void std::allocator_traits<::CE::Memory::Allocators::ObjectPoolAllocator<T> >::destroy(T* p)
 * @brief Destroy a tracked live slot through Constructor<T>'s shared tracker.
 * @param p Object storage retained throughout destruction.
 * Does not deallocate backing storage. User destructor exceptions propagate.
 * Raw/untracked slots are skipped; a busy slot throws bad_request.
 */
