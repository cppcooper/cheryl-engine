#pragma once
#ifndef ASSET2D_H
#define ASSET2D_H
#include "geometry2d.h"
#include "idraw.h"

#include <memory>
#include <utility>

namespace CE::Assets {
    // TODO: Clarify the intended distinction between Asset2D and Draw2D. Both currently implement
    // iDraw and own the same Geometry2D/Image pair, so two parallel bases can drift without adding
    // an obvious semantic boundary.
    struct Asset2D : iDraw {
        const std::shared_ptr<Geometry2D> geometry;
        const std::shared_ptr<Image> texture;

        Asset2D(std::shared_ptr<Geometry2D> geometry, std::shared_ptr<Image> texture) :
            geometry(std::move(geometry)), texture(std::move(texture)) {}
    };
}
#endif
