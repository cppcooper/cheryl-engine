#pragma once
#ifndef ASSET2D_H
#define ASSET2D_H
#include <assets/primitives.h>
#include "idraw.h"

namespace CE::Assets {
    struct Asset2D : iDraw {
        const VAO vao;
        const std::shared_ptr<Texture> texture;
        Asset2D(const std::shared_ptr<Vertex2D> &vertices, uint32_t num_vertices,
                const std::shared_ptr<Texture> &texture)
                : vao(vertices, num_vertices), texture(texture) { }
    };
}
#endif
