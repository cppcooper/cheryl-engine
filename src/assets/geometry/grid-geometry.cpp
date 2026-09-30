#include <assets/geometry/grid-geometry.h>

#include <core/resources/memory.h>
#include <core/resources/memory/managed-block.hpp>
#include <math/anchor.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace CE::Assets {
    GridGeometry make_grid_geometry(const GridDefinition& grid, const math::Pivot pivot, const PixelSize texture_size) {
        // Validate the pixel rectangle and vertex-count limit before requesting pooled CPU storage.
        if (texture_size.width == 0 || texture_size.height == 0) {
            throw Exceptions::runtime_exception(CE_HERE, "Cannot build an asset grid from an empty texture");
        }
        if (grid.occupied_right() > texture_size.width || grid.occupied_bottom() > texture_size.height) {
            throw Exceptions::runtime_exception(CE_HERE, "Asset grid extends beyond its texture bounds");
        }

        const auto cell_count = grid.cell_count();
        if (cell_count > std::numeric_limits<std::uint32_t>::max() / VAONumbers::vertices_per_strip_quad) {
            throw Exceptions::runtime_exception("overflow", CE_HERE, "Asset grid has too many vertices for a VAO");
        }
        const auto vertex_count = static_cast<std::uint32_t>(cell_count * VAONumbers::vertices_per_strip_quad);
        const auto vertices_bytes = sizeof(Vertex2D) * vertex_count;
        auto& manager = Mem::ExactMMgr::get();
        auto block = manager.checkout_chunk(vertices_bytes, alignof(Vertex2D));
        auto vertices = Mem::make_managed_block<Vertex2D>(manager, std::move(block));

        // Each atlas cell gets its own four-vertex strip. A draw of a single cell starts
        // a new strip, so adjacent frames never become triangles across their boundary.
        for (CellIndex cell = 0; cell < cell_count; ++cell) {
            const auto rect = grid.cell_rect(cell);
            if (rect.x > std::numeric_limits<std::uint32_t>::max() ||
                rect.y > std::numeric_limits<std::uint32_t>::max()) {
                throw Exceptions::runtime_exception("overflow", CE_HERE,
                    "Asset grid pixel coordinate exceeds uint32_t");
            }
            const auto strip = math::Anchor::MakeQuadStrip(pivot, texture_size.width, texture_size.height, rect.width,
                rect.height, static_cast<std::uint32_t>(rect.x),
                static_cast<std::uint32_t>(rect.y));
            std::copy(strip.vertices().begin(), strip.vertices().end(),
                vertices.get() + cell * VAONumbers::vertices_per_strip_quad);
        }
        return {std::move(vertices), vertex_count};
    }
}
