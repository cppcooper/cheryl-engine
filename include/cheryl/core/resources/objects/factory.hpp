#pragma once
#ifndef OFACTORYH
#define OFACTORYH
#include <internals/celog.h>
#include <unordered_map>
#include <vector>
#include <memory>
#include <type_traits>

namespace CE::Obj {
     template<typename T, typename Allocator>
     struct Factory {
         static_assert(std::is_class_v<T>, "The Factory template is only for creating objects of classes.");
         using AAloc = std::allocator_traits<Allocator>;

         // creates N T objects with Allocator (always constructs)
         template<typename... Args>
         static std::vector<std::shared_ptr<T>> create(size_t N, Args... args);

		 // uses Allocator's traits::construct(alloc, p, args...)
         template<typename... Args>
         static void construct(T* p, size_t N, Args... args);

         // uses Allocator's traits::destroy(alloc, p)
         static void destroy(T* p, size_t N = 1);

     private:
         static std::unordered_map<void*,bool> constructed;
     };

     template<typename T, typename Allocator>
     template<typename... Args>
     std::vector<std::shared_ptr<T>> Factory<T, Allocator>::create(size_t N, Args... args) {
         if (N == 0) {
             CELog::critical("Less than 1 object is not possible, be reasonable sir.");
             return {nullptr};
         }
         auto ptr = AAloc::allocate(N);
         Factory::construct(ptr, N, std::forward<Args>(args)...);
         std::vector<std::shared_ptr<T>> objects;
         objects.reserve(N);
         for (int i = 0; i < N; ++i) {
             auto pi = ptr + i;
             objects.push_back(std::shared_ptr<T>(pi, [](const T* p) {
                 AAloc::destroy(p);
                 AAloc::deallocate(p, 1);
             }));
         }
         return objects;
     }

     template<typename T, typename Allocator>
     template<typename ... Args>
     void Factory<T, Allocator>::construct(T* p, size_t N, Args... args) {
         static_assert(std::is_constructible_v<T, Args...>, "A constructor for type T with the arguments provided does not exist.");
         AAloc::construct(p, N, std::forward<Args>(args)...);
     }

     template<typename T, typename Allocator>
     void Factory<T, Allocator>::destroy(T* p, size_t N) {
         AAloc::destroy(p, N);
     }
}
#endif
