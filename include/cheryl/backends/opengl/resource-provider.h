#pragma once

#include <assets/resources/resource-provider.h>

namespace CE::RenderAPIs {
    struct OpenGLRenderer;
}

namespace CE::Assets {
    /** Translate backend-neutral resource requests into OpenGL images, buffers, and programs.
     * Calls require the rendering context current on the calling thread; CPU data is copied
     * during construction and need not outlive each call.
     */
    class OpenGLResourceProvider final : public ResourceProvider {
    public:
        explicit OpenGLResourceProvider(RenderAPIs::OpenGLRenderer& renderer) : renderer_(renderer) {}

        [[nodiscard]] std::shared_ptr<Image> create_image(const DecodedImage& image) override;
        [[nodiscard]] std::shared_ptr<Image> create_font_atlas(std::span<const unsigned char> alpha, PixelSize size) override;
        using ResourceProvider::upload_geometry;
        [[nodiscard]] std::shared_ptr<Geometry2D> upload_geometry(std::span<const Vertex2D> vertices, PrimitiveTopology topology) override;
        [[nodiscard]] std::shared_ptr<Shader> link_program(const std::vector<std::filesystem::path>& stages) override;

    private:
        RenderAPIs::OpenGLRenderer& renderer_;
    };
}
