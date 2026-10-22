#pragma once

#include <cheryl/export.h>

#include <assets/resources/resource-provider.h>
#include <assets/resources/shader-asset-builder.h>
#include <backends/opengl/pipeline.h>

#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <utility>
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
        std::map<std::pair<CE::Rendering::Materials::Builtin2D, bool>, std::weak_ptr<CE::Rendering::Resources::Program>> builtin_programs_;
        std::map<
            std::pair<CE::Rendering::Materials::Builtin2D, CE::Rendering::Pipelines::Blend>,
            std::weak_ptr<const CE::Rendering::Resources::Material>>
            builtin_materials_;

    public:
        explicit ResourceProvider(Renderer& renderer)
        : renderer_(renderer) {}

        [[nodiscard]] CE_OPENGL_API ::CE::Rendering::GraphicsDomain graphics_domain() const noexcept override;

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
        [[nodiscard]] CE_OPENGL_API std::shared_ptr<const CE::Rendering::Resources::Material>
        build_builtin_material(CE::Rendering::Materials::Builtin2D kind, CE::Rendering::Pipelines::Blend blend) override;
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

/**
 * @fn CE::Rendering::OpenGL::ResourceProvider::ResourceProvider(Renderer& renderer)
 * @brief Borrow the renderer that defines all created resource domains.
 * @param renderer Initialized before upload calls and kept alive through this provider's use/destruction.
 * Construction performs no upload; weak preset/sampler caches do not pin resources after final external release.
 */

/**
 * @fn CE::Assets::Shaders::Builder* CE::Rendering::OpenGL::ResourceProvider::shader_asset_builder() noexcept
 * @return Borrowed builder implemented by this provider, valid only while the provider lives. Builder construction calls retain normal OpenGL upload affinity.
 */

/**
 * @fn std::shared_ptr<CE::Rendering::Resources::Image> CE::Rendering::OpenGL::ResourceProvider::create_image(const CE::Assets::Images::Decoded& image)
 * @brief Copy top-to-bottom RGBA8 pixels into a complete native image.
 * @param image Nonzero int-representable dimensions with exactly four bytes per pixel.
 * @return Retained OpenGL texture: linear filtering, clamp-to-edge, full mipmaps and available maximum anisotropy.
 * Requires the renderer's initialized live owner/current context. Invalid dimensions/count throw invalid_args; native/allocation errors propagate. CPU pixels are not retained and no cache entry is published.
 */

/**
 * @fn std::shared_ptr<const CE::Rendering::Sampling::Sampler> CE::Rendering::OpenGL::ResourceProvider::create_sampler(const CE::Rendering::Sampling::Options& options)
 * @brief Return a retained immutable sampler for an exact requested-options key.
 * @param options Valid filter/wrap/anisotropy request.
 * @return Existing live weak-cache candidate or a newly created native sampler.
 * Requires initialized live owner/current context; unsupported anisotropy resolves to 1. Failure publishes no candidate; final external release permits cache expiration.
 */

/**
 * @fn std::shared_ptr<CE::Rendering::Resources::Image> CE::Rendering::OpenGL::ResourceProvider::create_font_atlas(std::span<const unsigned char> alpha, CE::Rendering::Geometry::PixelSize size)
 * @brief Upload one-byte glyph alpha with the baker's row/UV order unchanged.
 * @param alpha Exactly width*height bytes, borrowed until return.
 * @param size Nonzero int-representable pixel dimensions.
 * @return Retained red-channel texture swizzled to white RGB/alpha, with linear isotropic base-level clamp sampling.
 * Requires initialized live owner/current context. Invalid data or native/allocation failure rejects creation without cache publication.
 */

/**
 * @fn std::shared_ptr<CE::Rendering::Geometry::Buffer2D> CE::Rendering::OpenGL::ResourceProvider::upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D> vertices, CE::Rendering::Geometry::Topology topology)
 * @brief Copy transient position/UV vertices into an immutable OpenGL VertexArray.
 * @param vertices Nonempty contiguous storage within native buffer/draw size limits, borrowed until return.
 * @param topology Triangles or TriangleStrip, retained by the upload.
 * @return Same-domain geometry retaining no CPU vertex storage.
 * Requires initialized live owner/current context; input/native/allocation failures propagate without cache publication.
 */

/**
 * @fn std::shared_ptr<CE::Rendering::Geometry::Buffer2D> CE::Rendering::OpenGL::ResourceProvider::upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2DColor> vertices, CE::Rendering::Geometry::Topology topology)
 * @brief Copy transient position/UV/RGBA vertices into an immutable OpenGL VertexArray.
 * @param vertices Nonempty contiguous storage within native buffer/draw size limits, borrowed until return.
 * @param topology Triangles or TriangleStrip, retained by the upload.
 * @return Same-domain geometry retaining no CPU vertex storage.
 * Requires initialized live owner/current context; input/native/allocation failures propagate without cache publication.
 */

/**
 * @fn std::shared_ptr<CE::Rendering::Resources::Program> CE::Rendering::OpenGL::ResourceProvider::link_program(const std::vector<std::filesystem::path>& stages)
 * @brief Read, compile and link shader stage files into a tracked OpenGL executable.
 * @param stages Stage paths borrowed through the call; extension mapping follows the native program builder.
 * @return Complete retained Program in this renderer's native domain.
 * Requires initialized live owner/current context. File/compile/link/native/allocation failures retire adopted transient objects and return no executable/cache publication.
 */

/**
 * @fn std::shared_ptr<const CE::Rendering::OpenGL::Pipeline> CE::Rendering::OpenGL::ResourceProvider::build_pipeline(CE::Rendering::Pipelines::Definition definition, const PipelineBindings& bindings)
 * @brief Validate the recipe, compile its stage files and reflect an immutable native pipeline.
 * @param definition Owned pipeline snapshot consumed by value.
 * @param bindings Stable explicit uniform/attribute mappings borrowed through construction.
 * @return Complete same-domain pipeline, without cache publication.
 * Requires initialized live owner/current context. Common validation, file/compile/link/reflection/native/allocation failures return no pipeline; existing generations remain unchanged.
 */

/**
 * @fn std::shared_ptr<const CE::Rendering::Resources::Material> CE::Rendering::OpenGL::ResourceProvider::build_material(CE::Rendering::Materials::Definition definition)
 * @brief Validate and retain a material using this provider's OpenGL pipeline/sampler domain.
 * @param definition Owned recipe with required same-domain OpenGL pipeline and optional same-domain samplers.
 * @return Complete immutable material after validating default image bindings.
 * Requires initialized live owner/current context. Foreign/null resources or invalid defaults reject before publication; native/allocation failures propagate.
 * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
 */
