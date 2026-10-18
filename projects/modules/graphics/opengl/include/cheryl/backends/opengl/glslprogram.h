#pragma once

#include <cheryl/export.h>
#ifndef GLSLPROGRAM_H
#define GLSLPROGRAM_H

#define GLM_ENABLE_EXPERIMENTAL
#include <assets/resources/shader.h>

#include <backends/opengl/gl.h>
#include <backends/opengl/resource-lifetime.h>
#include <cassert>
#include <glm.hpp>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

namespace CE::Rendering::OpenGL {
    /** Owned native-reflection record copied from one linked program query. */
    struct Variable {
        std::string name; ///< Driver-reported variable name, including any native array spelling.
        NativeEnum type = 0; ///< Native GLSL type token.
        NativeInteger size = 0; ///< Driver-reported array/element count.
        NativeInteger location = -1; ///< Native location; -1 means unavailable/nonaddressable.
    };

    // GLSL naming belongs to this backend. Empty names omit roles a program does not use.
    /** Legacy engine-semantic to GLSL uniform mapping; empty names omit unused roles. */
    struct MaterialBindings {
        std::string projection = "projectionMatrix"; ///< Projection uniform; bind_pass selects the program before updating it.
        std::string view = "viewMatrix"; ///< View uniform; omitted when empty.
        std::string model = "modelMatrix"; ///< Model uniform used by bind_draw.
        std::string alpha = "in_Alpha"; ///< Floating alpha uniform; no CPU clamping.
        std::string scale = "in_Scale"; ///< Floating scale uniform; shader determines units.
        std::string texture = "mytexture"; ///< Legacy signed sampler-unit uniform.
    };

    /** Linked-program handle and legacy mutable uniform access.
     * Native operations require this program's live owner/current context. Uniform
     * setters do not select the program: call use() first (bind_pass does so).
     * Missing setters assert in assertion builds and skip the write otherwise;
     * location queries themselves return -1 for missing names. No setter validates
     * every native GLSL type or promises rollback of earlier writes.
     * @see [OpenGL backend](cheryl-guide:modules/opengl)
     */
    struct CE_TYPE_API Program final : CE::Rendering::Resources::Program {
    private:
        Handle program_;
        MaterialBindings material_bindings_;
        std::map<std::string, int> uniforms_;
        std::map<std::string, int> attributes_;
        const CE::Diagnostics::DomainId domain_ = CE::Diagnostics::next_domain_id();

    public:
        CE_OPENGL_API explicit Program(Handle program);
        CE_OPENGL_API void use() override;
        CE_OPENGL_API void bind_pass(const CE::Rendering::Parameters::PassSemantics& pass) override;
        CE_OPENGL_API void bind_draw(const CE::Rendering::Parameters::DrawSemantics& draw) override;
        CE_OPENGL_API void set_material_bindings(MaterialBindings bindings);
        void require_current() const { (void)program_.id(); }
        CE_OPENGL_API void require_linked() const;
        [[nodiscard]] const ResourceLifetime* resource_domain() const noexcept { return program_.resource_domain(); }
        [[nodiscard]] CE_OPENGL_API std::vector<Variable> active_uniforms() const;
        [[nodiscard]] CE_OPENGL_API std::vector<Variable> active_attributes() const;

        CE_OPENGL_API void set_uniform_vec(const char* name, const glm::vec2& value);
        CE_OPENGL_API void set_uniform_vec(const char* name, const glm::vec3& value);
        CE_OPENGL_API void set_uniform_vec(const char* name, const glm::vec4& value);
        CE_OPENGL_API void set_uniform_matrix(const char* name, const glm::mat2& value);
        CE_OPENGL_API void set_uniform_matrix(const char* name, const glm::mat3& value);
        CE_OPENGL_API void set_uniform_matrix(const char* name, const glm::mat4& value) override;
        CE_OPENGL_API void set_uniform_value(const char* name, float value) override;
        CE_OPENGL_API void set_uniform_value(const char* name, int value) override;
        CE_OPENGL_API void set_uniform_value(const char* name, unsigned int value) override;
        CE_OPENGL_API void set_uniform_value(const char* name, bool value) override;

