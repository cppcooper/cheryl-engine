#pragma once

#include <assets/resources/sampler.h>
#include <backends/opengl/resource-lifetime.h>

#include <cstdint>
#include <memory>

namespace CE::Assets {
    class OpenGLSampler final : public Sampler {
        RenderAPIs::OpenGLHandle handle_;
        std::uint32_t binding_unit_limit_ = 0;

    public:
        OpenGLSampler(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime, const SamplerOptions& options);
        void bind(std::uint32_t unit) const override;
        void require_binding(std::uint32_t unit) const;
        [[nodiscard]] const RenderAPIs::OpenGLResourceLifetime* resource_domain() const noexcept { return handle_.resource_domain(); }
    };
}
