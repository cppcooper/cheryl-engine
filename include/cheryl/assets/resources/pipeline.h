#pragma once

#include <assets/definitions/pipeline.h>

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
        [[nodiscard]] const PipelineDefinition& definition() const { return definition_; }
    };

    class Material final {
        const MaterialDefinition definition_;

    public:
        explicit Material(MaterialDefinition definition);
        [[nodiscard]] const MaterialDefinition& definition() const { return definition_; }
        [[nodiscard]] ParameterSet resolve(
            const ShaderPass& pass_semantics,
            const ShaderDraw& draw_semantics,
            const ParameterSet& pass_values,
            const ParameterSet& draw_values
        ) const;
    };
}
