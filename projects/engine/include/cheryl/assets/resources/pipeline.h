#pragma once

#include <cheryl/export.h>

#include <assets/definitions/pipeline.h>

#include <cstddef>

namespace CE::Rendering::Resources {
    /** Immutable noncopyable pipeline metadata plus a backend-owned executable implementation.
     * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
     */
    // A backend subclass retains its executable resource. Construction takes a
    // definition snapshot; publishing another instance creates another generation.
    class CE_TYPE_API Pipeline {
        const CE::Rendering::Pipelines::Definition definition_;

    protected:
        CE_API explicit Pipeline(CE::Rendering::Pipelines::Definition definition);

    public:
        CE_API virtual ~Pipeline();
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;
        // Borrowed immutable metadata, valid while this Pipeline is retained.
        [[nodiscard]] const CE::Rendering::Pipelines::Definition& definition() const { return definition_; }
        // CPU metadata check: matching layout/topology, nonempty bounded vertex range,
        // complete primitives and pass constraints. No native bind/domain check.
        CE_API void validate_draw(
            const CE::Rendering::Geometry::Buffer2D& geometry,
            std::size_t first_vertex,
            std::size_t vertex_count,
            const CE::Rendering::Pipelines::PassConstraints2D& constraints
        ) const;
    };

    /** Immutable recipe retaining its pipeline/default images. Read-only resolution
     * can run on CPU owners; it does not bind resources or mutate uniform state.
     */
    class Material final {
        const CE::Rendering::Materials::Definition definition_;

    public:
        // A null pipeline or malformed defaults throw before a Material is returned.
        CE_API explicit Material(CE::Rendering::Materials::Definition definition);
        // Borrowed metadata, valid while this Material is retained.
        [[nodiscard]] const CE::Rendering::Materials::Definition& definition() const { return definition_; }
        // Owned resolved copy; validation failure leaves the recipe and caller inputs intact.
        [[nodiscard]] CE_API CE::Rendering::Parameters::Set resolve(
            const CE::Rendering::Parameters::PassSemantics& pass_semantics,
            const CE::Rendering::Parameters::DrawSemantics& draw_semantics,
            const CE::Rendering::Parameters::Set& pass_values,
            const CE::Rendering::Parameters::Set& draw_values
        ) const;
    };
}

/**
 * @fn CE::Rendering::Resources::Pipeline::Pipeline(CE::Rendering::Pipelines::Definition definition)
 * @brief Validate and retain an owned CPU recipe snapshot.
 * @param definition Copied/moved program-source, state, layout and public parameter contract.
 * @throws CE::Exceptions::invalid_args If common structural validation rejects the recipe.
 * Native compilation/reflection belongs to the derived backend constructor.
 */

/**
 * @fn CE::Rendering::Resources::Pipeline::~Pipeline()
 * @brief Release the pipeline generation; retained materials/frames keep their own ownership until released.
 */

/**
 * @fn const CE::Rendering::Pipelines::Definition& CE::Rendering::Resources::Pipeline::definition() const
 * @return Borrowed immutable recipe; keep this pipeline alive while using the reference. This is CPU metadata, not a native bind.
 */

/**
 * @fn void CE::Rendering::Resources::Pipeline::validate_draw(const CE::Rendering::Geometry::Buffer2D& geometry, std::size_t first_vertex, std::size_t vertex_count, const CE::Rendering::Pipelines::PassConstraints2D& constraints) const
 * @brief Validate CPU layout/topology, range, primitive completeness and pass-state compatibility.
 * @param geometry Stable uploaded metadata borrowed for this call.
 * @param first_vertex Zero-based first vertex.
 * @param vertex_count Nonzero number of vertices inside the upload; triangles require multiples of three, strips at least three.
 * @param constraints Required fixed-state choices; absent constraints impose no requirement.
 * @throws CE::Exceptions::invalid_args If metadata/range/state does not match.
 * No native domain, context, program or image checks occur; inputs remain unchanged.
 */

/**
 * @fn CE::Rendering::Resources::Material::Material(CE::Rendering::Materials::Definition definition)
 * @brief Validate and retain a complete immutable material recipe.
 * @param definition Required pipeline, partial custom defaults and optional sampler-key overrides.
 * @throws CE::Exceptions::invalid_args For a null pipeline, invalid defaults or a null/non-sampler override.
 * Construction does not upload or bind native resources. Backend/domain compatibility requires the backend builder/playback path.
 */

/**
 * @fn const CE::Rendering::Materials::Definition& CE::Rendering::Resources::Material::definition() const
 * @return Borrowed immutable recipe and retained handles, valid while this material is retained.
 */

/**
 * @fn CE::Rendering::Parameters::Set CE::Rendering::Resources::Material::resolve(const CE::Rendering::Parameters::PassSemantics& pass_semantics, const CE::Rendering::Parameters::DrawSemantics& draw_semantics, const CE::Rendering::Parameters::Set& pass_values, const CE::Rendering::Parameters::Set& draw_values) const
 * @brief Resolve owned CPU packet values without binding resources or mutating uniforms.
 * @param pass_semantics Copied projection/view source.
 * @param draw_semantics Copied model/alpha/scale source.
 * @param pass_values Custom pass layer, below this material's defaults.
 * @param draw_values Highest-priority custom layer.
 * @return Complete resolved set retaining images/samplers. Explicit binding samplers win over this material's sampling overrides.
 * All layers validate even when overridden; failures leave the recipe and inputs unchanged. Resolution requires stable inputs, not a graphics context.
 * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
 */
