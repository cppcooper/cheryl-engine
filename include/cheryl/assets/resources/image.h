#pragma once

#include <assets/manifest.h>

namespace CE::Assets {
    // The dimensions of an image uploaded by the selected rendering backend.
    struct Image {
        virtual ~Image() = default;
        [[nodiscard]] virtual PixelSize pixel_size() const = 0;
    };
}
