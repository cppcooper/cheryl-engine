#pragma once

#include <cstddef>

namespace CE::Assets {
    enum class PrimitiveTopology { Triangles, TriangleStrip };

    // A backend-owned 2D vertex buffer, independent of material/image bindings.
    struct Geometry2D {
        virtual ~Geometry2D() = default;
        virtual void bind() const = 0;
        // Each call draws one range using the topology chosen when the buffer was uploaded.
        virtual void draw(std::size_t first_vertex, std::size_t vertex_count) const = 0;
    };
}