        // Explicit template arguments remain supported by thin dispatch frontends.
        template <glm::length_t dim> void set_uniform_vec(const char* name, const glm::vec<dim, glm::f32, glm::defaultp>& value) {
            static_assert(2 <= dim && dim <= 4, "set_uniform_vec can only take 2-4D vectors");
            set_uniform_vec(name, value);
        }
        template <glm::length_t dim> void set_uniform_matrix(const char* name, const glm::mat<dim, dim, glm::f32, glm::defaultp>& value) {
            static_assert(2 <= dim && dim <= 4, "set_uniform_matrix can only take 2-4D matrices");
            set_uniform_matrix(name, value);
        }
        template <typename T> void set_uniform_value(const char* name, const T& value) {
            static_assert(
                std::is_same_v<T, float> || std::is_same_v<T, unsigned int> || std::is_same_v<T, int> || std::is_same_v<T, bool>,
                "set_uniform_value must take a float, int, unsigned int, or bool"
            );
            set_uniform_value(name, T(value));
        }

        // Explicit reflection diagnostics: DEBUG summary, TRACE variable records.
        // Queries retain their normal error contract; no stdout/event transport.
        CE_OPENGL_API void print_active_uniforms() const;
        CE_OPENGL_API void print_active_attribs() const;

        CE_OPENGL_API int get_uniform_location(const char* name);
        CE_OPENGL_API int get_attribute_location(const char* name);
    };
}

/* Important: no changelog will be updated Version 2.0 was quite a while ago
 * ..and that is why we're at the bottom of the file
 *
 * CE::Rendering::OpenGL::Program is based on code from  the excellent text "OpenGL 4.0 Shading Language Cookbook"
    by David Wolff.
    Modified by Darren Reid to suit Blit3D needs.

    Version 2.0 replaced functions with compile time templates
    Version 1.1 added support for vec2 uniforms
    Version 1.0	added a map for uniform/attributes, to cache lookup of locations in shader
*/
#endif

/**
 * @fn CE::Rendering::OpenGL::Program::Program(Handle program)
 * @brief Take ownership of an already tracked program name.
 * @param program Live owner/current-context handle of ResourceKind::Program.
 * Empty/stopped/current-context failures propagate; a different handle kind throws invalid_args. Construction checks the kind, not link status; require_linked() validates that separately.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::use()
 * @brief Select this live program with glUseProgram on its native owner/current context. Does not resolve material parameters or validate link status.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::bind_pass(const CE::Rendering::Parameters::PassSemantics& pass)
 * @brief Select the program, then write nonempty projection/view mappings.
 * @param pass Stable matrices borrowed through the call.
 * Requires native owner/current context; missing uniforms follow setter assertion/skip behavior. Earlier writes are not rolled back if a later lookup fails.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::bind_draw(const CE::Rendering::Parameters::DrawSemantics& draw)
 * @brief Write nonempty model/alpha/scale/legacy-texture mappings to the selected program.
 * @param draw Stable copied draw semantics borrowed through the call.
 * Caller first selects this program with use()/bind_pass(). No material packet or image binding is constructed; earlier writes are not rolled back.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::set_material_bindings(MaterialBindings bindings)
 * @brief Replace legacy semantic-name mappings without retaining caller strings.
 * @param bindings Owned names; empty values omit those roles.
 * Requires live owner/current context; no reflection/type/name validation is performed until use.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::require_current() const
 * @brief Validate this retained native handle's active owner/current context without selecting the program.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::require_linked() const
 * @brief Query native link status on the live owner/current context.
 * @throws CE::Exceptions::failed_operation For an unlinked program or native query error. Does not bind or relink the executable.
 */

