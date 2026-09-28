#pragma once
#ifndef ASSET2D_H
#define ASSET2D_H
#include <assets/resources/geometry2d.h>

#include <memory>
#include <utility>

namespace CE::Assets {
    /** Shared image and geometry, without a mutable per-object frame selection. */
    struct Asset2D {
        const std::shared_ptr<Geometry2D> geometry;
        const std::shared_ptr<Image> texture;

        Asset2D(std::shared_ptr<Geometry2D> geometry, std::shared_ptr<Image> texture) :
            geometry(std::move(geometry)), texture(std::move(texture)) {}
    };
}
#endif
