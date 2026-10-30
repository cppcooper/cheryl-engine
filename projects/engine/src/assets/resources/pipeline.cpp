#include <assets/resources/pipeline.h>

#include <internals/exceptions.h>

#include <algorithm>
#include <utility>

namespace CE::Rendering::Pipelines {
    void validate(const Definition& definition) {
        if (definition.program_sources.empty())
            throw Exceptions::invalid_args(CE_HERE, "Pipeline requires program sources");
        for (const auto& source : definition.program_sources) {
            if (source.empty())
                throw Exceptions::invalid_args(CE_HERE, "Pipeline program source must not be empty");
        }
        if (definition.vertex_layout != CE::Rendering::Geometry::Layout2D::Position3UV2 &&
            definition.vertex_layout != CE::Rendering::Geometry::Layout2D::Position3UV2Color4)
            throw Exceptions::invalid_args(CE_HERE, "Unsupported 2D vertex layout");
        if (definition.topology != CE::Rendering::Geometry::Topology::Triangles &&
            definition.topology != CE::Rendering::Geometry::Topology::TriangleStrip)
            throw Exceptions::invalid_args(CE_HERE, "Unsupported pipeline topology");
        const auto& state = definition.state;
        if (state.blend < Blend::Opaque || state.blend > Blend::Additive || state.depth < Depth::Disabled ||
            state.depth > Depth::LessEqual || state.cull < Cull::None || state.cull > Cull::Back ||
            (state.depth == Depth::Disabled && state.depth_write))
            throw Exceptions::invalid_args(CE_HERE, "Unsupported or inconsistent 2D pipeline state");
        CE::Rendering::Parameters::validate_contract(definition.parameters);
    }
}

namespace CE::Rendering::Resources {
    Pipeline::Pipeline(CE::Rendering::Pipelines::Definition definition)
    : definition_(std::move(definition)) {
        CE::Rendering::Pipelines::validate(definition_);
    }

    void Pipeline::validate_draw(
        const CE::Rendering::Geometry::Buffer2D& geometry,
        const std::size_t first_vertex,
        const std::size_t vertex_count,
        const CE::Rendering::Pipelines::PassConstraints2D& constraints
    ) const {
        if (geometry.vertex_layout() != definition_.vertex_layout || geometry.topology() != definition_.topology)
            throw Exceptions::invalid_args(CE_HERE, "Geometry layout/topology does not match the pipeline");
        if (vertex_count == 0 || first_vertex > geometry.vertex_count() || vertex_count > geometry.vertex_count() - first_vertex)
            throw Exceptions::invalid_args(CE_HERE, "Pipeline draw range exceeds uploaded geometry");
        if ((definition_.topology == CE::Rendering::Geometry::Topology::Triangles && vertex_count % 3 != 0) ||
            (definition_.topology == CE::Rendering::Geometry::Topology::TriangleStrip && vertex_count < 3))
            throw Exceptions::invalid_args(CE_HERE, "Pipeline draw range has incomplete primitives");
        const auto& state = definition_.state;
        if ((constraints.blend && *constraints.blend != state.blend) || (constraints.depth && *constraints.depth != state.depth) ||
            (constraints.depth_write && *constraints.depth_write != state.depth_write) ||
            (constraints.cull && *constraints.cull != state.cull))
            throw Exceptions::invalid_args(CE_HERE, "Pipeline state conflicts with its pass constraints");
    }

    Material::Material(CE::Rendering::Materials::Definition definition)
    : definition_(std::move(definition)) {
        if (!definition_.pipeline)
            throw Exceptions::invalid_args(CE_HERE, "Material requires a pipeline generation");
        CE::Rendering::Parameters::validate_values(definition_.pipeline->definition().parameters, definition_.defaults);
        for (const auto& [key, sampler] : definition_.sampling) {
            const auto& parameters = definition_.pipeline->definition().parameters;
            const auto found = std::ranges::find(parameters, key, &CE::Rendering::Parameters::Definition::key);
            if (!sampler || found == parameters.end() || found->type != CE::Rendering::Parameters::Type::Sampler2D)
                throw Exceptions::invalid_args(CE_HERE, "Material sampling override requires a sampler parameter");
        }
    }

    CE::Rendering::Parameters::Set Material::resolve(
        const CE::Rendering::Parameters::PassSemantics& pass_semantics,
        const CE::Rendering::Parameters::DrawSemantics& draw_semantics,
        const CE::Rendering::Parameters::Set& pass_values,
        const CE::Rendering::Parameters::Set& draw_values
    ) const {
        auto result = CE::Rendering::Parameters::resolve(
            definition_.pipeline->definition().parameters, pass_semantics, draw_semantics, pass_values, definition_.defaults, draw_values
        );
        for (const auto& [key, sampler] : definition_.sampling)
            if (const auto found = result.find(key); found != result.end()) {
                auto& binding = std::get<CE::Rendering::Parameters::ImageBinding>(found->second);
                if (!binding.sampler)
                    binding.sampler = sampler;
            }
        return result;
    }
}

CE::Rendering::Resources::Pipeline::~Pipeline() = default;
