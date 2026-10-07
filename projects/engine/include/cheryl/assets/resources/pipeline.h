#pragma once

#include <assets/definitions/pipeline.h>

#include <cstddef>

namespace CE::Assets {
    // A backend subclass retains its executable resource. Construction takes a
    // definition snapshot; publishing another instance creates another generation.
    class Pipeline {
        const PipelineDefinition definition_;

    protected:
        explicit Pipeline(PipelineDefinition definition);

    public:
        virtual ~Pipeline() = default;
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;
        // Borrowed immutable metadata, valid while this Pipeline is retained.
        [[nodiscard]] const PipelineDefinition& definition() const { return definition_; }
        // CPU metadata check: matching layout/topology, nonempty bounded vertex range,
        // complete primitives and pass constraints. No native bind/domain check.
        void validate_draw(
            const Geometry2D& geometry,
            std::size_t first_vertex,
            std::size_t vertex_count,
            const PassConstraints2D& constraints
        ) const;
    };

    /** Immutable recipe retaining its pipeline/default images. Read-only resolution
     * can run on CPU owners; it does not bind resources or mutate uniform state.
     */
    class Material final {
        const MaterialDefinition definition_;

    public:
        // A null pipeline or malformed defaults throw before a Material is returned.
        explicit Material(MaterialDefinition definition);
        // Borrowed metadata, valid while this Material is retained.
        [[nodiscard]] const MaterialDefinition& definition() const { return definition_; }
        // Owned resolved copy; validation failure leaves the recipe and caller inputs intact.
        [[nodiscard]] ParameterSet resolve(
            const ShaderPass& pass_semantics,
            const ShaderDraw& draw_semantics,
            const ParameterSet& pass_values,
            const ParameterSet& draw_values
        ) const;
    };
}
