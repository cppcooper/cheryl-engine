#pragma once
#ifndef ASSET2D_H
#define ASSET2D_H
#include <assets/resources/geometry2d.h>
#include <assets/resources/image.h>

#include <memory>
#include <utility>

namespace CE::Assets {
    class Material;
    /** Retains immutable uploaded contents without a per-object frame selection.
     * Direct construction permits null handles; loaders/submission validate their
     * own inputs. Native use still requires the original live backend domain.
     */
    struct Asset2D {
        const std::shared_ptr<Geometry2D> geometry;
        const std::shared_ptr<Image> texture;
        const std::shared_ptr<const Material> material;

        Asset2D(std::shared_ptr<Geometry2D> geometry, std::shared_ptr<Image> texture, std::shared_ptr<const Material> material = {})
        : geometry(std::move(geometry)), texture(std::move(texture)), material(std::move(material)) {}
    };
}
#endif
