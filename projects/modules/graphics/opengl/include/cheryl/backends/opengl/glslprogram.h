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
    struct Variable {
        std::string name;
        NativeEnum type = 0;
        NativeInteger size = 0;
        NativeInteger location = -1;
    };

    // GLSL naming belongs to this backend. Empty names omit roles a program does not use.
    struct MaterialBindings {
        std::string projection = "projectionMatrix";
        std::string view = "viewMatrix";
        std::string model = "modelMatrix";
        std::string alpha = "in_Alpha";
        std::string scale = "in_Scale";
        std::string texture = "mytexture";
    };

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