/**
 * @fn const CE::Rendering::OpenGL::ResourceLifetime* CE::Rendering::OpenGL::Program::resource_domain() const noexcept
 * @return Borrowed domain identity while retaining this program; identity alone does not authorize native use.
 */

/**
 * @fn std::vector<CE::Rendering::OpenGL::Variable> CE::Rendering::OpenGL::Program::active_uniforms() const
 * @brief Reflect active uniforms on the live owner/current context.
 * @return Owned records independent of later vector destruction; an unused uniform may be absent.
 * Invalid/native reflection failures throw failed_operation; no program selection or cache publication occurs.
 */

/**
 * @fn std::vector<CE::Rendering::OpenGL::Variable> CE::Rendering::OpenGL::Program::active_attributes() const
 * @brief Reflect active vertex attributes on the live owner/current context.
 * @return Owned native name/type/count/location records.
 * Invalid/native reflection failures propagate; this does not validate compatibility with a pipeline layout.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::set_uniform_vec(const char* name, const glm::vec2& value)
 * @brief Write one 2-component floating GLSL vector to this already selected program.
 * @param name Nonnull NUL-terminated uniform name, borrowed through lookup.
 * @param value Components borrowed through this call and copied by OpenGL.
 * Requires live owner/current context. A missing location asserts in assertion builds and skips the write otherwise; the method does not select the program or reflect the exact uniform type.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::set_uniform_vec(const char* name, const glm::vec3& value)
 * @brief Write one 3-component floating GLSL vector to this already selected program.
 * @param name Nonnull NUL-terminated uniform name, borrowed through lookup.
 * @param value Components borrowed through this call and copied by OpenGL.
 * Requires live owner/current context. A missing location asserts in assertion builds and skips the write otherwise; the method does not select the program or reflect the exact uniform type.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::set_uniform_vec(const char* name, const glm::vec4& value)
 * @brief Write one 4-component floating GLSL vector to this already selected program.
 * @param name Nonnull NUL-terminated uniform name, borrowed through lookup.
 * @param value Components borrowed through this call and copied by OpenGL.
 * Requires live owner/current context. A missing location asserts in assertion builds and skips the write otherwise; the method does not select the program or reflect the exact uniform type.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::set_uniform_matrix(const char* name, const glm::mat2& value)
 * @brief Write one 2 by 2 floating matrix to this already selected program, without transposition.
 * @param name Nonnull NUL-terminated uniform name borrowed through the call.
 * @param value GLM column-major storage borrowed through upload.
 * Requires live owner/current context; absent locations assert/skip rather than selecting another program. Native type compatibility remains the caller's responsibility.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::set_uniform_matrix(const char* name, const glm::mat3& value)
 * @brief Write one 3 by 3 floating matrix to this already selected program, without transposition.
 * @param name Nonnull NUL-terminated uniform name borrowed through the call.
 * @param value GLM column-major storage borrowed through upload.
 * Requires live owner/current context; absent locations assert/skip rather than selecting another program. Native type compatibility remains the caller's responsibility.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::set_uniform_matrix(const char* name, const glm::mat4& value)
 * @brief Write one 4 by 4 floating matrix to this already selected program, without transposition.
 * @param name Nonnull NUL-terminated uniform name borrowed through the call.
 * @param value GLM column-major storage borrowed through upload.
 * Requires live owner/current context; absent locations assert/skip rather than selecting another program. Native type compatibility remains the caller's responsibility.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::set_uniform_value(const char* name, float value)
 * @brief Write a floating-point uniform to this already selected native program.
 * @param name Nonnull NUL-terminated name borrowed through the call.
 * @param value Copied scalar; no coercion beyond its declared overload.
 * Requires live owner/current context. Missing names assert in assertion builds and skip the write otherwise; this does not select a program or reflect type compatibility.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::set_uniform_value(const char* name, int value)
 * @brief Write a signed integer uniform to this already selected native program.
 * @param name Nonnull NUL-terminated name borrowed through the call.
 * @param value Copied scalar; no coercion beyond its declared overload.
 * Requires live owner/current context. Missing names assert in assertion builds and skip the write otherwise; this does not select a program or reflect type compatibility.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::set_uniform_value(const char* name, unsigned int value)
 * @brief Write a unsigned integer uniform to this already selected native program.
 * @param name Nonnull NUL-terminated name borrowed through the call.
 * @param value Copied scalar; no coercion beyond its declared overload.
 * Requires live owner/current context. Missing names assert in assertion builds and skip the write otherwise; this does not select a program or reflect type compatibility.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::set_uniform_value(const char* name, bool value)
 * @brief Write a boolean uniform to this already selected native program.
 * @param name Nonnull NUL-terminated name borrowed through the call.
 * @param value Copied scalar; no coercion beyond its declared overload.
 * Requires live owner/current context. Missing names assert in assertion builds and skip the write otherwise; this does not select a program or reflect type compatibility.
 */

