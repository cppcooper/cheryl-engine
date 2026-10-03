#pragma once

#include <assets/types/primitives/pixel.h>

#include <cstdint>

namespace CE::Assets {
    // The dimensions of an image uploaded by the selected rendering backend.
    struct Image {
        virtual ~Image() = default;
        [[nodiscard]] virtual PixelSize pixel_size() const = 0;
        // Binding belongs to a draw/material request; units are zero-based.
        virtual void bind(std::uint32_t unit) const = 0;
    };
}
