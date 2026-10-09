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

namespace CE::RenderAPIs {
    namespace ResourceDetail {
        struct LifetimeAccess;
    }

    enum class GLResourceKind { Texture, Buffer, VertexArray, Program, ShaderStage, Sampler };

    struct NativeResourceStats {
        Diagnostics::DomainId domain = 0;
        std::uint64_t tracked = 0;
        std::uint64_t live = 0;
        std::uint64_t pending = 0;
        std::uint64_t deleted = 0;
        std::uint64_t abandoned = 0;
        bool active = true;
    };

    // All GL calls happen on the context thread. Asset destructors may run on another
    // thread, so they only mark their handles for deletion here.
    class OpenGLResourceLifetime final {
        static constexpr std::size_t none = std::numeric_limits<std::size_t>::max();
        struct Entry {
            GLResourceKind kind;
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
        NativeResourceStats diagnostics_{Diagnostics::next_domain_id()};

    public:
        // The predicate borrows a context that must outlive this active lifetime.
        // It is queried only on the owner thread, and must not reenter the lifetime.
        OpenGLResourceLifetime(std::thread::id owner, std::function<bool()> is_current);

        [[nodiscard]] std::size_t track(GLResourceKind kind, GLuint id);
        void retire(std::size_t slot) noexcept;
        void collect();
        // Only for a native ID not adopted by track(). Failure cleanup must not
        // delete on a foreign/missing context or replace the original exception.
        void discard_untracked(GLResourceKind kind, GLuint id) noexcept;
        void shutdown();
        // Failure fallback: invalidate handles without issuing calls to an unavailable context.
        // The platform's context destruction releases any remaining native resources.
        void abandon() noexcept;
        void require_owner() const;
        void require_current() const;
        // Snapshot under the lifetime lock; no logging or native calls.
        [[nodiscard]] NativeResourceStats diagnostics() const;

    private:
        friend struct ResourceDetail::LifetimeAccess;
        OpenGLResourceLifetime(
            std::thread::id owner,
            std::function<bool()> is_current,
            std::shared_ptr<std::pmr::memory_resource> entry_memory
        );
        static void delete_handle(GLResourceKind kind, GLuint id) noexcept;
        void require_owner_locked() const;
        void require_current_locked() const;
    };

    // Move-only registration. Its destructor never calls OpenGL.
    class OpenGLHandle final {
        std::shared_ptr<OpenGLResourceLifetime> lifetime_;
        std::size_t slot_ = std::numeric_limits<std::size_t>::max();
        GLuint id_ = 0;
        GLResourceKind kind_ = GLResourceKind::Texture;

    public:
        OpenGLHandle() = default;
        OpenGLHandle(std::shared_ptr<OpenGLResourceLifetime> lifetime, GLResourceKind kind, GLuint id);
        ~OpenGLHandle() { reset(); }

        OpenGLHandle(const OpenGLHandle&) = delete;
        OpenGLHandle& operator=(const OpenGLHandle&) = delete;
        OpenGLHandle(OpenGLHandle&& other) noexcept;
        OpenGLHandle& operator=(OpenGLHandle&& other) noexcept;

        [[nodiscard]] GLuint id() const;
        [[nodiscard]] GLResourceKind kind() const noexcept { return kind_; }
        // Identity only: this does not query a context or authorize native use.
        [[nodiscard]] const OpenGLResourceLifetime* resource_domain() const noexcept { return lifetime_.get(); }

    private:
        void reset() noexcept;
    };
}
