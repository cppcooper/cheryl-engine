#include <backends/opengl/pipeline.h>
#include <backends/opengl/texture.h>

#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
    using namespace CE::Assets;
    using namespace CE::RenderAPIs;
    using CE::Exceptions::invalid_args;

    // Synthetic IDs and recording GLAD entries exercise contract code without a window.
    // All entries are restored, including when fixture initialization throws.
    class NativeProgramRecorder final {
        inline static NativeProgramRecorder* active_ = nullptr;
        std::vector<std::function<void()>> restore_;
        std::shared_ptr<OpenGLResourceLifetime> lifetime_ =
            std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [] { return true; });
        std::vector<std::shared_ptr<OpenGLResourceLifetime>> other_domains_;
        GLuint next_id_ = 10;
        std::uint32_t active_unit_ = 0;

    public:
        bool linked = true;
        std::vector<GLSLVariable> uniforms;
        std::vector<GLSLVariable> attributes{{"in_Position", GL_FLOAT_VEC3, 1, 0}, {"in_Texcoord", GL_FLOAT_VEC2, 1, 1}};
        std::map<GLint, ParameterValue> writes;
        std::vector<std::pair<std::uint32_t, GLuint>> image_binds;
        int uses = 0;

    private:
        template <typename T>
        void replace(T& entry, T replacement) {
            const auto old = entry;
            restore_.push_back([&entry, old] { entry = old; });
            entry = replacement;
        }

        void restore() noexcept {
            for (auto item = restore_.rbegin(); item != restore_.rend(); ++item)
                (*item)();
            restore_.clear();
            active_ = nullptr;
        }

        static GLint maximum_name_length(const std::vector<GLSLVariable>& variables) {
            std::size_t longest = 0;
            for (const auto& variable : variables)
                longest = std::max(longest, variable.name.size());
            return static_cast<GLint>(longest + 1);
        }

        static void GLAD_API_PTR program_query(GLuint, GLenum parameter, GLint* value) {
            switch (parameter) {
                case GL_LINK_STATUS: *value = active_->linked ? GL_TRUE : GL_FALSE; break;
                case GL_ACTIVE_UNIFORMS: *value = static_cast<GLint>(active_->uniforms.size()); break;
                case GL_ACTIVE_UNIFORM_MAX_LENGTH: *value = maximum_name_length(active_->uniforms); break;
                case GL_ACTIVE_ATTRIBUTES: *value = static_cast<GLint>(active_->attributes.size()); break;
                case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH: *value = maximum_name_length(active_->attributes); break;
                default: *value = 0; break;
            }
        }

        static void variable_query(
            const GLSLVariable& variable,
            GLsizei capacity,
            GLsizei* written,
            GLint* size,
            GLenum* type,
            GLchar* name
        ) {
            const auto count = std::min(variable.name.size(), static_cast<std::size_t>(std::max(0, capacity - 1)));
            std::copy_n(variable.name.data(), count, name);
            name[count] = '\0';
            *written = static_cast<GLsizei>(count);
            *size = variable.size;
            *type = variable.type;
        }

        static void GLAD_API_PTR uniform_query(
            GLuint,
            GLuint index,
            GLsizei capacity,
            GLsizei* written,
            GLint* size,
            GLenum* type,
            GLchar* name
        ) {
            variable_query(active_->uniforms.at(index), capacity, written, size, type, name);
        }

        static void GLAD_API_PTR attribute_query(
            GLuint,
            GLuint index,
            GLsizei capacity,
            GLsizei* written,
            GLint* size,
            GLenum* type,
            GLchar* name
        ) {
            variable_query(active_->attributes.at(index), capacity, written, size, type, name);
        }

        static GLint location(const std::vector<GLSLVariable>& variables, const GLchar* name) {
            const auto variable = std::find_if(variables.begin(), variables.end(), [&](const auto& item) { return item.name == name; });
            return variable == variables.end() ? -1 : variable->location;
        }
        static GLint GLAD_API_PTR uniform_location(GLuint, const GLchar* name) { return location(active_->uniforms, name); }
        static GLint GLAD_API_PTR attribute_location(GLuint, const GLchar* name) { return location(active_->attributes, name); }
        static void GLAD_API_PTR use(GLuint) { ++active_->uses; }
        static void GLAD_API_PTR scalar(GLint location, GLfloat value) { active_->writes.insert_or_assign(location, value); }
        static void GLAD_API_PTR integer(GLint location, GLint value) { active_->writes.insert_or_assign(location, value); }
        static void GLAD_API_PTR colour(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
            active_->writes.insert_or_assign(location, glm::vec4{x, y, z, w});
        }
        static void GLAD_API_PTR matrix(GLint location, GLsizei, GLboolean, const GLfloat* values) {
            glm::mat4 copied{0.0f};
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    copied[column][row] = values[column * 4 + row];
            active_->writes.insert_or_assign(location, copied);
        }
        static void GLAD_API_PTR generate_images(GLsizei count, GLuint* images) {
            for (GLsizei i = 0; i < count; ++i)
                images[i] = ++active_->next_id_;
        }
        static void GLAD_API_PTR activate_image(GLenum unit) { active_->active_unit_ = unit - GL_TEXTURE0; }
        static void GLAD_API_PTR bind_image(GLenum, GLuint image) { active_->image_binds.emplace_back(active_->active_unit_, image); }
        static void GLAD_API_PTR integer_query(GLenum parameter, GLint* value) {
            *value = parameter == GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS ? 8 : 4;
        }
        static void GLAD_API_PTR image_parameter(GLenum, GLenum, GLint) {}
        static void GLAD_API_PTR pixel_store(GLenum, GLint) {}
        static void GLAD_API_PTR upload_image(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) {}

    public:
        NativeProgramRecorder() {
            active_ = this;
            try {
                replace(glad_glGetProgramiv, program_query);
                replace(glad_glGetActiveUniform, uniform_query);
                replace(glad_glGetActiveAttrib, attribute_query);
                replace(glad_glGetUniformLocation, uniform_location);
                replace(glad_glGetAttribLocation, attribute_location);
                replace(glad_glUseProgram, use);
                replace(glad_glUniform1f, scalar);
                replace(glad_glUniform1i, integer);
                replace(glad_glUniform4f, colour);
                replace(glad_glUniformMatrix4fv, matrix);
                replace(glad_glGenTextures, generate_images);
                replace(glad_glActiveTexture, activate_image);
                replace(glad_glBindTexture, bind_image);
                replace(glad_glGetIntegerv, integer_query);
                replace(glad_glTexParameteri, image_parameter);
                replace(glad_glPixelStorei, pixel_store);
                replace(glad_glTexImage2D, upload_image);
                replace(GLAD_GL_EXT_texture_filter_anisotropic, 0);
            } catch (...) {
                restore();
                throw;
            }
        }
        ~NativeProgramRecorder() {
            lifetime_->abandon();
            for (const auto& domain : other_domains_)
                domain->abandon();
            restore();
        }
        NativeProgramRecorder(const NativeProgramRecorder&) = delete;
        NativeProgramRecorder& operator=(const NativeProgramRecorder&) = delete;

        std::shared_ptr<GLSLProgram> program() {
            return std::make_shared<GLSLProgram>(OpenGLHandle(lifetime_, GLResourceKind::Program, ++next_id_));
        }
        std::shared_ptr<Texture> image(bool another_domain = false) {
            auto domain = lifetime_;
            if (another_domain) {
                domain = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [] { return true; });
                other_domains_.push_back(domain);
            }
            const unsigned char pixels[]{255, 255, 255, 255};
            return std::make_shared<Texture>(domain, pixels, 1, 1, false, false, GL_CLAMP_TO_EDGE, GL_RGBA);
        }
    };

    PipelineDefinition time_definition(bool required = true) {
        PipelineDefinition definition;
        definition.program_sources = {"effect.vert", "effect.frag"};
        definition.parameters = {{"time", ParameterType::Float, required}};
        return definition;
    }
}

