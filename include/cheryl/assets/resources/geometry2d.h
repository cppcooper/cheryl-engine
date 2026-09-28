#pragma once

#include "image.h"

#include <cstddef>

namespace CE::Assets {
    enum class PrimitiveTopology { Triangles, TriangleStrip };

    // A backend-owned 2D vertex buffer. The image must come from the same backend.
    struct Geometry2D {
        virtual ~Geometry2D() = default;
        // TODO: Revisit binding an Image through Geometry2D. Geometry and texture/material state
        // are independent resources; making geometry bind both pushes resource-composition policy
        // into the buffer abstraction and may work against batching or other rendering backends.
        virtual void bind(const Image& image) const = 0;
        // Each call draws one range using the topology chosen when the buffer was uploaded.
        virtual void draw(std::size_t first_vertex, std::size_t vertex_count) const = 0;
    };
}
