#pragma once

#include <cstddef>

namespace CE::Assets {
    enum class PrimitiveTopology { Triangles, TriangleStrip };
    enum class VertexLayout2D { Position3UV2, Position3UV2Color4, Unsupported };

    // A backend-owned 2D vertex buffer, independent of material/image bindings.
    struct Geometry2D {
        virtual ~Geometry2D() = default;
        // Immutable upload metadata; safe to inspect during CPU frame preparation.
        [[nodiscard]] virtual VertexLayout2D vertex_layout() const noexcept = 0;
        [[nodiscard]] virtual PrimitiveTopology topology() const noexcept = 0;
        [[nodiscard]] virtual std::size_t vertex_count() const noexcept = 0;
        virtual void bind() const = 0;
        // Each call draws one range using the topology chosen when the buffer was uploaded.
        virtual void draw(std::size_t first_vertex, std::size_t vertex_count) const = 0;
    };
}
