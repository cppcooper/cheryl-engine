#pragma once

namespace CE::ptr {
    // returns true if address >= start && address < end
    inline bool is_in_range(const uintptr_t start, const uintptr_t end, const uintptr_t address) {
        return start <= address && address < end;
    }

    // returns true if address >= start && address < end
    inline bool is_in_range(const void* start, const void* end, const void* address) {
        return start <= address && address < end;
    }

    // returns the sum of address and offset
    inline std::uintptr_t offset_address(const uintptr_t address, const std::size_t offset_bytes) {
        return address + offset_bytes;
    }

    // returns the address of ptr+offset
    inline std::uintptr_t offset_address(void* ptr, const std::size_t offset_bytes) {
        return offset_address(reinterpret_cast<uintptr_t>(ptr), offset_bytes);
    }

    // returns a pointer whose address was offset by <offset> bytes
    template<class T>
    T* add_offset(void* ptr, const std::size_t offset_bytes) {
        return reinterpret_cast<T*>(offset_address(ptr, offset_bytes));
    }

    // returns the offset from ptr to alignment
    inline std::size_t get_alignment_offset(void* ptr, std::align_val_t alignment) {
        const auto av = static_cast<std::size_t>(alignment);
        const auto address = reinterpret_cast<uintptr_t>(ptr);
        return av - (address % av);
    }

    // returns the offset from ptr to alignment for type <T>
    template <typename T>
    std::size_t get_alignment_offset(void* ptr) {
        return get_alignment_offset(ptr, std::align_val_t{alignof(T)});
    }

    // returns an aligned offset for ptr
    inline std::size_t align_offset(void* ptr, std::size_t offset_bytes, std::align_val_t alignment) {
        return offset_bytes + get_alignment_offset(add_offset<void>(ptr,offset_bytes), alignment);
    }

    // returns the alignment for address
    inline std::size_t calculate_alignment(uintptr_t address) {
        // Calculate the alignment by finding the largest power of two that divides the address
        return address & (~address + 1);
    }

    // returns the alignment for ptr
    inline std::align_val_t calculate_alignment(void* ptr) {
        // Calculate the alignment by finding the largest power of two that divides the address
        return static_cast<std::align_val_t>(calculate_alignment(reinterpret_cast<uintptr_t>(ptr)));
    }

    // // returns ptr aligned
    // inline void* align_ptr(void* ptr, const std::size_t alignment) {
    //     return add_offset<void>(ptr, get_alignment_offset(ptr, alignment));
    // }
    //
    // // returns ptr aligned as T*
    // template <typename T>
    // T* align_ptr(void* ptr) {
    //     return static_cast<T*>(align_ptr(ptr, alignof(T)));
    // }
}
