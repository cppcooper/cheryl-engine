#pragma once

#include <assets/types/2d/base/asset2d.h>
#include <math/anchor.h>

#include <filesystem>
#include <memory>
#include <utility>

namespace CE::Assets {
    struct ResourceProvider;

    /** A whole-image quad in local Y-up pixels about a normalized top-left pivot.
     * Direct construction retains handles without validating their compatibility.
     */
    struct Graphic final : Asset2D {
        Graphic(std::shared_ptr<Geometry2D> geometry, std::shared_ptr<Image> image)
        : Asset2D(std::move(geometry), std::move(image)) {}

        // Upload on the provider owner with a compatible image domain. Null/empty
        // images throw; no cache entry is published, and CPU vertices are transient.
        [[nodiscard]] static Graphic
        from_image(std::shared_ptr<Image> image, ResourceProvider& provider, math::Pivot pivot = math::Pivot{0.0f, 0.0f});
        // Decode then upload on that same owner; failure returns no Graphic.
        [[nodiscard]] static Graphic
        load(const std::filesystem::path& file, ResourceProvider& provider, math::Pivot pivot = math::Pivot{0.0f, 0.0f});
    };
}
