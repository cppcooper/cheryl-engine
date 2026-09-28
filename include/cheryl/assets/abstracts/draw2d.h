#pragma once
#ifndef DRAW2D_H
#define DRAW2D_H
#include "geometry2d.h"
#include "idraw.h"

#include <memory>
#include <utility>

namespace CE::Assets {
    // Immediate drawing remains for legacy tile values. Published RenderFrame commands
    // do not call this interface from the simulation thread.
    struct Draw2D : iDraw {
        const std::shared_ptr<Geometry2D> geometry;
        const std::shared_ptr<Image> texture;

        Draw2D(std::shared_ptr<Geometry2D> geometry, std::shared_ptr<Image> texture) :
            geometry(std::move(geometry)), texture(std::move(texture)) {}
        ~Draw2D() override = default;
    };
}
#endif