TEST(opengl_pipeline, effect_binds_two_images_and_copied_time_colour_and_camera) {
    NativeProgramRecorder native;
    native.uniforms = {{"uTime", GL_FLOAT, 1, 5}, {"uColor", GL_FLOAT_VEC4, 1, 6}, {"uCamera", GL_FLOAT_MAT4, 1, 7},
        {"uBase", GL_SAMPLER_2D, 1, 8}, {"uMask", GL_SAMPLER_2D, 1, 9}};
    auto definition = time_definition();
    definition.parameters.push_back({"color", ParameterType::Vec4});
    definition.parameters.push_back({"camera", ParameterType::Mat4, true, ParameterSemantic::Projection});
    definition.parameters.push_back({"base", ParameterType::Sampler2D});
    definition.parameters.push_back({"mask", ParameterType::Sampler2D});
    GLSLPipelineBindings bindings{{{"time", "uTime"}, {"color", "uColor"}, {"camera", "uCamera"}, {"base", "uBase"}, {"mask", "uMask"}}};
    auto pipeline = std::make_shared<GLSLPipeline>(definition, native.program(), bindings);
    auto first = native.image();
    auto second = native.image();
    Material material({pipeline, {{"base", ImageBinding{first, 1}}, {"mask", ImageBinding{second, 3}}}});
    ShaderPass camera;
    camera.projection[3][0] = 17.0f;
    auto values = material.resolve(camera, {}, {{"time", 4.0f}}, {{"color", glm::vec4{0.1f, 0.2f, 0.3f, 0.4f}}});
    native.image_binds.clear();
    pipeline->bind_parameters(values);
    EXPECT_EQ(native.uses, 1);
    EXPECT_FLOAT_EQ(std::get<float>(native.writes.at(5)), 4.0f);
    EXPECT_FLOAT_EQ(std::get<glm::vec4>(native.writes.at(6)).w, 0.4f);
    EXPECT_FLOAT_EQ(std::get<glm::mat4>(native.writes.at(7))[3][0], 17.0f);
    EXPECT_EQ(std::get<int>(native.writes.at(8)), 1);
    EXPECT_EQ(std::get<int>(native.writes.at(9)), 3);
    ASSERT_EQ(native.image_binds.size(), 2u);
    EXPECT_EQ(native.image_binds[0].first, 1u);
    EXPECT_EQ(native.image_binds[1].first, 3u);
    EXPECT_NE(native.image_binds[0].second, native.image_binds[1].second);
}

