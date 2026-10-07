#pragma once

#include <assets/definitions/grid.h>
#include <assets/types/primitives/vertex.h>
#include <math/anchor.h>

#include <cstdint>
#include <memory>

namespace CE::Assets {
    // Retained temporary CPU storage, four vertices per row-major cell. Keep through
    // the transient provider upload; no native resource is created by this builder.
    struct GridGeometry {
        std::shared_ptr<Vertex2D> vertices;
        std::uint32_t vertex_count{};
    };

    // Local Y-up pixels around the normalized top-left pivot, normalized UVs. Throws
    // for empty texture dimensions, out-of-image bounds or an excessive vertex count.
    // Direct grid/pivot values must satisfy the manifest's representability constraints.
    [[nodiscard]] GridGeometry make_grid_geometry(const GridDefinition& grid, math::Pivot pivot, PixelSize texture_size);
}
