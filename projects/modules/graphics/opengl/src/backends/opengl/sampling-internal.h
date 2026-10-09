#pragma once

#include "upload-check.h"

#include <cmath>
#include <cstdint>

namespace CE::RenderAPIs::SamplingDetail {
    inline bool anisotropy_supported() {
        return GLAD_GL_VERSION_4_6 || GLAD_GL_ARB_texture_filter_anisotropic || GLAD_GL_EXT_texture_filter_anisotropic;
    }

    inline float maximum_anisotropy() {
        if (!anisotropy_supported())
            return 1;
        GLfloat maximum = 0;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maximum);
        require_no_gl_error("OpenGL anisotropy-limit query failed");
        if (!std::isfinite(maximum) || maximum < 1)
            throw Exceptions::failed_operation(CE_HERE, "Current context reports an invalid anisotropy limit");
        return maximum;
    }

    inline std::uint32_t binding_unit_limit() {
        GLint maximum = 0;
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maximum);
        require_no_gl_error("OpenGL texture binding-limit query failed");
        if (maximum <= 0)
            throw Exceptions::failed_operation(CE_HERE, "Current context reports no texture binding units");
        return static_cast<std::uint32_t>(maximum);
    }
}
