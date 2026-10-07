#pragma once

#include <cstddef>

namespace CE::Assets {
    enum class PrimitiveTopology { Triangles, TriangleStrip };
    enum class VertexLayout2D { Position3UV2, Position3UV2Color4, Unsupported };

    // Retained immutable upload, independent of material/image bindings. Logical
    // ownership can outlive shutdown; native operations require its live backend domain.
    struct Geometry2D {
        virtual ~Geometry2D() = default;
        // Immutable upload metadata; safe to inspect during CPU frame preparation.
        [[nodiscard]] virtual VertexLayout2D vertex_layout() const noexcept = 0;
        [[nodiscard]] virtual PrimitiveTopology topology() const noexcept = 0;
        [[nodiscard]] virtual std::size_t vertex_count() const noexcept = 0;
        // Bind/draw obey backend owner/current-context rules and do not select a material.
        virtual void bind() const = 0;
        // Range uses vertex indices/counts, not bytes; draw one complete primitive range
        // using the uploaded topology. Backend-specific state/validation still applies.
        virtual void draw(std::size_t first_vertex, std::size_t vertex_count) const = 0;
    };
}
