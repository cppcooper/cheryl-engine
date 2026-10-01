#pragma once

#include <assets/resources/geometry2d.h>
#include <assets/resources/parameters.h>

#include <filesystem>
#include <memory>
#include <vector>

namespace CE::Assets {
    class Pipeline;

    // The initial contract accepts the existing interleaved Vertex2D upload only.
    enum class VertexLayout2D {
        Position3UV2
    };

    enum class BlendMode {
        Opaque,
        StraightAlpha,
        PremultipliedAlpha,
        Additive
    };

    enum class DepthMode {
        Disabled,
        Less,
        LessEqual
    };

    // Front-facing triangles use counterclockwise winding.
    enum class CullMode {
        None,
        Front,
        Back
    };

    struct PipelineState2D {
        BlendMode blend = BlendMode::StraightAlpha;
        DepthMode depth = DepthMode::Disabled;
        bool depth_write = false;
        CullMode cull = CullMode::None;
    };

    struct PipelineDefinition {
        std::vector<std::filesystem::path> program_sources;
        VertexLayout2D vertex_layout = VertexLayout2D::Position3UV2;
        PrimitiveTopology topology = PrimitiveTopology::Triangles;
        PipelineState2D state;
        ParameterContract parameters;
    };

    // Defaults include image handles/unit requests through ImageBinding values.
    struct MaterialDefinition {
        std::shared_ptr<const Pipeline> pipeline;
        ParameterSet defaults;
    };

    void validate_pipeline_definition(const PipelineDefinition& definition);
}
