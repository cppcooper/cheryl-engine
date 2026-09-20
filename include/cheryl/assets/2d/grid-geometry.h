#pragma once

#include <assets/manifest.h>
#include <assets/primitives/texture.h>
#include <assets/primitives/vertex-array-object.h>

#include <cstdint>
#include <memory>

namespace CE::Assets {
    struct GridGeometry {
        std::shared_ptr<Vertex2D> vertices;
        std::uint32_t vertex_count{};
    };

    [[nodiscard]] GridGeometry make_grid_geometry(const GridDefinition& grid,
                                                  math::Pivot pivot,
                                                  const Texture& texture);
}
