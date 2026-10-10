#pragma once

#include <assets/resources/geometry2d.h>
#include <assets/resources/parameters.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace CE::Assets {
    class Pipeline;

    enum class BlendMode { Opaque, StraightAlpha, PremultipliedAlpha, Additive };

    enum class DepthMode { Disabled, Less, LessEqual };

    // Front-facing triangles use counterclockwise winding.
    enum class CullMode { None, Front, Back };

    struct PipelineState2D {
        BlendMode blend = BlendMode::StraightAlpha;
        DepthMode depth = DepthMode::Disabled;
        bool depth_write = false;
        CullMode cull = CullMode::None;
    };

    // A pass constrains pipeline intent; it never overrides one draw's state.
    // Default 2D passes require disabled depth. Reset depth to allow either mode.
    struct PassConstraints2D {
        std::optional<BlendMode> blend;
        std::optional<DepthMode> depth = DepthMode::Disabled;
        std::optional<bool> depth_write;
        std::optional<CullMode> cull;
    };

    // Owned CPU recipe; source paths are not read by common validation. Publication
    // copies a snapshot into Pipeline; native builders additionally link/reflect it.
    struct PipelineDefinition {
        std::vector<std::filesystem::path> program_sources;
        VertexLayout2D vertex_layout = VertexLayout2D::Position3UV2;
        PrimitiveTopology topology = PrimitiveTopology::Triangles;
        PipelineState2D state;
        ParameterContract parameters;
    };

    // Retains a pipeline generation; defaults own image handles/unit requests.
    // Material construction validates this partial custom layer, not a complete draw.
    struct MaterialDefinition {
        std::shared_ptr<const Pipeline> pipeline;
        ParameterSet defaults;
        std::map<std::string, std::shared_ptr<const Sampler>, std::less<>> sampling = {};
    };

    // CPU-only structural validation; throws on empty paths, unsupported layout/
    // topology/state, disabled-depth writes or an invalid parameter contract.
    void validate_pipeline_definition(const PipelineDefinition& definition);
}
