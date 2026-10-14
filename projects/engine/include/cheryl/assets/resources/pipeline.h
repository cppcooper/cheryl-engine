#pragma once

#include <assets/definitions/pipeline.h>

#include <cstddef>

namespace CE::Rendering::Resources {
    // A backend subclass retains its executable resource. Construction takes a
    // definition snapshot; publishing another instance creates another generation.
    class Pipeline {
        const CE::Rendering::Pipelines::Definition definition_;

    protected:
        explicit Pipeline(CE::Rendering::Pipelines::Definition definition);

    public:
        virtual ~Pipeline() = default;
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;
        // Borrowed immutable metadata, valid while this Pipeline is retained.
        [[nodiscard]] const CE::Rendering::Pipelines::Definition& definition() const { return definition_; }
        // CPU metadata check: matching layout/topology, nonempty bounded vertex range,
        // complete primitives and pass constraints. No native bind/domain check.
        void validate_draw(
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
        explicit Material(CE::Rendering::Materials::Definition definition);
        // Borrowed metadata, valid while this Material is retained.
        [[nodiscard]] const CE::Rendering::Materials::Definition& definition() const { return definition_; }
        // Owned resolved copy; validation failure leaves the recipe and caller inputs intact.
        [[nodiscard]] CE::Rendering::Parameters::Set resolve(
            const CE::Rendering::Parameters::PassSemantics& pass_semantics,
            const CE::Rendering::Parameters::DrawSemantics& draw_semantics,
            const CE::Rendering::Parameters::Set& pass_values,
            const CE::Rendering::Parameters::Set& draw_values
        ) const;
    };
}
