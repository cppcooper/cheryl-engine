#pragma once

#include <glm.hpp>

namespace CE::Assets {
    // Parameters supplied by draw calls to the selected rendering backend.
    struct Shader {
        virtual ~Shader() = default;
        virtual void use() = 0;
        virtual void set_uniform_value(const char* name, float value) = 0;
        virtual void set_uniform_value(const char* name, int value) = 0;
        virtual void set_uniform_value(const char* name, unsigned int value) = 0;
        virtual void set_uniform_value(const char* name, bool value) = 0;
        virtual void set_uniform_matrix(const char* name, const glm::mat4& value) = 0;
    };
}
