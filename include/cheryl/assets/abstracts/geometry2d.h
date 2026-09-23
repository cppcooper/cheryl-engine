#pragma once

#include "image.h"

#include <cstddef>

namespace CE::Assets {
    // A backend-owned vertex buffer. The image must come from the same backend.
    struct Geometry2D {
        virtual ~Geometry2D() = default;
        virtual void bind(const Image& image) const = 0;
        virtual void draw(std::size_t first_vertex, std::size_t vertex_count) const = 0;
    };
}
