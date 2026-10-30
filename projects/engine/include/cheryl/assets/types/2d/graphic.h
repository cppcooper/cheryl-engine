#pragma once

#include <cheryl/export.h>

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
        Asset(
            std::shared_ptr<CE::Rendering::Geometry::Buffer2D> geometry,
            std::shared_ptr<CE::Rendering::Resources::Image> image,
            std::shared_ptr<const CE::Rendering::Resources::Material> material = {}
        )
        : Content2D(std::move(geometry), std::move(image), std::move(material)) {}

        // Upload on the provider owner with a compatible image domain. Null/empty
        // images throw; no cache entry is published, and CPU vertices are transient.
        [[nodiscard]] CE_API static Asset from_image(
            std::shared_ptr<CE::Rendering::Resources::Image> image,
            CE::Rendering::Resources::Provider& provider,
            CE::Rendering::Geometry::Pivot pivot = CE::Rendering::Geometry::Pivot{0.0f, 0.0f},
            std::shared_ptr<const CE::Rendering::Resources::Material> material = {}
        );
        // Decode then upload on that same owner; failure returns no CE::Assets::Graphics::Asset.
        [[nodiscard]] CE_API static Asset load(
            const std::filesystem::path& file,
            CE::Rendering::Resources::Provider& provider,
            CE::Rendering::Geometry::Pivot pivot = CE::Rendering::Geometry::Pivot{0.0f, 0.0f},
            std::shared_ptr<const CE::Rendering::Resources::Material> material = {}
        );
    };
}

/**
 * @fn CE::Assets::Graphics::Asset::Asset(std::shared_ptr<CE::Rendering::Geometry::Buffer2D> geometry, std::shared_ptr<CE::Rendering::Resources::Image> image, std::shared_ptr<const CE::Rendering::Resources::Material> material)
 * @brief Retain an explicitly assembled whole-image value without compatibility/null validation.
 * @param geometry Uploaded quad geometry.
 * @param image Uploaded image contents.
 * @param material Optional explicit material; this direct constructor creates none.
 * Use from_image()/load() for ordinary default-material creation and upload.
 * @see [Detailed guide](cheryl-guide:getting-started/how-to/draw-sprite)
 */

/**
 * @fn CE::Assets::Graphics::Asset CE::Assets::Graphics::Asset::from_image(std::shared_ptr<CE::Rendering::Resources::Image> image, CE::Rendering::Resources::Provider& provider, CE::Rendering::Geometry::Pivot pivot, std::shared_ptr<const CE::Rendering::Resources::Material> material)
 * @brief Upload a six-vertex whole-image quad on the provider's owner/current context.
 * @param image Non-null, nonempty image compatible with the provider's backend domain.
 * @param provider Borrowed resource owner for geometry/default material creation.
 * @param pivot Finite normalized top-left-origin origin; geometry uses local Y-up image pixels.
 * @param material Optional override; absent requests the provider's builtin textured material.
 * @return Retained geometry/image/material asset, with no cache publication.
 * Invalid image/pivot or backend/default-material/upload failures propagate. CPU vertices are copied during upload and not borrowed by the result.
 * @see [Detailed guide](cheryl-guide:getting-started/how-to/draw-sprite)
 */

/**
 * @fn CE::Assets::Graphics::Asset CE::Assets::Graphics::Asset::load(const std::filesystem::path& file, CE::Rendering::Resources::Provider& provider, CE::Rendering::Geometry::Pivot pivot, std::shared_ptr<const CE::Rendering::Resources::Material> material)
 * @brief Decode/upload an image and construct its whole-image quad on the provider owner.
 * @param file Source path; relative paths use the caller's working directory.
 * @param provider Live compatible resource provider, borrowed for the operation.
 * @param pivot Finite normalized top-left-origin pivot.
 * @param material Optional override; absent selects the provider textured default.
 * @return Complete retained asset; failures return no asset and publish no cache entry.
 * File decoding can block; preparation/upload obey provider affinity and exception contracts.
 * @see [Detailed guide](cheryl-guide:getting-started/how-to/draw-sprite)
 */
