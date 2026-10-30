#include <assets/types/2d/graphic.h>

#include <assets/resources/resource-provider.h>
#include <assets/resources/shader-asset-builder.h>
#include <assets/types/primitives/vertex.h>
#include <internals/exceptions.h>

#include <utility>

namespace CE::Assets::Graphics {
    Asset Asset::from_image(
        std::shared_ptr<CE::Rendering::Resources::Image> image,
        CE::Rendering::Resources::Provider& provider,
        const CE::Rendering::Geometry::Pivot pivot,
        std::shared_ptr<const CE::Rendering::Resources::Material> material
    ) {
        if (!image)
            throw Exceptions::invalid_args(CE_HERE, "A graphic requires an image");
        const auto size = image->pixel_size();
        if (size.width == 0 || size.height == 0) {
            throw Exceptions::invalid_args(CE_HERE, "A graphic requires non-zero image dimensions");
        }
        if (!material)
            material = CE::Assets::Shaders::build_builtin_material(provider, CE::Rendering::Materials::Builtin2D::Textured);

        // The whole image supplies the quad's dimensions and [0, 1] texture coordinates.
        // Upload copies its six vertices before this local owner is released.
        auto quad = std::make_shared<CE::Rendering::Geometry::Quad>(
            CE::Rendering::Geometry::Anchor::MakeQuad(pivot, size.width, size.height, size.width, size.height)
        );
        std::shared_ptr<CE::Rendering::Geometry::Vertex2D> vertices(quad, quad->vertices.data());
        auto geometry = provider.upload_geometry(
            std::move(vertices), CE::Rendering::Geometry::vertices_per_quad, CE::Rendering::Geometry::Topology::Triangles
        );
        return {std::move(geometry), std::move(image), std::move(material)};
    }

    Asset Asset::load(
        const std::filesystem::path& file,
        CE::Rendering::Resources::Provider& provider,
        const CE::Rendering::Geometry::Pivot pivot,
        std::shared_ptr<const CE::Rendering::Resources::Material> material
    ) {
        return from_image(provider.load_image(file), provider, pivot, std::move(material));
    }
}
