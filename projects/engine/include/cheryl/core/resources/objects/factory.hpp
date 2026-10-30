#pragma once
#include <internals/celog.h>
#include <unordered_map>
#include <vector>
#include <memory>
#include <type_traits>

namespace CE::Memory::Objects {
    /**
      * Allocates one contiguous batch and constructs each T separately. Each
      * returned handle destroys its object; the shared allocation owner calls
      * allocator deallocate(original, count) after the final handle is gone.
      */
    template <typename T, typename Allocator> struct Factory {
        static_assert(std::is_class_v<T>, "The Factory template is only for creating objects of classes.");
        /** Allocator traits used for allocation, per-object construction/destruction and final original-range deallocation. */
        using AAloc = std::allocator_traits<Allocator>;

        template <typename... Args> static std::vector<std::shared_ptr<T>> create(size_t N, Args... args);

        template <typename... Args> static void construct(T* p, size_t N, Args... args);

        static void destroy(T* p, size_t N = 1);

    private:
        static std::unordered_map<void*, bool> constructed;
    };

    /** Allocate one contiguous batch and construct independent per-object handles.
     * @param N Number of objects; zero returns an empty vector without allocation.
     * @param args Stored arguments reused as lvalues for each object's constructor.
     * @return Separate object owners retaining a shared allocation owner; only the
     * last one deallocates the original pointer/count through its copied allocator.
     * A later construction/handle allocation failure destroys completed objects and
     * releases the backing batch. Final deleters are noexcept: throwing destroy or
     * deallocate operations terminate. Object access itself is unsynchronized.
     */
    template <typename T, typename Allocator>
    template <typename... Args>
    std::vector<std::shared_ptr<T>> Factory<T, Allocator>::create(size_t N, Args... args) {
        std::vector<std::shared_ptr<T>> objects;
        objects.reserve(N);
        if (N == 0)
            return objects;

        Allocator allocator;
        auto* ptr = AAloc::allocate(allocator, N);
        // One shared owner retains the batch allocation, while each completed
        // element handle owns destruction of its individual object. If construction
        // fails, stack unwinding destroys completed elements before the final owner
        // releases the allocation.
        std::shared_ptr<T> allocation(ptr, [allocator, N](T* base) mutable noexcept { AAloc::deallocate(allocator, base, N); });
        for (std::size_t i = 0; i < N; ++i) {
            // Construct first, then attach a per-object handle retaining the
            // allocation owner until the final element handle disappears.
            auto* pi = ptr + i;
            AAloc::construct(allocator, pi, args...);
            objects.emplace_back(pi, [allocation, allocator](T* object) mutable noexcept { AAloc::destroy(allocator, object); });
        }
        return objects;
    }

    /** Construct N objects in caller-owned contiguous storage, rolling back the completed prefix on failure.
     * @param p Aligned writable storage for N unconstructed T objects, kept valid through rollback.
     * @param N Number of objects; zero does nothing.
     * @param args Stored arguments reused as lvalues for every constructor.
     * Constructor exceptions propagate after reverse destruction of the completed
     * prefix. This does not allocate, track slots or return storage; callers exclude
     * overlapping access and supply nonthrowing rollback destruction.
     */
    template <typename T, typename Allocator>
    template <typename... Args>
    void Factory<T, Allocator>::construct(T* p, size_t N, Args... args) {
        Allocator allocator;
        std::size_t constructed = 0;
        try {
            // The prefix length identifies exactly which constructors
            // succeeded if a later element throws.
            for (; constructed < N; ++constructed) {
                AAloc::construct(allocator, p + constructed, args...);
            }
        } catch (...) {
            while (constructed)
                AAloc::destroy(allocator, p + --constructed);
            throw;
        }
    }

    /** Destroy a contiguous live range without returning its backing allocation.
     * @param p First valid live object, borrowed through the call.
     * @param N Object count, one by default; zero does nothing.
     * Uses a default allocator instance and destroys in increasing address order;
     * a destructor failure propagates with earlier destructions already applied.
     * Callers synchronize access and ensure every selected slot is live.
     */
    template <typename T, typename Allocator> void Factory<T, Allocator>::destroy(T* p, size_t N) {
        Allocator allocator;
        for (std::size_t i = 0; i < N; ++i)
            AAloc::destroy(allocator, p + i);
    }
}
