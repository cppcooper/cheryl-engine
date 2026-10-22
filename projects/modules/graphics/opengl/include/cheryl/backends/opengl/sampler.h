#pragma once

#include <cheryl/export.h>

#include <assets/resources/sampler.h>
#include <backends/opengl/resource-lifetime.h>

#include <cstdint>
#include <memory>

namespace CE::Rendering::OpenGL {
    /** Immutable native sampler object; separate from image pixels/default sampling.
     * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
     */
    class CE_TYPE_API Sampler final : public CE::Rendering::Sampling::Sampler {
        Handle handle_;
        std::uint32_t binding_unit_limit_ = 0;

    public:
        CE_OPENGL_API Sampler(std::shared_ptr<ResourceLifetime> lifetime, const CE::Rendering::Sampling::Options& options);
        CE_OPENGL_API void bind(std::uint32_t unit) const override;
        CE_OPENGL_API void require_binding(std::uint32_t unit) const;
        [[nodiscard]] const ResourceLifetime* resource_domain() const noexcept { return handle_.resource_domain(); }
    };
}

/**
 * @fn CE::Rendering::OpenGL::Sampler::Sampler(std::shared_ptr<ResourceLifetime> lifetime, const CE::Rendering::Sampling::Options& options)
 * @brief Create native sampling state in a live owner/current-context lifetime.
 * @param lifetime Required domain retained by its Handle.
 * @param options Valid immutable request; MaximumSupported resolves to the available maximum or 1.
 * Malformed options/domain or native/allocation failures reject construction; no image is retained or mutated.
 */

/**
 * @fn void CE::Rendering::OpenGL::Sampler::require_binding(std::uint32_t unit) const
 * @brief Validate active owner/current context and zero-based unit range without binding.
 * @param unit Unit below the construction-context limit.
 * Invalid units throw invalid_args; stopped/foreign/noncurrent access throws failed_operation.
 */

/**
 * @fn void CE::Rendering::OpenGL::Sampler::bind(std::uint32_t unit) const
 * @brief Bind the sampler object to one validated unit.
 * @param unit Zero-based supported texture unit.
 * Requires live owner/current context; does not select an image or alter its default policy. Pipeline validation separately checks compatible image mipmaps/domain.
 */

/**
 * @fn const CE::Rendering::OpenGL::ResourceLifetime* CE::Rendering::OpenGL::Sampler::resource_domain() const noexcept
 * @return Borrowed native lifetime identity while this sampler is retained; no activity/current-context check occurs.
 */
