#pragma once
#include <variant>
#include <cstring>
#include <internals/exceptions.h>
namespace CE::Memory::Pointers {
    /** Tests numeric membership in a half-open address interval.
     * @param start Inclusive address bound.
     * @param end Exclusive address bound; callers supply an ordered interval.
     * @param address Integer address to compare, without dereferencing it.
     * @return True exactly when start <= address and address < end.
     * These helpers provide no allocation, ownership or bounds/lifetime validation.
     * Arithmetic must be representable and derived pointers must remain within
     * their backing allocation. Alignment offsets require nonzero valid alignment.
     * @see [Utility contracts](cheryl-guide:runtime/utility-contracts)
     */
    inline bool is_in_range(const uintptr_t start, const uintptr_t end, const uintptr_t address) {
        return start <= address && address < end;
    }

    /** Tests pointer membership in a half-open comparable pointer range.
     * @param start Inclusive pointer bound.
     * @param end Exclusive pointer bound.
     * @param address Pointer to compare; no pointed-to object is accessed.
     * @return Result of the built-in ordered pointer comparisons.
     * @pre The pointers form a range for which those comparisons are meaningful;
     * this does not validate unrelated allocations or extend their lifetime.
     */
    inline bool is_in_range(const void* start, const void* end, const void* address) {
        return start <= address && address < end;
    }

    /** Adds a byte count to an integer address without bounds or overflow checking.
     * @param address Starting address value, not an owned allocation.
     * @param offset_bytes Forward displacement in bytes; addition must be representable.
     * @return Offset numeric address. No storage is read or allocated.
     */
    inline std::uintptr_t offset_address(const uintptr_t address, const std::size_t offset_bytes) {
        return address + offset_bytes;
    }

    /** Converts a pointer to an integer address and adds an unchecked byte offset.
     * @param ptr Borrowed starting pointer; no dereference or ownership transfer.
     * @param offset_bytes Byte displacement, not a count of pointed-to objects.
     * @return Numeric address. Caller ensures representability and valid backing range.
     */
    inline std::uintptr_t offset_address(void* ptr, const std::size_t offset_bytes) {
        return offset_address(reinterpret_cast<uintptr_t>(ptr), offset_bytes);
    }

    /** Forms a typed pointer by address arithmetic; constructs no object.
     * @tparam T Result pointee type; this does not multiply the offset by sizeof(T).
     * @param ptr Borrowed starting storage address.
     * @param offset_bytes Forward byte displacement within valid backing storage.
     * @return Borrowed pointer. The caller ensures address range, alignment, object
     * lifetime and representability before dereferencing it.
     */
    template <class T> T* add_offset(void* ptr, const std::size_t offset_bytes) {
        return reinterpret_cast<T*>(offset_address(ptr, offset_bytes));
    }

    /** Computes bytes to the next aligned address, returning zero if already aligned.
     * @param ptr Numeric starting position; no object is accessed.
     * @param alignment Nonzero valid byte alignment; not checked before division.
     * @return Forward byte padding in [0, alignment).
     */
    inline std::size_t get_alignment_offset(void* ptr, std::align_val_t alignment) {
        const auto av = static_cast<std::size_t>(alignment);
        const auto address = reinterpret_cast<uintptr_t>(ptr);
        return (av - (address % av)) % av;
    }

    /** Computes forward padding required by alignof(T).
     * @tparam T Complete type whose alignment is requested.
     * @param ptr Borrowed storage address; no allocation or object construction.
     * @return Byte padding, zero for an already aligned address.
     */
    template <typename T> std::size_t get_alignment_offset(void* ptr) {
        return get_alignment_offset(ptr, std::align_val_t{alignof(T)});
    }

    /** Rounds a forward byte offset up so the resulting address is aligned.
     * @param ptr Borrowed storage base.
     * @param offset_bytes Initial forward byte displacement.
     * @param alignment Nonzero valid alignment of the final address.
     * @return Initial displacement plus required padding, in bytes.
     * @pre Both additions are representable and stay within the backing allocation.
     */
    inline std::size_t align_offset(void* ptr, std::size_t offset_bytes, std::align_val_t alignment) {
        return offset_bytes + get_alignment_offset(add_offset<void>(ptr, offset_bytes), alignment);
    }

    /** Returns the least-significant set address bit as a numeric alignment.
     * @param address Integer address to inspect without dereferencing it.
     * @return Largest power of two dividing address, or zero for address zero.
     * Zero is not valid input to the alignment-offset helpers.
     */
    inline std::size_t calculate_alignment(uintptr_t address) {
        // Calculate the alignment by finding the largest power of  two that divides the address
        return address & (~address + 1);
    }

    /** Reports the address-derived alignment of a borrowed pointer.
     * @param ptr Pointer whose numeric address is inspected; null produces zero.
     * @return Alignment value, not a guarantee about allocation size or object validity.
     */
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

    /** Hashes the pointer address, never the pointed-to bytes.
     * @param ptr Pointer address, borrowed and not dereferenced; may be null.
     * @param hash_size_bytes Requested variant width: exactly 1, 2, 4 or 8 bytes.
     * @return Corresponding unsigned variant alternative. Smaller results reduce
     * the FNV-1a value modulo their unsigned maximum, excluding that maximum;
     * the eight-byte result retains all hash bits.
     * @throws CE::Exceptions::invalid_args For every other requested width.
     * Collisions are possible. Address reuse and process relocation make this
     * unsuitable as a persistent identity or object-lifetime token.
     */
    inline std::variant<uint8_t, uint16_t, uint32_t, uint64_t> pointer_to_hash(void* ptr, size_t hash_size_bytes) {
        // Convert the pointer to an integer representation
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);

        // Apply FNV-1a hash
        const uint64_t fnv_prime = 0x100000001b3;
        uint64_t hash = 0xcbf29ce484222325; // FNV offset basis

        // Hash each byte of the pointer address
        for (size_t i = 0; i < sizeof(addr); ++i) {
            uint8_t byte = (addr >> (i * 8)) & 0xFF;
            hash ^= byte;
            hash *= fnv_prime;
        }

        // Apply modulus to fit the hash into the requested size
        switch (hash_size_bytes) {
            case 1:
                return static_cast<uint8_t>(hash % UINT8_MAX);
            case 2:
                return static_cast<uint16_t>(hash % UINT16_MAX);
            case 4:
                return static_cast<uint32_t>(hash % UINT32_MAX);
            case 8:
                return hash; // No need for modulus, 64 bits fit directly
            default:
                throw ::CE::Exceptions::invalid_args(CE_HERE, "Invalid hash size. Supported sizes: 1, 2, 4, or 8 bytes.");
        }
    }
}