TEST(opengl_pipeline, an_omitted_active_optional_uniform_resets_after_the_previous_draw) {
    NativeProgramRecorder native;
    native.uniforms = {{"uTime", GL_FLOAT, 1, 5}};
    const auto program = native.program();
    const auto definition = time_definition(false);
    EXPECT_THROW(static_cast<void>(GLSLPipeline(definition, program, {{{"time", "uTime"}}})), invalid_args);
    GLSLPipeline pipeline(definition, program, {{{"time", "uTime", 0.0f}}});
    pipeline.bind_parameters({{"time", 7.0f}});
    pipeline.bind_parameters({});
    EXPECT_EQ(native.uses, 2);
    EXPECT_FLOAT_EQ(std::get<float>(native.writes.at(5)), 0.0f);
}

TEST(opengl_pipeline, inactive_optional_uniforms_do_not_force_sprite_roles) {
    NativeProgramRecorder native;
    GLSLPipeline pipeline(time_definition(false), native.program(), {{{"time", "uTime"}}});
    pipeline.bind_parameters({});
    EXPECT_EQ(native.uses, 1);
    EXPECT_TRUE(native.writes.empty());
}

TEST(opengl_pipeline, bad_required_types_arrays_and_unmapped_uniforms_fail_before_use) {
    NativeProgramRecorder native;
    const auto program = native.program();
    const auto definition = time_definition();
    const GLSLPipelineBindings bindings{{{"time", "uTime"}}};
    EXPECT_THROW(static_cast<void>(GLSLPipeline(definition, program, bindings)), invalid_args);
    native.uniforms = {{"uTime", GL_INT, 1, 5}};
    EXPECT_THROW(static_cast<void>(GLSLPipeline(definition, program, bindings)), invalid_args);
    native.uniforms = {{"uTime", GL_FLOAT, 2, 5}};
    EXPECT_THROW(static_cast<void>(GLSLPipeline(definition, program, bindings)), invalid_args);
    native.uniforms = {{"uTime", GL_FLOAT, 1, -1}};
    EXPECT_THROW(static_cast<void>(GLSLPipeline(definition, program, bindings)), invalid_args);
    native.uniforms = {{"uTime", GL_FLOAT, 1, 5}, {"outside", GL_FLOAT, 1, 6}};
    EXPECT_THROW(static_cast<void>(GLSLPipeline(definition, program, bindings)), invalid_args);
    EXPECT_EQ(native.uses, 0);
    EXPECT_TRUE(native.writes.empty());
}

TEST(opengl_pipeline, attribute_locations_and_unlinked_programs_are_rejected) {
    NativeProgramRecorder native;
    native.uniforms = {{"uTime", GL_FLOAT, 1, 5}};
    const auto program = native.program();
    native.attributes[0].location = 2;
    EXPECT_THROW(static_cast<void>(GLSLPipeline(time_definition(), program, {{{"time", "uTime"}}})), invalid_args);
    native.attributes[0].location = 0;
    native.linked = false;
    EXPECT_THROW(static_cast<void>(GLSLPipeline(time_definition(), program, {{{"time", "uTime"}}})), CE::Exceptions::failed_operation);
    EXPECT_EQ(native.uses, 0);
}

TEST(opengl_pipeline, bad_sampler_domains_and_units_do_not_partially_bind_a_draw) {
    NativeProgramRecorder native;
    native.uniforms = {{"uTime", GL_FLOAT, 1, 5}, {"uImage", GL_SAMPLER_2D, 1, 6}};
    auto definition = time_definition();
    definition.parameters.push_back({"image", ParameterType::Sampler2D});
    GLSLPipeline pipeline(definition, native.program(), {{{"time", "uTime"}, {"image", "uImage"}}});
    auto foreign = native.image(true);
    auto local = native.image();
    native.image_binds.clear();
    EXPECT_THROW(pipeline.bind_parameters({{"time", 1.0f}, {"image", ImageBinding{foreign, 0}}}), invalid_args);
    EXPECT_THROW(pipeline.bind_parameters({{"time", 1.0f}, {"image", ImageBinding{local, 8}}}), invalid_args);
    EXPECT_THROW(pipeline.bind_parameters({{"time", 1}, {"image", ImageBinding{local, 0}}}), invalid_args);
    EXPECT_EQ(native.uses, 0);
    EXPECT_TRUE(native.writes.empty());
    EXPECT_TRUE(native.image_binds.empty());
}
