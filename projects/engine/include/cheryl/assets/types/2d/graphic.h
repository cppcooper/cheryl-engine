#pragma once

#include <assets/types/2d/base/asset2d.h>
#include <math/anchor.h>

#include <filesystem>
#include <memory>
#include <utility>

namespace CE::Rendering::Resources {
    struct Provider;
}

namespace CE::Assets::Graphics {
    /** A whole-image quad in local Y-up pixels about a normalized top-left pivot.
     * Direct construction retains handles without validating their compatibility.
     */
    struct Asset final : Content2D {
        Asset(std::shared_ptr<CE::Rendering::Geometry::Buffer2D> geometry, std::shared_ptr<CE::Rendering::Resources::Image> image)
        : Content2D(std::move(geometry), std::move(image)) {}

        // Upload on the provider owner with a compatible image domain. Null/empty
        // images throw; no cache entry is published, and CPU vertices are transient.
        [[nodiscard]] static Asset from_image(
            std::shared_ptr<CE::Rendering::Resources::Image> image,
            CE::Rendering::Resources::Provider& provider,
            CE::Rendering::Geometry::Pivot pivot = CE::Rendering::Geometry::Pivot{0.0f, 0.0f}
        );
        // Decode then upload on that same owner; failure returns no CE::Assets::Graphics::Asset.
        [[nodiscard]] static Asset load(
            const std::filesystem::path& file,
            CE::Rendering::Resources::Provider& provider,
            CE::Rendering::Geometry::Pivot pivot = CE::Rendering::Geometry::Pivot{0.0f, 0.0f}
        );
    };
}
