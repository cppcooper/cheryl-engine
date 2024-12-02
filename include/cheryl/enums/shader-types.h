#pragma once

namespace CE::Enum {
    enum ShaderTypes {
        VERTEX,
        FRAGMENT,
        GEOMETRY,
        TESS_CONTROL,
        TESS_EVALUATION
    };

    inline ShaderTypes get_shader_type(const std::string& extension) {
        if (extension == ".vert") {
            return VERTEX;
        }
        if (extension == ".frag") {
            return FRAGMENT;
        }
        if (extension == ".geo") {
            return GEOMETRY;
        }
        if (extension == ".tesc") {
            return TESS_CONTROL;
        }
        if (extension == ".tese") {
            return TESS_EVALUATION;
        }
        return FRAGMENT;  // Default case
    }
}
