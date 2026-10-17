#pragma once

#include <cheryl/export.h>

#include <assets/resources/resource-provider.h>
#include <assets/resources/shader-asset-builder.h>
#include <backends/opengl/pipeline.h>

#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <vector>

namespace CE::Rendering::OpenGL {
    struct Renderer;

    /** Translate backend-neutral resource requests into OpenGL images, buffers, and programs.
     * Calls require the rendering context current on the calling thread; CPU data is copied
     * during construction and need not outlive each call.
     */
    class CE_TYPE_API ResourceProvider final : public CE::Rendering::Resources::Provider,
                                   private CE::Assets::Shaders::Builder {
        Renderer& renderer_;
        std::map<CE::Rendering::Sampling::Options, std::weak_ptr<const CE::Rendering::Sampling::Sampler>> samplers_;

    public:
        explicit ResourceProvider(Renderer& renderer)
        : renderer_(renderer) {}

        [[nodiscard]] CE::Assets::Shaders::Builder* shader_asset_builder() noexcept override { return this; }

        [[nodiscard]] CE_OPENGL_API std::shared_ptr<CE::Rendering::Resources::Image> create_image(const CE::Assets::Images::Decoded& image) override;
        [[nodiscard]] CE_OPENGL_API std::shared_ptr<const CE::Rendering::Sampling::Sampler>
        create_sampler(const CE::Rendering::Sampling::Options& options) override;
        [[nodiscard]] CE_OPENGL_API std::shared_ptr<CE::Rendering::Resources::Image>
        create_font_atlas(std::span<const unsigned char> alpha, CE::Rendering::Geometry::PixelSize size) override;
        using CE::Rendering::Resources::Provider::upload_geometry;
        [[nodiscard]] CE_OPENGL_API std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
        upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D> vertices, CE::Rendering::Geometry::Topology topology) override;
        [[nodiscard]] CE_OPENGL_API std::shared_ptr<CE::Rendering::Geometry::Buffer2D> upload_geometry(
            std::span<const CE::Rendering::Geometry::Vertex2DColor> vertices,
            CE::Rendering::Geometry::Topology topology
        ) override;
        [[nodiscard]] CE_OPENGL_API std::shared_ptr<CE::Rendering::Resources::Program>
        link_program(const std::vector<std::filesystem::path>& stages) override;
        [[nodiscard]] CE_OPENGL_API std::shared_ptr<const Pipeline>
        build_pipeline(CE::Rendering::Pipelines::Definition definition, const PipelineBindings& bindings);
        [[nodiscard]] CE_OPENGL_API std::shared_ptr<const CE::Rendering::Resources::Material>
        build_material(CE::Rendering::Materials::Definition definition);

    private:
        CE_OPENGL_API void validate_program(const CE::Assets::Shaders::ProgramRecipe& recipe) const override;
        CE_OPENGL_API void validate_material(
            const CE::Assets::Shaders::MaterialRecipe& recipe,
            const CE::Assets::Shaders::ProgramRecipe& program
        ) const override;
        [[nodiscard]] CE_OPENGL_API std::shared_ptr<CE::Rendering::Resources::Program>
        build_program(const CE::Assets::Shaders::ProgramRecipe& recipe) override;
        [[nodiscard]] CE_OPENGL_API std::shared_ptr<const CE::Rendering::Resources::Material> build_material(
            const CE::Assets::Shaders::MaterialRecipe& recipe,
            const CE::Assets::Shaders::ProgramRecipe& program,
            const std::shared_ptr<CE::Rendering::Resources::Program>& executable
        ) override;
    };
}
