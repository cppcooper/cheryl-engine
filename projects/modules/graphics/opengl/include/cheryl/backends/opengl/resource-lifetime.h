#pragma once

#include <backends/opengl/gl.h>
#include <core/diagnostics.h>

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <thread>
#include <vector>

namespace CE::Rendering::OpenGL {
    namespace Detail {
        struct LifetimeAccess;
    }

    enum class ResourceKind { Texture, Buffer, VertexArray, Program, ShaderStage, Sampler };

    struct ResourceStats {
        ::CE::Diagnostics::DomainId domain = 0;
        std::uint64_t tracked = 0;
        std::uint64_t live = 0;
        std::uint64_t pending = 0;
        std::uint64_t deleted = 0;
        std::uint64_t abandoned = 0;
        bool active = true;
    };

    // All GL calls happen on the context thread. Asset destructors may run on another
    // thread, so they only mark their handles for deletion here.
    class ResourceLifetime final {
        static constexpr std::size_t none = std::numeric_limits<std::size_t>::max();
        struct Entry {
            ::CE::Rendering::OpenGL::ResourceKind kind;
            GLuint id;
            std::size_t next = none;
            bool pending = false;
        };

        mutable std::mutex mutex_;
        // Retain an injected allocator until the entry vector is destroyed.
        std::shared_ptr<std::pmr::memory_resource> entry_memory_;
        std::pmr::vector<Entry> entries_;
        std::size_t pending_ = none;
        std::size_t free_ = none;
        std::thread::id owner_;
        std::function<bool()> is_current_;
        bool active_ = true;
        ::CE::Rendering::OpenGL::ResourceStats diagnostics_{::CE::Diagnostics::next_domain_id()};

    public:
        // The predicate borrows a context that must outlive this active lifetime.
        // It is queried only on the owner thread, and must not reenter the lifetime.
        ResourceLifetime(std::thread::id owner, std::function<bool()> is_current);

        [[nodiscard]] std::size_t track(::CE::Rendering::OpenGL::ResourceKind kind, GLuint id);
        void retire(std::size_t slot) noexcept;
        void collect();
        // Only for a native ID not adopted by track(). Failure cleanup must not
        // delete on a foreign/missing context or replace the original exception.
        void discard_untracked(::CE::Rendering::OpenGL::ResourceKind kind, GLuint id) noexcept;
        void shutdown();
        // Failure fallback: invalidate handles without issuing calls to an unavailable context.
        // The platform's context destruction releases any remaining native resources.
        void abandon() noexcept;
        void require_owner() const;
        void require_current() const;
        // Snapshot under the lifetime lock; no logging or native calls.
        [[nodiscard]] ::CE::Rendering::OpenGL::ResourceStats diagnostics() const;

    private:
        friend struct ::CE::Rendering::OpenGL::Detail::LifetimeAccess;
        ResourceLifetime(std::thread::id owner, std::function<bool()> is_current, std::shared_ptr<std::pmr::memory_resource> entry_memory);
        static void delete_handle(::CE::Rendering::OpenGL::ResourceKind kind, GLuint id) noexcept;
        void require_owner_locked() const;
        void require_current_locked() const;
    };

    // Move-only registration. Its destructor never calls OpenGL.
    class Handle final {
        std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime_;
        std::size_t slot_ = std::numeric_limits<std::size_t>::max();
        GLuint id_ = 0;
        ::CE::Rendering::OpenGL::ResourceKind kind_ = ::CE::Rendering::OpenGL::ResourceKind::Texture;

    public:
        Handle() = default;
        Handle(std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime, ::CE::Rendering::OpenGL::ResourceKind kind, GLuint id);
        ~Handle() { reset(); }

        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        Handle(Handle&& other) noexcept;
        Handle& operator=(Handle&& other) noexcept;

        [[nodiscard]] GLuint id() const;
        [[nodiscard]] ::CE::Rendering::OpenGL::ResourceKind kind() const noexcept { return kind_; }
        // Identity only: this does not query a context or authorize native use.
        [[nodiscard]] const ::CE::Rendering::OpenGL::ResourceLifetime* resource_domain() const noexcept { return lifetime_.get(); }

    private:
        void reset() noexcept;
    };
}
