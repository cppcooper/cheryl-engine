#pragma once

#include <assets/types/primitives/pixel.h>

#include <cstdint>

namespace CE::Assets {
    // Immutable uploaded contents/dimensions in the selected backend domain.
    // Retain this handle in frames/materials; changed pixels require a new image.
    // Logical ownership after backend shutdown does not permit native bind/use.
    struct Image {
        virtual ~Image() = default;
        [[nodiscard]] virtual PixelSize pixel_size() const = 0;
        // Binding belongs to a draw/material request; units are zero-based.
        virtual void bind(std::uint32_t unit) const = 0;
    };
}
