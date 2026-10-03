#pragma once

#include <backends/opengl/gl.h>
#include <core/diagnostics.h>
#include <chrono>
#include <cstdint>

namespace CE::RenderAPIs {
    struct NativeDebugStats {
        Diagnostics::DomainId domain = 0;
        std::uint64_t high = 0;
        std::uint64_t medium = 0;
        std::uint64_t filtered = 0;
        GLuint last_id = 0;
        GLenum last_source = 0;
        GLenum last_type = 0;
        bool supported = false;
        bool installed = false;
        bool host_owned = false;
    };

    namespace RendererDetail {
        // Internal synchronous observation. No renderer pointer is stored in GL.
        // The static callback is harmless after failed context recovery/destruction.
        class DebugOutput final {
            NativeDebugStats stats_;
            bool previous_enabled_ = false;
            bool previous_synchronous_ = false;
            void* previous_user_ = nullptr;
            std::uint64_t reported_high_ = 0;
            std::uint64_t reported_medium_ = 0;
            std::chrono::steady_clock::time_point next_report_;
            static thread_local DebugOutput* observing_;

        public:
            class Scope final {
                DebugOutput* previous_;

            public:
                explicit Scope(DebugOutput* output) noexcept;
                ~Scope();
                Scope(const Scope&) = delete;
                Scope& operator=(const Scope&) = delete;
            };

            void start(Diagnostics::DomainId domain) noexcept;
            void stop() noexcept;
            void report(bool force = false) noexcept;
            [[nodiscard]] NativeDebugStats diagnostics() const noexcept { return stats_; }

        private:
            static void GLAD_API_PTR
            callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei, const GLchar*, const void*) noexcept;
        };
    }
}
