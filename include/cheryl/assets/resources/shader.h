#pragma once

#include <glm.hpp>

namespace CE::Assets {
    struct ShaderPass {
        glm::mat4 projection{1.0f};
        glm::mat4 view{1.0f};
    };
    struct ShaderDraw {
        glm::mat4 model{1.0f};
        float alpha = 1.0f;
        float scale = 1.0f;
        int texture_unit = 0;
    };

    // Parameters supplied by draw calls to the selected rendering backend.
    struct Shader {
        virtual ~Shader() = default;
        // Bind semantic engine parameters; concrete backends choose their representation.
        virtual void bind_pass(const ShaderPass& pass) = 0;
        virtual void bind_draw(const ShaderDraw& draw) = 0;
        // Backend/custom parameters remain available to callers that define their own uniforms.
        virtual void use() = 0;
        virtual void set_uniform_value(const char* name, float value) = 0;
        virtual void set_uniform_value(const char* name, int value) = 0;
        virtual void set_uniform_value(const char* name, unsigned int value) = 0;
        virtual void set_uniform_value(const char* name, bool value) = 0;
        virtual void set_uniform_matrix(const char* name, const glm::mat4& value) = 0;
    };
}
