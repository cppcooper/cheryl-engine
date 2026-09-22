#include <assets/2d/grid-geometry.h>

#include <core/resources/memory.h>
#include <math/anchor.h>
#include <internals/exceptions.h>

#include <limits>
#include <utility>

namespace CE::Assets {
    GridGeometry make_grid_geometry(const GridDefinition& grid, const math::Pivot pivot, const Texture& texture) {
        if (texture.width <= 0 || texture.height <= 0) {
            throw Exceptions::runtime_exception(CE_HERE, "Cannot build an asset grid from an empty texture");
        }
        if (grid.occupied_right() > static_cast<std::uint64_t>(texture.width) ||
            grid.occupied_bottom() > static_cast<std::uint64_t>(texture.height)) {
            throw Exceptions::runtime_exception(CE_HERE, "Asset grid extends beyond its texture bounds");
        }

        const auto cell_count = grid.cell_count();
        if (cell_count > std::numeric_limits<std::uint32_t>::max() / VAONumbers::vertices_per_quad) {
            throw Exceptions::runtime_exception("overflow", CE_HERE, "Asset grid has too many vertices for a VAO");
        }
        const auto vertex_count = static_cast<std::uint32_t>(cell_count * VAONumbers::vertices_per_quad);
        const auto vertices_bytes = sizeof(Vertex2D) * vertex_count;
        auto block = Mem::ExactMMgr::get().checkout_chunk(vertices_bytes, alignof(Vertex2D));
        auto vertices = std::shared_ptr<Vertex2D>(static_cast<Vertex2D*>(block.head.get()),
                                                  [block](Vertex2D*) { Mem::ExactMMgr::get().return_chunk(block); });

        for (CellIndex cell = 0; cell < cell_count; ++cell) {
            const auto rect = grid.cell_rect(cell);
            if (rect.x > std::numeric_limits<std::uint32_t>::max() ||
                rect.y > std::numeric_limits<std::uint32_t>::max()) {
                throw Exceptions::runtime_exception("overflow", CE_HERE,
                                                    "Asset grid pixel coordinate exceeds uint32_t");
            }
            math::Anchor::MakePivot(pivot, vertices.get() + cell * VAONumbers::vertices_per_quad,
                                    static_cast<std::uint32_t>(texture.width),
                                    static_cast<std::uint32_t>(texture.height), rect.width, rect.height,
                                    static_cast<std::uint32_t>(rect.x), static_cast<std::uint32_t>(rect.y));
        }
        return {std::move(vertices), vertex_count};
    }
}
