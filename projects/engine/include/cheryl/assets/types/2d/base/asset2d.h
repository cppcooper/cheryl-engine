#pragma once
#ifndef ASSET2D_H
#define ASSET2D_H
#include <assets/resources/geometry2d.h>
#include <assets/resources/image.h>

#include <memory>
#include <utility>

namespace CE::Rendering::Resources {
    class Material;
}

namespace CE::Assets {
    /** Retains immutable uploaded contents without a per-object frame selection.
     * Direct construction permits null handles; loaders/submission validate their
     * own inputs. Native use still requires the original live backend domain.
     */
    struct Content2D {
        const std::shared_ptr<CE::Rendering::Geometry::Buffer2D> geometry;
        const std::shared_ptr<CE::Rendering::Resources::Image> texture;
        const std::shared_ptr<const CE::Rendering::Resources::Material> material;

        Content2D(
            std::shared_ptr<CE::Rendering::Geometry::Buffer2D> geometry,
            std::shared_ptr<CE::Rendering::Resources::Image> texture,
            std::shared_ptr<const CE::Rendering::Resources::Material> material = {}
        )
        : geometry(std::move(geometry)), texture(std::move(texture)), material(std::move(material)) {}
    };
}

#endif
