#pragma once

#include <backends/opengl/gl.h>

#include <cstddef>
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
        explicit OpenGLResourceLifetime(std::thread::id owner) : owner_(owner) {}

        [[nodiscard]] std::size_t track(GLResourceKind kind, GLuint id);
        void retire(std::size_t slot) noexcept;
        void collect();
        void shutdown();
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
        void require_current_locked() const;

        mutable std::mutex mutex_;
        std::vector<Entry> entries_;
        std::size_t pending_ = none;
        std::size_t free_ = none;
        std::thread::id owner_;
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
