#include <core/camera.h>

#include <internals/exceptions.h>
#include <ext/matrix_clip_space.hpp>

#include <algorithm>
#include <cmath>

namespace CE {
    void CameraBase::set_framebuffer_size(const FramebufferSize size) {
        if (size.width < 0 || size.height < 0) {
            throw Exceptions::invalid_args(CE_HERE, "Framebuffer dimensions must not be negative");
        }
        if (framebuffer_size_ == size)
            return;
        // Advance one revision for both the size and the resulting projection,
        // allowing the renderer to publish them as a single camera update.
        framebuffer_size_ = size;
        recalculate_projection();
        ++revision_;
    }

    void CameraBase::set_view_matrix(const glm::mat4& view) {
        if (view_matrix_ == view)
            return;
        view_matrix_ = view;
        ++revision_;
    }

    Camera2D::Camera2D() {
        recalculate_projection();
    }

    void Camera2D::recalculate_projection() {
        // A minimized window can have a zero-sized framebuffer; keep the projection defined.
        const auto width = static_cast<float>(std::max(framebuffer_size_.width, 1));
        const auto height = static_cast<float>(std::max(framebuffer_size_.height, 1));
        projection_matrix_ = glm::ortho(0.0f, width, 0.0f, height, 0.0f, 1.0f);
    }

    Camera3D::Camera3D() {
        recalculate_projection();
    }

    void Camera3D::set_perspective(const float fov_degrees, const float near_plane, const float far_plane) {
        if (!std::isfinite(fov_degrees) || fov_degrees <= 0.0f || fov_degrees >= 180.0f || !std::isfinite(near_plane) ||
            !std::isfinite(far_plane) || near_plane <= 0.0f || far_plane <= near_plane) {
            throw Exceptions::invalid_args(CE_HERE, "Perspective needs a valid field of view and near/far planes");
        }
        if (fov_degrees_ == fov_degrees && near_plane_ == near_plane && far_plane_ == far_plane)
            return;
        fov_degrees_ = fov_degrees;
        near_plane_ = near_plane;
        far_plane_ = far_plane;
        recalculate_projection();
        ++revision_;
    }

    void Camera3D::recalculate_projection() {
        const auto width = static_cast<float>(std::max(framebuffer_size_.width, 1));
        const auto height = static_cast<float>(std::max(framebuffer_size_.height, 1));
        projection_matrix_ = glm::perspective(glm::radians(fov_degrees_), width / height, near_plane_, far_plane_);
    }
}
