#pragma once

#include <core/display/framebuffer-size.h>
#include <glm.hpp>

#include <cstdint>

namespace CE {
    /** Tracks projection and view changes by revision. The game or a render pass
     * chooses when to use these matrices; the camera does not set render policy.
     * Mutable state is unsynchronized: use one owner and copy matrices into frames.
     * Matrix references borrow this camera and must not race with its setters.
     */
    class CameraBase {
    public:
        virtual ~CameraBase() = default;

        // Pixel dimensions; negatives throw unchanged. Zero sizes are retained but
        // projection uses at least one pixel per axis. Equal sizes keep the revision.
        void set_framebuffer_size(FramebufferSize size);
        // Exact component equality suppresses updates. No finiteness/invertibility
        // validation; changed matrices advance the revision once.
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

    // Y-up orthographic bounds [0, width] x [0, height], near/far 0/1 using GLM.
    class Camera2D final : public CameraBase {
    public:
        Camera2D();

    protected:
        void recalculate_projection() override;
    };

    // GLM perspective using framebuffer aspect; construction starts at revision zero.
    class Camera3D final : public CameraBase {
    public:
        Camera3D();
        // Vertical FOV in finite degrees (0, 180), finite 0 < near < far in view
        // units. Invalid values throw unchanged; a changed configuration advances once.
        void set_perspective(float fov_degrees, float near_plane, float far_plane);

    protected:
        void recalculate_projection() override;

    private:
        float fov_degrees_ = 45.0f;
        float near_plane_ = 0.1f;
        float far_plane_ = 10000.0f;
    };
}
