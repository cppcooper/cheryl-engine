#pragma once

#include <core/display/framebuffer-size.h>
#include <enums/gfx-mode.h>
#include <glm.hpp>

#include <cstdint>

namespace CE {
    /** Tracks projection and view changes by revision. The runtime checks the
     * active camera after framebuffer changes and publishes matrices only when
     * that revision or the active camera changes.
     */
    class CameraBase {
    public:
        virtual ~CameraBase() = default;
        [[nodiscard]] virtual Enum::gfx_mode mode() const = 0;

        void set_framebuffer_size(FramebufferSize size);
        void set_view_matrix(const glm::mat4& view);

        [[nodiscard]] FramebufferSize framebuffer_size() const { return framebuffer_size_; }
        [[nodiscard]] const glm::mat4& projection_matrix() const { return projection_matrix_; }
        [[nodiscard]] const glm::mat4& view_matrix() const { return view_matrix_; }
        [[nodiscard]] std::uint64_t revision() const { return revision_; }

    protected:
        virtual void recalculate_projection() = 0;

        FramebufferSize framebuffer_size_{};
        glm::mat4 projection_matrix_{1.0f};
        glm::mat4 view_matrix_{1.0f};
        std::uint64_t revision_ = 0;
    };

    class Camera2D final : public CameraBase {
    public:
        Camera2D();
        [[nodiscard]] Enum::gfx_mode mode() const override { return Enum::gfx_mode::R2D; }

    protected:
        void recalculate_projection() override;
    };

    class Camera3D final : public CameraBase {
    public:
        Camera3D();
        [[nodiscard]] Enum::gfx_mode mode() const override { return Enum::gfx_mode::R3D; }
        void set_perspective(float fov_degrees, float near_plane, float far_plane);

    protected:
        void recalculate_projection() override;

    private:
        float fov_degrees_ = 45.0f;
        float near_plane_ = 0.1f;
        float far_plane_ = 10000.0f;
    };
}
