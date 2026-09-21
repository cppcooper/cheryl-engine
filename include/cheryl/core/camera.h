#pragma once

#include <glm.hpp>

namespace CE {
    struct FramebufferSize {
        int width = 1;
        int height = 1;

        bool operator==(const FramebufferSize&) const = default;
    };

    class CameraBase {
    public:
        virtual ~CameraBase() = default;

        void set_framebuffer_size(FramebufferSize size);
        void set_view_matrix(const glm::mat4& view);

        [[nodiscard]] FramebufferSize framebuffer_size() const { return framebuffer_size_; }
        [[nodiscard]] const glm::mat4& projection_matrix() const { return projection_matrix_; }
        [[nodiscard]] const glm::mat4& view_matrix() const { return view_matrix_; }

    protected:
        virtual void recalculate_projection() = 0;

        FramebufferSize framebuffer_size_{};
        glm::mat4 projection_matrix_{1.0f};
        glm::mat4 view_matrix_{1.0f};
    };

    class Camera2D final : public CameraBase {
    public:
        Camera2D();

    protected:
        void recalculate_projection() override;
    };

    class Camera3D final : public CameraBase {
    public:
        Camera3D();
        void set_perspective(float fov_degrees, float near_plane, float far_plane);

    protected:
        void recalculate_projection() override;

    private:
        float fov_degrees_ = 45.0f;
        float near_plane_ = 0.1f;
        float far_plane_ = 10000.0f;
    };
}
