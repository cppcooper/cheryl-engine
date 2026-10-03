#include <assets/types/2d/graphic.h>

#include <assets/resources/resource-provider.h>
#include <assets/types/primitives/vertex.h>
#include <internals/exceptions.h>

#include <utility>

namespace CE::Assets {
    Graphic Graphic::from_image(std::shared_ptr<Image> image, ResourceProvider& provider, const math::Pivot pivot) {
        if (!image)
            throw Exceptions::invalid_args(CE_HERE, "A graphic requires an image");
        const auto size = image->pixel_size();
        if (size.width == 0 || size.height == 0) {
            throw Exceptions::invalid_args(CE_HERE, "A graphic requires non-zero image dimensions");
        }

        // The whole image supplies the quad's dimensions and [0, 1] texture coordinates.
        // Upload copies its six vertices before this local owner is released.
        auto quad = std::make_shared<Quad>(math::Anchor::MakeQuad(pivot, size.width, size.height, size.width, size.height));
        std::shared_ptr<Vertex2D> vertices(quad, quad->vertices.data());
        auto geometry = provider.upload_geometry(std::move(vertices), VAONumbers::vertices_per_quad, PrimitiveTopology::Triangles);
        return {std::move(geometry), std::move(image)};
    }

    Graphic Graphic::load(const std::filesystem::path& file, ResourceProvider& provider, const math::Pivot pivot) {
        return from_image(provider.load_image(file), provider, pivot);
    }

}
