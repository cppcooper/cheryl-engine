#pragma once

#include <cheryl/export.h>

#include <assets/resources/sampler.h>
#include <backends/opengl/resource-lifetime.h>

#include <cstdint>
#include <memory>

namespace CE::Rendering::OpenGL {
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
