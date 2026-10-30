#pragma once

#include <cheryl/export.h>

#include <assets/resources/pipeline.h>
#include <core/rendering/clip-region.h>
#include <backends/opengl/glslprogram.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <cstddef>

namespace CE::Rendering::OpenGL {
    /** One public parameter key mapped to one scalar/vector/matrix/sampler GLSL uniform. */
    struct ParameterBinding {
        std::string key; ///< Nonempty unique key present in the pipeline contract.
        std::string uniform; ///< Nonempty unique GLSL name, reflected against exact type/array storage.
        // Optional active custom uniforms require this or a contract default.
        // A missing engine semantic instead rejects the incomplete packet.
        std::optional<CE::Rendering::Parameters::Value> missing_value; ///< Typed reset for an active optional custom uniform; contract defaults take precedence.
    };

    /** Explicit GLSL names; all active uniforms must be covered and active attributes must match the selected layout. */
    struct PipelineBindings {
        std::vector<ParameterBinding> parameters; ///< One explicit mapping for every public contract key.
        std::string position_attribute = "in_Position"; ///< Active position must be vec3 at location 0.
        std::string uv_attribute = "in_Texcoord"; ///< Active UV must be vec2 at location 1.
        std::string color_attribute = "in_Color"; ///< Active color must be vec4 at location 2 for the colored layout.
    };

    /** Immutable reflected OpenGL pipeline retaining its linked program and reset values.
     * Construction/playback use the program's live owner/current context. Complete
     * requests validate before program/image/fixed draw state is changed; native
     * execution after validation is not an atomic transaction.
     * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
     */
    class CE_TYPE_API Pipeline final : public CE::Rendering::Resources::Pipeline {
        struct BoundParameter {
            std::string key;
            NativeInteger location = -1;
            std::optional<CE::Rendering::Parameters::Value> missing_value;
        };

        const std::shared_ptr<Program> program_;
        std::vector<BoundParameter> parameters_;

    public:
        CE_OPENGL_API Pipeline(CE::Rendering::Pipelines::Definition definition, std::shared_ptr<Program> program, const PipelineBindings& bindings);
        [[nodiscard]] const ResourceLifetime* resource_domain() const noexcept { return program_->resource_domain(); }
        // Check images/defaults without binding or changing any native state.
        CE_OPENGL_API void validate_resources(const CE::Rendering::Parameters::Set& values) const;
        // Applies copied parameter/resource values only; fixed state/pass integration is separate.
        CE_OPENGL_API void bind_parameters(const CE::Rendering::Parameters::Set& values) const;
        // Validate the complete request before mutating any native draw state.
        CE_OPENGL_API void draw(
            const CE::Rendering::Geometry::Buffer2D& geometry,
            std::size_t first_vertex,
            std::size_t vertex_count,
            const CE::Rendering::Parameters::Set& values,
            const CE::Rendering::Pipelines::PassConstraints2D& constraints
        ) const;
        // A clipped draw supplies its playback extent explicitly.
        CE_OPENGL_API void draw(
            const CE::Rendering::Geometry::Buffer2D& geometry,
            std::size_t first_vertex,
            std::size_t vertex_count,
            const CE::Rendering::Parameters::Set& values,
            const CE::Rendering::Pipelines::PassConstraints2D& constraints,
            const std::optional<ClipRegion2D>& clip,
            CE::Platform::FramebufferSize framebuffer
        ) const;

    private:
        [[nodiscard]] CE_OPENGL_API CE::Rendering::Parameters::Set prepare_parameters(const CE::Rendering::Parameters::Set& values) const;
        CE_OPENGL_API void apply_parameters(const CE::Rendering::Parameters::Set& values) const;
        CE_OPENGL_API void apply_fixed_state() const;
    };
}

/**
 * @fn CE::Rendering::OpenGL::Pipeline::Pipeline(CE::Rendering::Pipelines::Definition definition, std::shared_ptr<Program> program, const PipelineBindings& bindings)
 * @brief Validate common recipe structure and reflect a retained linked program.
 * @param definition Owned immutable layout/topology/state/public-parameter contract.
 * @param program Nonnull linked executable in its live owner/current context.
 * @param bindings Borrowed explicit uniform/attribute mappings copied into resolved native locations.
 * Unknown/duplicate mappings, incompatible active attributes/uniforms, missing required uniforms, uncovered active uniforms or optional uniforms lacking resets/defaults throw invalid_args. Native reflection/allocation failures propagate; no pipeline is published.
 */

/**
 * @fn const CE::Rendering::OpenGL::ResourceLifetime* CE::Rendering::OpenGL::Pipeline::resource_domain() const noexcept
 * @return Borrowed native domain identity of the retained program; it does not prove live/current native access.
 */

/**
 * @fn void CE::Rendering::OpenGL::Pipeline::validate_resources(const CE::Rendering::Parameters::Set& values) const
 * @brief Check image/sampler domains, unit limits and requested mipmap completeness without binding.
 * @param values Stable partial or complete values; this method alone does not validate all parameter keys/types/completeness.
 * Requires the program's live owner/current context, even when values contain no images. Foreign/non-OpenGL images/samplers, invalid units or mipmapped sampling of an incomplete image throw invalid_args. Inputs/native binding state remain unchanged.
 */

/**
 * @fn void CE::Rendering::OpenGL::Pipeline::bind_parameters(const CE::Rendering::Parameters::Set& values) const
 * @brief Prepare resets, validate a complete request, then select the program and apply values/images/samplers.
 * @param values Complete required values; active optional omissions receive contract defaults or explicit resets.
 * Requires live owner/current context. Validation/allocation failures occur before binding changes; native application has no rollback. This does not apply fixed blend/depth/cull/scissor state or draw geometry.
 */

/**
 * @fn void CE::Rendering::OpenGL::Pipeline::draw(const CE::Rendering::Geometry::Buffer2D& geometry, std::size_t first_vertex, std::size_t vertex_count, const CE::Rendering::Parameters::Set& values, const CE::Rendering::Pipelines::PassConstraints2D& constraints) const
 * @brief Validate and draw an unclipped range, explicitly disabling scissor state.
 * @param geometry OpenGL VertexArray in this pipeline's live native domain.
 * @param first_vertex Zero-based first vertex.
 * @param vertex_count Nonzero complete-primitive count within the upload.
 * @param values Required resolved values plus optional resets/defaults.
 * @param constraints Pass-state requirements.
 * Validates CPU metadata, native domain/current owner and complete resource values before changing draw state. Then reapplies fixed state, parameters and geometry in order; no rollback of native execution is promised.
 */

/**
 * @fn void CE::Rendering::OpenGL::Pipeline::draw(const CE::Rendering::Geometry::Buffer2D& geometry, std::size_t first_vertex, std::size_t vertex_count, const CE::Rendering::Parameters::Set& values, const CE::Rendering::Pipelines::PassConstraints2D& constraints, const std::optional<ClipRegion2D>& clip, CE::Platform::FramebufferSize framebuffer) const
 * @brief Draw the same validated range with an optional retained logical clip.
 * @param geometry Compatible same-domain OpenGL VertexArray.
 * @param first_vertex First vertex index, not bytes.
 * @param vertex_count Complete-primitive vertex count.
 * @param values Complete request with optional native resets.
 * @param constraints Pass-state requirements.
 * @param clip Retained logical clip; absent disables scissor.
 * @param framebuffer Nonnegative playback pixel extent used only for clip conversion.
 * Clips clamp/round outward and convert top-left edges to native bottom-left scissor. Empty resolved clips return after validation without changing draw state. Errors before native application preserve binding/fixed state; later native execution is not atomic.
 */
