#pragma once

#include <cheryl/export.h>

#include <cstddef>

namespace CE::Rendering::Geometry {
    enum class Topology { Triangles, TriangleStrip };
    enum class Layout2D { Position3UV2, Position3UV2Color4, Unsupported };

    // Retained immutable upload, independent of material/image bindings. Logical
    // ownership can outlive shutdown; native operations require its live backend domain.
    struct CE_API Buffer2D {
        virtual ~Buffer2D() = default;
        // Immutable upload metadata; safe to inspect during CPU frame preparation.
        [[nodiscard]] virtual Layout2D vertex_layout() const noexcept = 0;
        [[nodiscard]] virtual Topology topology() const noexcept = 0;
        [[nodiscard]] virtual std::size_t vertex_count() const noexcept = 0;
        // Bind/draw obey backend owner/current-context rules and do not select a material.
        virtual void bind() const = 0;
        // Range uses vertex indices/counts, not bytes; draw one complete primitive range
        // using the uploaded topology. Backend-specific state/validation still applies.
        virtual void draw(std::size_t first_vertex, std::size_t vertex_count) const = 0;
    };
}
