#pragma once

#include <backends/opengl/resource-lifetime.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <memory_resource>
#include <thread>
#include <utility>

namespace CE::RenderAPIs::ResourceDetail {
    struct LifetimeAccess {
        static std::shared_ptr<OpenGLResourceLifetime>
        create(std::thread::id owner, std::function<bool()> is_current, std::shared_ptr<std::pmr::memory_resource> entry_memory);
        static std::size_t capacity(const OpenGLResourceLifetime& lifetime);
        static std::size_t size(const OpenGLResourceLifetime& lifetime);
    };

    // allocate_shared retains a rebound allocator in its control block. Own the
    // memory resource there so even the last weak owner can safely deallocate it.
    template <typename T> struct RetainedMemoryAllocator {
        using value_type = T;
        std::shared_ptr<std::pmr::memory_resource> memory;

        explicit RetainedMemoryAllocator(std::shared_ptr<std::pmr::memory_resource> memory) noexcept
        : memory(std::move(memory)) {}

        template <typename U>
        RetainedMemoryAllocator(const RetainedMemoryAllocator<U>& other) noexcept
        : memory(other.memory) {}

        T* allocate(const std::size_t count) { return std::pmr::polymorphic_allocator<T>{memory.get()}.allocate(count); }

        void deallocate(T* pointer, const std::size_t count) noexcept {
            std::pmr::polymorphic_allocator<T>{memory.get()}.deallocate(pointer, count);
        }

        template <typename U> bool operator==(const RetainedMemoryAllocator<U>& other) const noexcept { return memory == other.memory; }
    };
}
