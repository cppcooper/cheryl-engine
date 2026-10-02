#pragma once

#include <atomic>
#include <cstddef>
#include <memory_resource>
#include <new>

namespace CE::Testing {
    // Scoped rejection of a real memory-resource request; no global new override.
    struct FailingMemoryResource final : std::pmr::memory_resource {
        std::atomic<std::size_t> requests = 0;
        std::atomic<std::size_t> reject_request = 0;
        std::atomic<std::size_t> rejected = 0;
        std::atomic<std::size_t> outstanding = 0;

        void reject_next() noexcept { reject_request = requests.load() + 1; }

    private:
        void* do_allocate(
            const std::size_t bytes,
            const std::size_t alignment
        ) override {
            const auto request = ++requests;
            if (request == reject_request.load()) {
                ++rejected;
                throw std::bad_alloc{};
            }
            auto* result = std::pmr::new_delete_resource()->allocate(bytes, alignment);
            ++outstanding;
            return result;
        }

        void do_deallocate(
            void* pointer,
            const std::size_t bytes,
            const std::size_t alignment
        ) override {
            --outstanding;
            std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
        }

        bool do_is_equal(
            const std::pmr::memory_resource& other
        ) const noexcept override {
            return this == &other;
        }
    };
}
