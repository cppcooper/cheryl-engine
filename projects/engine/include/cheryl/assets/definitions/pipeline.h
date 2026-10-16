#pragma once

#include <cheryl/export.h>

#include <assets/resources/geometry2d.h>
#include <assets/resources/parameters.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace CE::Rendering::Resources {
    class Pipeline;
}

namespace CE::Rendering::Pipelines {
    enum class Blend { Opaque, StraightAlpha, PremultipliedAlpha, Additive };

    enum class Depth { Disabled, Less, LessEqual };

    // Front-facing triangles use counterclockwise winding.
    enum class Cull { None, Front, Back };

    struct State2D {
        Blend blend = Blend::StraightAlpha;
        Depth depth = Depth::Disabled;
        bool depth_write = false;
        Cull cull = Cull::None;
    };

    // A pass constrains pipeline intent; it never overrides one draw's state.
    // Default 2D passes require disabled depth. Reset depth to allow either mode.
    struct PassConstraints2D {
        std::optional<Blend> blend;
        std::optional<Depth> depth = Depth::Disabled;
        std::optional<bool> depth_write;
        std::optional<Cull> cull;
    };

    // Owned CPU recipe; source paths are not read by common validation. Publication
    // copies a snapshot into Pipeline; native builders additionally link/reflect it.
    struct Definition {
        std::vector<std::filesystem::path> program_sources;
        CE::Rendering::Geometry::Layout2D vertex_layout = CE::Rendering::Geometry::Layout2D::Position3UV2;
        CE::Rendering::Geometry::Topology topology = CE::Rendering::Geometry::Topology::Triangles;
        State2D state;
        CE::Rendering::Parameters::Contract parameters;
    };
}

namespace CE::Rendering::Materials {
    // Retains a pipeline generation; defaults own image handles/unit requests.
    // Material construction validates this partial custom layer, not a complete draw.
    struct Definition {
        std::shared_ptr<const CE::Rendering::Resources::Pipeline> pipeline;
        CE::Rendering::Parameters::Set defaults;
        std::map<std::string, std::shared_ptr<const CE::Rendering::Sampling::Sampler>, std::less<>> sampling = {};
    };
}

namespace CE::Rendering::Pipelines {
    // CPU-only structural validation; throws on empty paths, unsupported layout/
    // topology/state, disabled-depth writes or an invalid parameter contract.
    CE_API void validate(const Definition& definition);
}
