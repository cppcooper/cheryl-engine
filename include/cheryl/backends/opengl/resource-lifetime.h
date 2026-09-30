#pragma once

#include <backends/opengl/gl.h>

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace CE::RenderAPIs {
    enum class GLResourceKind { Texture, Buffer, VertexArray, Program };

    // All GL calls happen on the context thread. Asset destructors may run on another
    // thread, so they only mark their handles for deletion here.
    class OpenGLResourceLifetime final {
    public:
        // The predicate borrows a context that must outlive this active lifetime.
        // It is queried only on the owner thread, and must not reenter the lifetime.
        OpenGLResourceLifetime(std::thread::id owner, std::function<bool()> is_current);

        [[nodiscard]] std::size_t track(GLResourceKind kind, GLuint id);
        void retire(std::size_t slot) noexcept;
        void collect();
        void shutdown();
        // Failure fallback: invalidate handles without issuing calls to an unavailable context.
        // The platform's context destruction releases any remaining native resources.
        void abandon() noexcept;
        void require_owner() const;
        void require_current() const;

    private:
        static constexpr std::size_t none = std::numeric_limits<std::size_t>::max();
        struct Entry {
            GLResourceKind kind;
            GLuint id;
            std::size_t next = none;
            bool pending = false;
        };

        static void delete_handle(GLResourceKind kind, GLuint id) noexcept;
        void require_owner_locked() const;
        void require_current_locked() const;

        mutable std::mutex mutex_;
        std::vector<Entry> entries_;
        std::size_t pending_ = none;
        std::size_t free_ = none;
        std::thread::id owner_;
        std::function<bool()> is_current_;
        bool active_ = true;
    };

    // Move-only registration. Its destructor never calls OpenGL.
    class OpenGLHandle final {
    public:
        OpenGLHandle() = default;
        OpenGLHandle(std::shared_ptr<OpenGLResourceLifetime> lifetime, GLResourceKind kind, GLuint id);
        ~OpenGLHandle() { reset(); }

        OpenGLHandle(const OpenGLHandle&) = delete;
        OpenGLHandle& operator=(const OpenGLHandle&) = delete;
        OpenGLHandle(OpenGLHandle&& other) noexcept;
        OpenGLHandle& operator=(OpenGLHandle&& other) noexcept;

        [[nodiscard]] GLuint id() const;

    private:
        void reset() noexcept;
        std::shared_ptr<OpenGLResourceLifetime> lifetime_;
        std::size_t slot_ = std::numeric_limits<std::size_t>::max();
        GLuint id_ = 0;
    };
}
