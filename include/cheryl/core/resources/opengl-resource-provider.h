#pragma once

#include <assets/abstracts/resource-provider.h>

namespace CE::RenderAPIs {
    struct OpenGLRenderer;
}

namespace CE::Assets {
    class OpenGLResourceProvider final : public ResourceProvider {
    public:
        explicit OpenGLResourceProvider(RenderAPIs::OpenGLRenderer& renderer) : renderer_(renderer) {}

        [[nodiscard]] std::shared_ptr<Image> load_image(const std::filesystem::path& file) override;
        [[nodiscard]] std::shared_ptr<Image> create_font_atlas(std::span<const unsigned char> alpha,
                                                               PixelSize size) override;
        [[nodiscard]] std::shared_ptr<Geometry2D> upload_geometry(std::shared_ptr<Vertex2D> vertices,
                                                                   std::uint32_t vertex_count) override;
        [[nodiscard]] std::shared_ptr<Shader> compile_stage(const std::filesystem::path& file) override;
        [[nodiscard]] std::shared_ptr<Shader> link_program(
            const std::vector<std::filesystem::path>& stages) override;

    private:
        RenderAPIs::OpenGLRenderer& renderer_;
    };
}
