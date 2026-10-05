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
        if (definition.vertex_layout != VertexLayout2D::Position3UV2 && definition.vertex_layout != VertexLayout2D::Position3UV2Color4)
            throw Exceptions::invalid_args(CE_HERE, "Unsupported 2D vertex layout");
        if (definition.topology != PrimitiveTopology::Triangles && definition.topology != PrimitiveTopology::TriangleStrip)
            throw Exceptions::invalid_args(CE_HERE, "Unsupported pipeline topology");
        const auto& state = definition.state;
        if (state.blend < BlendMode::Opaque || state.blend > BlendMode::Additive || state.depth < DepthMode::Disabled ||
            state.depth > DepthMode::LessEqual || state.cull < CullMode::None || state.cull > CullMode::Back ||
            (state.depth == DepthMode::Disabled && state.depth_write))
            throw Exceptions::invalid_args(CE_HERE, "Unsupported or inconsistent 2D pipeline state");
        validate_parameter_contract(definition.parameters);
    }

    Pipeline::Pipeline(PipelineDefinition definition)
    : definition_(std::move(definition)) {
        validate_pipeline_definition(definition_);
    }

    void Pipeline::validate_draw(
        const Geometry2D& geometry,
        const std::size_t first_vertex,
        const std::size_t vertex_count,
        const PassConstraints2D& constraints
    ) const {
        if (geometry.vertex_layout() != definition_.vertex_layout || geometry.topology() != definition_.topology)
            throw Exceptions::invalid_args(CE_HERE, "Geometry layout/topology does not match the pipeline");
        if (vertex_count == 0 || first_vertex > geometry.vertex_count() || vertex_count > geometry.vertex_count() - first_vertex)
            throw Exceptions::invalid_args(CE_HERE, "Pipeline draw range exceeds uploaded geometry");
        if ((definition_.topology == PrimitiveTopology::Triangles && vertex_count % 3 != 0) ||
            (definition_.topology == PrimitiveTopology::TriangleStrip && vertex_count < 3))
            throw Exceptions::invalid_args(CE_HERE, "Pipeline draw range has incomplete primitives");
        const auto& state = definition_.state;
        if ((constraints.blend && *constraints.blend != state.blend) || (constraints.depth && *constraints.depth != state.depth) ||
            (constraints.depth_write && *constraints.depth_write != state.depth_write) ||
            (constraints.cull && *constraints.cull != state.cull))
            throw Exceptions::invalid_args(CE_HERE, "Pipeline state conflicts with its pass constraints");
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
        return resolve_parameters(
            definition_.pipeline->definition().parameters, pass_semantics, draw_semantics, pass_values, definition_.defaults, draw_values
        );
    }
}
