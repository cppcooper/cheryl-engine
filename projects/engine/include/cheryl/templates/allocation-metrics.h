#pragma once
#include <atomic>
#include <cstddef>
#include <memory>

namespace CE::Memory::Detail {
    // Attribution to the calling thread, including while another thread computes.
    struct ThreadAllocationCounts {
        std::size_t backing_calls = 0, backing_frees = 0, metadata_calls = 0, metadata_frees = 0;
        std::size_t control_calls = 0, control_frees = 0, checkouts = 0;
        bool operator==(const ThreadAllocationCounts&) const = default;
    };
    inline thread_local ThreadAllocationCounts thread_allocations;
    // Allocation requests, not allocator arena overhead or resident memory.
    // Independent atomics are descriptive counters, not a transactional snapshot.
    struct AllocationSnapshot {
        std::size_t calls, frees, bytes, live, peak;
    };
    struct AllocationCounter {
        std::atomic<std::size_t> calls{0}, frees{0}, bytes{0}, live{0}, peak{0};
        void acquired(std::size_t n) noexcept {
            calls.fetch_add(1, std::memory_order_relaxed);
            bytes.fetch_add(n, std::memory_order_relaxed);
            const auto current = live.fetch_add(n, std::memory_order_relaxed) + n;
            auto high = peak.load(std::memory_order_relaxed);
            while (current > high && !peak.compare_exchange_weak(high, current, std::memory_order_relaxed)) {}
        }
        void released(std::size_t n) noexcept {
            frees.fetch_add(1, std::memory_order_relaxed);
            live.fetch_sub(n, std::memory_order_relaxed);
        }
        AllocationSnapshot snapshot() const noexcept {
            return {calls.load(), frees.load(), bytes.load(), live.load(), peak.load()};
        }
    };
    inline AllocationCounter backing_allocations, metadata_allocations, control_allocations;
    inline std::atomic<std::size_t> byte_checkout_calls{0}, byte_checkout_requested_bytes{0};

    // Bookkeeping deliberately uses the standard backend, avoiding recursion into
    // the byte manager whose transactions these nodes implement.
    template <class T, bool Control = false> struct ObservedAllocator {
        using value_type = T;
        using is_always_equal = std::true_type;
        template <class U> struct rebind { using other = ObservedAllocator<U, Control>; };
        ObservedAllocator() = default;
        template <class U> ObservedAllocator(const ObservedAllocator<U, Control>&) noexcept {}
        static AllocationCounter& counter() noexcept {
            if constexpr (Control) return control_allocations;
            else return metadata_allocations;
        }
        T* allocate(std::size_t count) {
            auto* result = std::allocator<T>{}.allocate(count);
            counter().acquired(count * sizeof(T));
            if constexpr (Control) ++thread_allocations.control_calls;
            else ++thread_allocations.metadata_calls;
            return result;
        }
        void deallocate(T* p, std::size_t count) noexcept {
            std::allocator<T>{}.deallocate(p, count);
            counter().released(count * sizeof(T));
            if constexpr (Control) ++thread_allocations.control_frees;
            else ++thread_allocations.metadata_frees;
        }
        template <class U> bool operator==(const ObservedAllocator<U, Control>&) const noexcept { return true; }
    };
    template <class T> using MetadataAllocator = ObservedAllocator<T>;
    template <class T> using ControlAllocator = ObservedAllocator<T, true>;
}
