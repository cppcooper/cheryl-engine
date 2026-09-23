#pragma once
#include <unordered_map>
#include <memory>

namespace CE::Obj {
    template <typename T>
    struct ObjCtor {
        template <typename... Args>
        static void construct(T* p, std::size_t N, Args... args) {
            static_assert(std::is_constructible_v<T, Args...>,
                          "A constructor for type T with the arguments provided does not exist.");
            for (std::size_t i = 0; i < N; ++i) {
                auto pi = p + i;
                // Reserve the tracking entry before constructing the object:
                // map growth must not throw after T has become live.
                auto [entry, inserted] = constructed.try_emplace(pi, false);
                if (entry->second) {
                    std::destroy_at(pi);
                    entry->second = false;
                }
                std::construct_at(pi, std::forward<Args>(args)...);
                entry->second = true;
            }
        }
        static void destroy(T* p, std::size_t N = 1) {
            for (std::size_t i = 0; i < N; ++i) {
                auto pi = p + i;
                if (constructed.count(pi) && constructed[pi]) {
                    std::destroy_at(pi);
                    constructed[pi] = false;
                }
            }
        }
        static void erase(T* start, T* end) {
            auto current = start;
            while (current < end) {
                constructed.erase(current++);
            }
        }

    protected:
        static std::unordered_map<void*, bool> constructed;
    };

    template <typename T>
    std::unordered_map<void*, bool> ObjCtor<T>::constructed;
}
