#pragma once

#include <assets/manifest.h>
#include <assets/primitives/vertex.h>

#include <cstdint>
#include <memory>

namespace CE::Assets {
    struct GridGeometry {
        std::shared_ptr<Vertex2D> vertices;
        std::uint32_t vertex_count{};
    };

    [[nodiscard]] GridGeometry make_grid_geometry(const GridDefinition& grid, math::Pivot pivot, PixelSize texture_size);
}
