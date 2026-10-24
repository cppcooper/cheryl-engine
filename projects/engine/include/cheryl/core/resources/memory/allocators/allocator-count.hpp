#pragma once

#include <internals/exceptions.h>
#include <cstddef>
#include <limits>

namespace CE::Memory::Allocators::Detail {
    // Limit both multiplication and addressable object ranges before touching storage.
    template <class T> constexpr std::size_t allocator_max_count() noexcept {
        return static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / sizeof(T);
    }
    template <class T> void check_allocator_count(std::size_t count) {
        if (count == 0 || count > allocator_max_count<T>())
            throw ::CE::Exceptions::bad_request(CE_HERE, "Allocator requires a positive addressable object count.");
    }
}
