#pragma once
#include <internals/celog.h>
#include <unordered_map>
#include <vector>
#include <memory>
#include <type_traits>

namespace CE::Obj {
     /**
      * Allocates one contiguous batch and constructs each T separately. Each
      * returned handle destroys its object; the shared allocation owner calls
      * allocator deallocate(original, count) after the final handle is gone.
      */
     template<typename T, typename Allocator>
     struct Factory {
         static_assert(std::is_class_v<T>, "The Factory template is only for creating objects of classes.");
         using AAloc = std::allocator_traits<Allocator>;

         /** Allocate one batch and return a handle for each constructed object. */
         template<typename... Args>
         static std::vector<std::shared_ptr<T>> create(size_t N, Args... args);

         /** Construct N objects in caller-owned storage; roll back on failure. */
         template<typename... Args>
         static void construct(T* p, size_t N, Args... args);

         /** Destroy N objects without releasing their backing allocation. */
         static void destroy(T* p, size_t N = 1);

     private:
         static std::unordered_map<void*,bool> constructed;
     };

     template<typename T, typename Allocator>
     template<typename... Args>
     std::vector<std::shared_ptr<T>> Factory<T, Allocator>::create(size_t N, Args... args) {
         std::vector<std::shared_ptr<T>> objects;
         objects.reserve(N);
         if (N == 0) return objects;

         Allocator allocator;
         auto* ptr = AAloc::allocate(allocator, N);
         // One allocator allocation is released once, after the last element
         // handle has destroyed its own object.
         std::shared_ptr<T> allocation(ptr, [allocator, N](T* base) mutable noexcept {
             AAloc::deallocate(allocator, base, N);
         });
         for (std::size_t i = 0; i < N; ++i) {
             auto* pi = ptr + i;
             AAloc::construct(allocator, pi, args...);
             objects.emplace_back(pi, [allocation, allocator](T* object) mutable noexcept {
                 AAloc::destroy(allocator, object);
             });
         }
         return objects;
     }

     template<typename T, typename Allocator>
     template<typename... Args>
     void Factory<T, Allocator>::construct(T* p, size_t N, Args... args) {
         Allocator allocator;
         std::size_t constructed = 0;
         try {
             for (; constructed < N; ++constructed) {
                 AAloc::construct(allocator, p + constructed, args...);
             }
         } catch (...) {
             while (constructed) AAloc::destroy(allocator, p + --constructed);
             throw;
         }
     }

     template<typename T, typename Allocator>
     void Factory<T, Allocator>::destroy(T* p, size_t N) {
         Allocator allocator;
         for (std::size_t i = 0; i < N; ++i) AAloc::destroy(allocator, p + i);
     }
}
