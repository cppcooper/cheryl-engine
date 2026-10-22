#pragma once

#include <cheryl/export.h>

#include <core/display/framebuffer-size.h>
#include <glm.hpp>

#include <cstdint>

namespace CE::Rendering {
    /** Tracks projection and view changes by revision. The game or a render pass
     * chooses when to use these matrices; the camera does not set render policy.
     * Mutable state is unsynchronized: use one owner and copy matrices into frames.
     * Matrix references borrow this camera and must not race with its setters.
     */
    class CE_TYPE_API CameraBase {
    public:
        CE_API virtual ~CameraBase();

        // Pixel dimensions; negatives throw unchanged. Zero sizes are retained but
        // projection uses at least one pixel per axis. Equal sizes keep the revision.
        CE_API void set_framebuffer_size(::CE::Platform::FramebufferSize size);
        // Exact component equality suppresses updates. No finiteness/invertibility
        // validation; changed matrices advance the revision once.
        CE_API void set_view_matrix(const glm::mat4& view);

        [[nodiscard]] ::CE::Platform::FramebufferSize framebuffer_size() const { return framebuffer_size_; }
        [[nodiscard]] const glm::mat4& projection_matrix() const { return projection_matrix_; }
        [[nodiscard]] const glm::mat4& view_matrix() const { return view_matrix_; }
        [[nodiscard]] std::uint64_t revision() const { return revision_; }

    protected:
        virtual void recalculate_projection() = 0;

        ::CE::Platform::FramebufferSize framebuffer_size_{}; ///< Stored pixel extent, including a minimized zero extent.
        glm::mat4 projection_matrix_{1.0f}; ///< Derived projection copied into retained render passes.
        glm::mat4 view_matrix_{1.0f}; ///< Caller-supplied world-to-view matrix.
        std::uint64_t revision_ = 0; ///< Advances once per changed public camera configuration.
    };

    /** Y-up GLM orthographic camera spanning [0,width] by [0,height], near/far 0/1. */
    class CE_TYPE_API Camera2D final : public CameraBase {
    public:
        CE_API Camera2D();

    protected:
        CE_API void recalculate_projection() override;
    };

    /** GLM perspective camera using framebuffer aspect and caller-selected view units. */
    class CE_TYPE_API Camera3D final : public CameraBase {
    public:
        CE_API Camera3D();
        // Vertical FOV in finite degrees (0, 180), finite 0 < near < far in view
        // units. Invalid values throw unchanged; a changed configuration advances once.
        CE_API void set_perspective(float fov_degrees, float near_plane, float far_plane);

    protected:
        CE_API void recalculate_projection() override;

    private:
        float fov_degrees_ = 45.0f;
        float near_plane_ = 0.1f;
        float far_plane_ = 10000.0f;
    };
}

/**
 * @fn CE::Rendering::CameraBase::~CameraBase()
 * @brief Destroy camera state; published frame matrix copies are independent.
 */

/**
 * @fn void CE::Rendering::CameraBase::set_framebuffer_size(CE::Platform::FramebufferSize size)
 * @brief Set pixel extent and recompute projection on this camera's unsynchronized owner.
 * @param size Nonnegative pixel width/height; zero is retained while projection uses at least one pixel per axis.
 * @throws CE::Exceptions::invalid_args For negative dimensions, without mutation.
 * Equal extents do nothing; a changed extent/projection advances the revision once.
 */

/**
 * @fn void CE::Rendering::CameraBase::set_view_matrix(const glm::mat4& view)
 * @brief Copy the view matrix and increment revision when any component differs exactly.
 * @param view Borrowed matrix; no finiteness or invertibility validation is added.
 * Does not alter projection or native state; exclude concurrent mutation/read of this camera.
 */

/**
 * @fn CE::Platform::FramebufferSize CE::Rendering::CameraBase::framebuffer_size() const
 * @return Copied stored pixel extent, including zero dimensions; access is unsynchronized.
 */

/**
 * @fn const glm::mat4& CE::Rendering::CameraBase::projection_matrix() const
 * @return Borrowed projection; keep this camera alive and exclude setters while reading/copying it.
 */

/**
 * @fn const glm::mat4& CE::Rendering::CameraBase::view_matrix() const
 * @return Borrowed view; keep this camera alive and exclude setters while reading/copying it.
 */

/**
 * @fn std::uint64_t CE::Rendering::CameraBase::revision() const
 * @return Current change counter, not a publication barrier or synchronization primitive.
 */

/**
 * @fn void CE::Rendering::CameraBase::recalculate_projection()
 * @brief Derived-camera hook recomputing projection from the stored configuration. Called by setters before their revision increment; no native rendering policy is selected.
 */

/**
 * @fn CE::Rendering::Camera2D::Camera2D()
 * @brief Compute the initial orthographic projection at revision zero; a zero initial pixel extent uses one pixel per axis.
 */

/**
 * @fn void CE::Rendering::Camera2D::recalculate_projection()
 * @brief Recompute Y-up orthographic bounds using at least one pixel per framebuffer axis, without selecting depth policy.
 */

/**
 * @fn CE::Rendering::Camera3D::Camera3D()
 * @brief Compute the initial perspective projection at revision zero using the declared default field of view and clipping planes.
 */

/**
 * @fn void CE::Rendering::Camera3D::set_perspective(float fov_degrees, float near_plane, float far_plane)
 * @brief Validate and apply perspective configuration as one camera revision.
 * @param fov_degrees Finite vertical angle strictly between 0 and 180 degrees.
 * @param near_plane Finite positive near distance in view units.
 * @param far_plane Finite distance greater than near_plane, in the same units.
 * @throws CE::Exceptions::invalid_args For invalid values, leaving the camera unchanged.
 * Equal configuration does nothing; a change recomputes projection and advances once.
 * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
 */

/**
 * @fn void CE::Rendering::Camera3D::recalculate_projection()
 * @brief Recompute GLM perspective from stored FOV/planes and framebuffer aspect, using at least one pixel per axis.
 */
