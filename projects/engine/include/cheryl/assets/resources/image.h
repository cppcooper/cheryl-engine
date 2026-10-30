#pragma once

#include <cheryl/export.h>

#include <assets/types/primitives/pixel.h>

#include <cstdint>

namespace CE::Rendering::Resources {
    /** Immutable uploaded contents/dimensions in the selected backend domain.
     * @see [Resource residency](cheryl-guide:resources/resource-residency)
     */
    // Immutable uploaded contents/dimensions in the selected backend domain.
    // Retain this handle in frames/materials; changed pixels require a new image.
    // Logical ownership after backend shutdown does not permit native bind/use.
    struct CE_API Image {
        virtual ~Image() = default;
        // Immutable pixel dimensions; CPU inspection does not bind the resource.
        [[nodiscard]] virtual CE::Rendering::Geometry::PixelSize pixel_size() const = 0;
        // Binding belongs to a draw/material request; units are zero-based. Requires
        // the backend owner/current context and a unit supported by that backend.
        virtual void bind(std::uint32_t unit) const = 0;
    };
}

/**
 * @fn CE::Rendering::Resources::Image::~Image()
 * @brief Release this logical image owner. Native retirement/deletion follows the backend contract; destroying a handle does not promise immediate native deletion.
 */

/**
 * @fn CE::Rendering::Geometry::PixelSize CE::Rendering::Resources::Image::pixel_size() const
 * @brief Read uploaded dimensions in pixels without selecting a texture unit.
 * @return Immutable image width and height. Retain this image through the call; concrete adapters define their synchronization requirements.
 */

/**
 * @fn void CE::Rendering::Resources::Image::bind(std::uint32_t unit) const
 * @brief Bind the image for a draw on its live backend owner/current context.
 * @param unit Zero-based texture-unit request, within the backend limit.
 * Retain the image and its original resource domain through native use. Backend validation/errors may throw; this operation does not choose a material or draw geometry.
 */
