#pragma once

#include <cheryl/export.h>

#include <assets/definitions/grid.h>
#include <assets/types/primitives/vertex.h>
#include <math/anchor.h>

#include <cstdint>
#include <memory>

namespace CE::Assets::Grids {
    // Retained temporary CPU storage, four vertices per row-major cell. Keep through
    // the transient provider upload; no native resource is created by this builder.
/** Retained CPU vertex allocation for four-vertex strips in row-major cell order; contains no native resource. */
    struct GeometryData {
        std::shared_ptr<CE::Rendering::Geometry::Vertex2D> vertices; ///< Owner retaining the vertex array; retain through the provider's transient upload.
        std::uint32_t vertex_count{}; ///< Array extent in vertices, four per grid cell.
    };

    // Local Y-up pixels around the normalized top-left pivot, normalized UVs. Throws
    // for empty texture dimensions, out-of-image bounds or an excessive vertex count.
    // Direct grid/pivot values must satisfy the manifest's representability constraints.
    [[nodiscard]] CE_API GeometryData
    make_geometry(const Definition& grid, CE::Rendering::Geometry::Pivot pivot, CE::Rendering::Geometry::PixelSize texture_size);
}

/**
 * @fn CE::Assets::Grids::GeometryData CE::Assets::Grids::make_geometry(const Definition& grid, CE::Rendering::Geometry::Pivot pivot, CE::Rendering::Geometry::PixelSize texture_size)
 * @brief Build local Y-up cell strips and normalized texture coordinates on the CPU.
 * @param grid Row-major pixel cells/spacing with representable count and occupied-edge arithmetic.
 * @param pivot Finite normalized top-left-origin pivot applied separately to every cell.
 * @param texture_size Nonzero image width/height in pixels.
 * @return Owned temporary vertices, four per cell; upload each selected cell as an independent strip.
 * Empty texture, out-of-image bounds or excessive vertex count reject before pooled vertex allocation. Invalid pivots and unrepresentable cell coordinates also reject; allocation failures propagate. This creates no graphics resource and publishes no cache entry.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */
