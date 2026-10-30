#pragma once

#include <cheryl/export.h>

#include <glm.hpp>

namespace CE::Rendering::Parameters {
    /** Copied pass matrices, independent of a live camera. */
    // Copied engine semantics. Matrices use the caller's coordinate convention;
    // alpha/scale have no common clamping. ShaderDraw::texture_unit is a legacy
    // zero-based request; material parameters use explicit ImageBinding instead.
    struct PassSemantics {
        glm::mat4 projection{1.0f}; ///< Projection matrix; identity by default, using the caller's coordinate convention.
        glm::mat4 view{1.0f}; ///< Copied world-to-view matrix; identity by default.
    };
    /** Copied per-draw engine values; alpha and scale are not clamped. */
    struct DrawSemantics {
        glm::mat4 model{1.0f}; ///< Local-to-world transform, independent of a live entity.
        float alpha = 1.0f; ///< Shader alpha multiplier; the shader/blend combination defines its effect.
        float scale = 1.0f; ///< Shader size multiplier; no shared unit conversion or validation.
        int texture_unit = 0; ///< Legacy zero-based texture request; materials use ImageBinding instead.
    };
}

namespace CE::Rendering::Resources {
    /** Mutable executable-program access on the backend owner/current context.
     * This legacy primitive interface does not provide immutable packet resolution.
     * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
     */
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

/**
 * @fn CE::Rendering::Resources::Program::~Program()
 * @brief Release the logical executable handle under its backend retirement contract.
 */

/**
 * @fn void CE::Rendering::Resources::Program::bind_pass(const CE::Rendering::Parameters::PassSemantics& pass)
 * @brief Apply the backend's mapped pass semantics.
 * @param pass Copied projection/view values borrowed through this call.
 * Requires live backend owner/current context; concrete mappings may omit roles unused by the program.
 */

/**
 * @fn void CE::Rendering::Resources::Program::bind_draw(const CE::Rendering::Parameters::DrawSemantics& draw)
 * @brief Apply the backend's mapped model, alpha, scale and legacy texture semantics.
 * @param draw Stable values borrowed through this call.
 * This mutates program state and performs no frame publication; backend errors may leave prior writes applied.
 */

/**
 * @fn void CE::Rendering::Resources::Program::use()
 * @brief Select this executable on the backend owner/current context. Retain the program and its original live resource domain through use.
 */

/**
 * @fn void CE::Rendering::Resources::Program::set_uniform_value(const char* name, float value)
 * @brief Set one backend-named floating-point uniform on the live backend owner.
 * @param name NUL-terminated backend uniform name borrowed only through this call.
 * @param value Value copied into program state.
 * Name/type validation and whether the program must already be selected belong to the backend; failure does not promise rollback of earlier writes.
 */

/**
 * @fn void CE::Rendering::Resources::Program::set_uniform_value(const char* name, int value)
 * @brief Set one backend-named signed integer uniform on the live backend owner.
 * @param name NUL-terminated backend uniform name borrowed only through this call.
 * @param value Value copied into program state.
 * Name/type validation and whether the program must already be selected belong to the backend; failure does not promise rollback of earlier writes.
 */

/**
 * @fn void CE::Rendering::Resources::Program::set_uniform_value(const char* name, unsigned int value)
 * @brief Set one backend-named unsigned integer uniform on the live backend owner.
 * @param name NUL-terminated backend uniform name borrowed only through this call.
 * @param value Value copied into program state.
 * Name/type validation and whether the program must already be selected belong to the backend; failure does not promise rollback of earlier writes.
 */

/**
 * @fn void CE::Rendering::Resources::Program::set_uniform_value(const char* name, bool value)
 * @brief Set one backend-named boolean uniform on the live backend owner.
 * @param name NUL-terminated backend uniform name borrowed only through this call.
 * @param value Value copied into program state.
 * Name/type validation and whether the program must already be selected belong to the backend; failure does not promise rollback of earlier writes.
 */

/**
 * @fn void CE::Rendering::Resources::Program::set_uniform_matrix(const char* name, const glm::mat4& value)
 * @brief Set one backend-named 4 by 4 matrix uniform.
 * @param name NUL-terminated name borrowed through the call.
 * @param value Matrix borrowed through the call; no live camera is retained.
 * Requires the backend owner/current context. Concrete adapters define matrix upload/name validation and current-program requirements.
 */
