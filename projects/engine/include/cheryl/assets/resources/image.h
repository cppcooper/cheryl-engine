#pragma once

#include <cheryl/export.h>

#include <assets/types/primitives/pixel.h>

#include <cstdint>

namespace CE::Rendering::Resources {
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
