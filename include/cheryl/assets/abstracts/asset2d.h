#pragma once
#ifndef ASSET2D_H
#define ASSET2D_H
#include "geometry2d.h"
#include "idraw.h"

#include <memory>
#include <utility>

namespace CE::Assets {
    struct Asset2D : iDraw {
        const std::shared_ptr<Geometry2D> geometry;
        const std::shared_ptr<Image> texture;

        Asset2D(std::shared_ptr<Geometry2D> geometry, std::shared_ptr<Image> texture) :
            geometry(std::move(geometry)), texture(std::move(texture)) {}
    };
}
#endif
