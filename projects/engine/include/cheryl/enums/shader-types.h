#pragma once
#include <string>

namespace CE::Rendering {
    enum ShaderStage { VERTEX, FRAGMENT, GEOMETRY, TESS_CONTROL, TESS_EVALUATION };

    inline ShaderStage shader_stage_from_extension(const std::string& extension) {
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
        return FRAGMENT; // Default case
    }
}
