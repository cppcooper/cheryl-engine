#pragma once

#include <assets/resources/pipeline.h>
#include <backends/opengl/glslprogram.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace CE::Assets {
    struct GLSLParameterBinding {
        std::string key;
        std::string uniform;
        // Optional active custom uniforms require this or a contract default.
        // A missing engine semantic instead rejects the incomplete packet.
        std::optional<ParameterValue> missing_value;
    };

    struct GLSLPipelineBindings {
        std::vector<GLSLParameterBinding> parameters;
        std::string position_attribute = "in_Position";
        std::string uv_attribute = "in_Texcoord";
    };

    class GLSLPipeline final : public Pipeline {
        struct BoundParameter {
            std::string key;
            GLint location = -1;
            std::optional<ParameterValue> missing_value;
        };

        const std::shared_ptr<GLSLProgram> program_;
        std::vector<BoundParameter> parameters_;

    public:
        GLSLPipeline(
            PipelineDefinition definition,
            std::shared_ptr<GLSLProgram> program,
            const GLSLPipelineBindings& bindings
        );
        [[nodiscard]] const RenderAPIs::OpenGLResourceLifetime* resource_domain() const noexcept { return program_->resource_domain(); }
        // Check images/defaults without binding or changing any native state.
        void validate_resources(const ParameterSet& values) const;
        // Applies copied parameter/resource values only; fixed state/pass integration is separate.
        void bind_parameters(const ParameterSet& values) const;
    };
}
