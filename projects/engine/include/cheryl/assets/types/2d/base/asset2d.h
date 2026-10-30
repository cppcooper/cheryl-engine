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
        const std::shared_ptr<CE::Rendering::Geometry::Buffer2D> geometry; ///< Retained immutable uploaded geometry; null is permitted by direct construction.
        const std::shared_ptr<CE::Rendering::Resources::Image> texture; ///< Retained image contents in the same usable backend domain as the geometry.
        const std::shared_ptr<const CE::Rendering::Resources::Material> material; ///< Optional retained material recipe/handle; consumers choose fallback when absent.

        Content2D(
            std::shared_ptr<CE::Rendering::Geometry::Buffer2D> geometry,
            std::shared_ptr<CE::Rendering::Resources::Image> texture,
            std::shared_ptr<const CE::Rendering::Resources::Material> material = {}
        )
        : geometry(std::move(geometry)), texture(std::move(texture)), material(std::move(material)) {}
    };
}

#endif

/**
 * @fn CE::Assets::Content2D::Content2D(std::shared_ptr<CE::Rendering::Geometry::Buffer2D> geometry, std::shared_ptr<CE::Rendering::Resources::Image> texture, std::shared_ptr<const CE::Rendering::Resources::Material> material)
 * @brief Retain uploaded handles without selecting a frame or validating compatibility.
 * @param geometry Retained geometry, possibly null for manually assembled values.
 * @param texture Retained image, possibly null for manually assembled values.
 * @param material Optional retained material; this constructor creates no default or native resource.
 * Handles can outlive caches, but native use still requires their original live backend domain. Loader/submission paths validate their own inputs.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#resources-and-caches)
 */
