#pragma once
#ifndef MEMMGRH
#define MEMMGRH

#include "typedefs.h"
#include <enums.h>
#include <templates/singleton.h>
#include <chrono>
#include <bit>

namespace CE::Mem {
    template<double growth_factor_ = 1.6, int32_t growth_base_ = 256>
    struct Manager : AbstractManager<void>, Singleton_CTS<Manager<growth_factor_, growth_base_>> {
        static_assert(growth_base_ >= 0, "The base growth should be a positive integer.");
        static_assert(growth_factor_ > 0, "The growth factor cannot be 0.");
        static_assert(growth_factor_ < 32, "Use a reasonable growth factor that is below 32.");

        Manager() = default;
        // retrieve stats
        [[nodiscard]] std::string stats();
        // return all ownership of a section containing ptr
        void return_ptr(void* ptr);
        // return a portion of a block
        void return_portion(void* ptr, std::size_t length);
        // return all ownership of a section - this argument is presumed to exist in `sections`
        void return_chunk(const HeapBlock& returned);
        // retrieve an allocation with the given length (+alignment) - returns a section of an allocation at least big enough to fill the request
        [[nodiscard]] HeapBlock checkout_chunk(
                size_t length, size_t alignment = 0, Enum::fitType fit = Enum::exact,
                size_t growth_base = growth_base_, double growth_factor = growth_factor_
        );

    private:
        // retrieve an allocation with the given length - returns a section of an allocation at least big enough to fill the request
        [[nodiscard]] static HeapBlock allocate(size_t length, std::align_val_t alignment) {
            alignment = static_cast<std::align_val_t>(std::bit_ceil(static_cast<std::size_t>(alignment)));
            auto deleter = [alignment](void* p) {
                ::operator delete[](p, alignment);
            };
            const auto ptr = std::shared_ptr<void>(::operator new[](length, alignment), deleter);
            return {ptr, ptr, ptr::calculate_alignment(ptr.get()), length};
        }

    public:
        // add (b1 * w2) new heap allocations aligned to a3
        void preallocate(size_t blocks, size_t width, size_t gb = growth_base_, double gf = growth_factor_, std::align_val_t alignment = std::align_val_t{64});

    };

    using ExactMMgr = Manager<1.0,0>;
    using CacheMMgr = Manager<4.0,4096>;
    template<typename T>
    using ObjMMgr = Manager<1.29,sizeof(T)*64>;
}

#include "mem-mgr.hpp"
#endif
