#pragma once

#include <assets/types/2d/base/asset2d.h>
#include <math/anchor.h>

#include <filesystem>
#include <memory>
#include <utility>

namespace CE::Assets {
    struct ResourceProvider;

    /** A whole-texture image with a local pixel-sized quad, suitable for a UI graphic. */
    struct Graphic final : Asset2D {
        Graphic(std::shared_ptr<Geometry2D> geometry, std::shared_ptr<Image> image)
        : Asset2D(std::move(geometry), std::move(image)) {}

        [[nodiscard]] static Graphic
        from_image(std::shared_ptr<Image> image, ResourceProvider& provider, math::Pivot pivot = math::Pivot{0.0f, 0.0f});
        [[nodiscard]] static Graphic
        load(const std::filesystem::path& file, ResourceProvider& provider, math::Pivot pivot = math::Pivot{0.0f, 0.0f});
    };
}
