#pragma once
#include <string>

namespace CE::Rendering {
    /** Legacy native stage identifiers; portable shader recipes support only vertex/fragment stages. */
    enum ShaderStage { VERTEX, ///< Vertex stage.
        FRAGMENT, ///< Fragment stage.
        GEOMETRY, ///< Geometry stage.
        TESS_CONTROL, ///< Tessellation control stage.
        TESS_EVALUATION ///< Tessellation evaluation stage.
    };

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

/**
 * @fn CE::Rendering::ShaderStage CE::Rendering::shader_stage_from_extension(const std::string& extension)
 * @brief Map an exact case-sensitive filename extension to the legacy stage identifier.
 * @param extension Leading-dot extension: .vert, .frag, .geo, .tesc or .tese.
 * @return Matching stage; every unrecognized spelling falls back to FRAGMENT rather than throwing.
 * No file is opened and backend capability/compile support is not established.
 */
