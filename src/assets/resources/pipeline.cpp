#include <assets/resources/pipeline.h>

#include <internals/exceptions.h>

#include <utility>

namespace CE::Assets {
    void validate_pipeline_definition(const PipelineDefinition& definition) {
        if (definition.program_sources.empty())
            throw Exceptions::invalid_args(CE_HERE, "Pipeline requires program sources");
        for (const auto& source : definition.program_sources) {
            if (source.empty())
                throw Exceptions::invalid_args(CE_HERE, "Pipeline program source must not be empty");
        }
        if (definition.vertex_layout != VertexLayout2D::Position3UV2)
            throw Exceptions::invalid_args(CE_HERE, "Unsupported 2D vertex layout");
        if (definition.topology != PrimitiveTopology::Triangles && definition.topology != PrimitiveTopology::TriangleStrip)
            throw Exceptions::invalid_args(CE_HERE, "Unsupported pipeline topology");
        const auto& state = definition.state;
        if (state.blend < BlendMode::Opaque || state.blend > BlendMode::Additive ||
            state.depth < DepthMode::Disabled || state.depth > DepthMode::LessEqual ||
            state.cull < CullMode::None || state.cull > CullMode::Back || (state.depth == DepthMode::Disabled && state.depth_write))
            throw Exceptions::invalid_args(CE_HERE, "Unsupported or inconsistent 2D pipeline state");
        validate_parameter_contract(definition.parameters);
    }

    Pipeline::Pipeline(PipelineDefinition definition)
    : definition_(std::move(definition)) {
        validate_pipeline_definition(definition_);
    }

    Material::Material(MaterialDefinition definition)
    : definition_(std::move(definition)) {
        if (!definition_.pipeline)
            throw Exceptions::invalid_args(CE_HERE, "Material requires a pipeline generation");
        validate_parameter_values(definition_.pipeline->definition().parameters, definition_.defaults);
    }

    ParameterSet Material::resolve(
        const ShaderPass& pass_semantics,
        const ShaderDraw& draw_semantics,
        const ParameterSet& pass_values,
        const ParameterSet& draw_values
    ) const {
        return resolve_parameters(definition_.pipeline->definition().parameters, pass_semantics, draw_semantics,
            pass_values, definition_.defaults, draw_values);
    }
}
