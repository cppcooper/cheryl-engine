#pragma once

#include <cheryl/export.h>

#include <glm.hpp>

namespace CE::Rendering::Parameters {
    // Copied engine semantics. Matrices use the caller's coordinate convention;
    // alpha/scale have no common clamping. ShaderDraw::texture_unit is a legacy
    // zero-based request; material parameters use explicit ImageBinding instead.
    struct PassSemantics {
        glm::mat4 projection{1.0f};
        glm::mat4 view{1.0f};
    };
    struct DrawSemantics {
        glm::mat4 model{1.0f};
        float alpha = 1.0f;
        float scale = 1.0f;
        int texture_unit = 0;
    };
}

namespace CE::Rendering::Resources {
    // Mutable executable-program access on the backend owner/current context.
    // Retain the program through use; uniform names/matrices are borrowed for each
    // call. Native type/name validation and failure state belong to the backend.
    struct CE_API Program {
        virtual ~Program() = default;
        // Bind semantic engine parameters; concrete backends choose their representation.
        virtual void bind_pass(const CE::Rendering::Parameters::PassSemantics& pass) = 0;
        virtual void bind_draw(const CE::Rendering::Parameters::DrawSemantics& draw) = 0;
        // Backend/custom parameters remain available to callers that define their own uniforms.
        virtual void use() = 0;
        virtual void set_uniform_value(const char* name, float value) = 0;
        virtual void set_uniform_value(const char* name, int value) = 0;
        virtual void set_uniform_value(const char* name, unsigned int value) = 0;
        virtual void set_uniform_value(const char* name, bool value) = 0;
        virtual void set_uniform_matrix(const char* name, const glm::mat4& value) = 0;
    };
}
