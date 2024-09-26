#pragma once
#ifndef DRAW2D_H
#define DRAW2D_H
#include <assets/primitives.h>
#include "idraw.h"

namespace CE::Assets {
    struct Draw2D : iDraw {
        const GLuint id_vao;
        const std::shared_ptr<Texture> texture;
        Draw2D(const GLuint vao_id, const std::shared_ptr<Texture> &texture)
                : id_vao(vao_id), texture(texture) { }
        ~Draw2D() override = default;
    };
}
#endif