/**
 * @fn template<glm::length_t dim> void CE::Rendering::OpenGL::Program::set_uniform_vec(const char* name, const glm::vec<dim, glm::f32, glm::defaultp>& value)
 * @brief Explicit-template compatibility frontend for floating vec2/vec3/vec4 setters.
 * @tparam dim Component count, statically restricted to 2..4.
 * @param name Uniform name borrowed through the call.
 * @param value Stable vector forwarded to the corresponding overload.
 * Current-program, lifetime, assertion/skip and failure behavior matches that overload.
 */

/**
 * @fn template<glm::length_t dim> void CE::Rendering::OpenGL::Program::set_uniform_matrix(const char* name, const glm::mat<dim, dim, glm::f32, glm::defaultp>& value)
 * @brief Explicit-template compatibility frontend for square mat2/mat3/mat4 setters.
 * @tparam dim Matrix dimension, statically restricted to 2..4.
 * @param name Uniform name borrowed through the call.
 * @param value Column-major matrix forwarded without transposition.
 * Requires this already selected program's live owner/current context; overload failure behavior applies.
 */

/**
 * @fn template<typename T> void CE::Rendering::OpenGL::Program::set_uniform_value(const char* name, const T& value)
 * @brief Dispatch an explicit scalar template argument to its native overload.
 * @tparam T Exactly float, int, unsigned int or bool; other types fail static assertion.
 * @param name Borrowed uniform name.
 * @param value Scalar copied to its matching overload.
 * No program selection/type conversion is added; native affinity and missing-uniform behavior are unchanged.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::print_active_uniforms() const
 * @brief Query reflection and emit a DEBUG summary with TRACE uniform records.
 * Requires live owner/current context; reflection failures propagate normally. Logging follows compile/runtime policy; no stdout or event delivery is used.
 */

/**
 * @fn void CE::Rendering::OpenGL::Program::print_active_attribs() const
 * @brief Query reflection and emit a DEBUG summary with TRACE attribute records.
 * Requires live owner/current context; reflection failures propagate normally, even if logging is disabled.
 */

/**
 * @fn int CE::Rendering::OpenGL::Program::get_uniform_location(const char* name)
 * @brief Query and cache a native uniform location on the live owner/current context.
 * @param name Nonnull NUL-terminated name; cache stores its own string.
 * @return Location, including -1 when absent/inactive; missing results are cached too.
 * Null names throw invalid_args; native query/allocation failures propagate. This cache is mutable and unsynchronized beyond owner affinity; the program is not relinked or selected.
 */

/**
 * @fn int CE::Rendering::OpenGL::Program::get_attribute_location(const char* name)
 * @brief Query and cache a native attribute location on the live owner/current context.
 * @param name Nonnull NUL-terminated name; cache stores its own string.
 * @return Location, including -1 when absent/inactive; missing results are cached too.
 * Null names throw invalid_args; native query/allocation failures propagate. This cache is mutable and unsynchronized beyond owner affinity; the program is not relinked or selected.
 */
