#include "backends/opengl/gl-test.h"
#include <backends/opengl/glslprogram.h>
#include <internals/exceptions.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace {
    class UniformRecorder final {
        inline static UniformRecorder* active_ = nullptr;
        std::vector<std::function<void()>> restore_;

    public:
        bool current = true;
        int queries = 0;
        int matrix_count = 0;
        bool transpose = true;
        int signed_value = 0;
        unsigned int unsigned_value = 0;
        std::vector<int> locations;
        std::vector<float> values;
        std::shared_ptr<CE::Rendering::OpenGL::ResourceLifetime> lifetime =
            std::make_shared<CE::Rendering::OpenGL::ResourceLifetime>(std::this_thread::get_id(), [this] { return current; });

    private:
        template <typename T> void replace(T& entry, T replacement) {
            const auto saved = entry;
            restore_.push_back([&entry, saved] { entry = saved; });
            entry = replacement;
        }

        static GLint GLAD_API_PTR location(GLuint, const GLchar*) {
            ++active_->queries;
            return 7;
        }
        static GLenum GLAD_API_PTR error() { return GL_NO_ERROR; }
        static void GLAD_API_PTR scalar_float(GLint location, GLfloat value) {
            active_->locations.push_back(location);
            active_->values = {value};
        }
        static void GLAD_API_PTR scalar_int(GLint location, GLint value) {
            active_->locations.push_back(location);
            active_->signed_value = value;
        }
        static void GLAD_API_PTR scalar_uint(GLint location, GLuint value) {
            active_->locations.push_back(location);
            active_->unsigned_value = value;
        }
        static void GLAD_API_PTR vector2(GLint location, GLfloat x, GLfloat y) {
            active_->locations.push_back(location);
            active_->values = {x, y};
        }
        static void GLAD_API_PTR vector3(GLint location, GLfloat x, GLfloat y, GLfloat z) {
            active_->locations.push_back(location);
            active_->values = {x, y, z};
        }
        static void GLAD_API_PTR vector4(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
            active_->locations.push_back(location);
            active_->values = {x, y, z, w};
        }
        template <int Dimension>
        static void GLAD_API_PTR matrix(GLint location, GLsizei count, GLboolean transpose, const GLfloat* values) {
            active_->locations.push_back(location);
            active_->matrix_count = count;
            active_->transpose = transpose;
            active_->values.assign(values, values + Dimension * Dimension);
        }

    public:
        UniformRecorder() {
            active_ = this;
            replace(glad_glGetUniformLocation, &location);
            replace(glad_glGetError, &error);
            replace(glad_glUniform1f, &scalar_float);
            replace(glad_glUniform1i, &scalar_int);
            replace(glad_glUniform1ui, &scalar_uint);
            replace(glad_glUniform2f, &vector2);
            replace(glad_glUniform3f, &vector3);
            replace(glad_glUniform4f, &vector4);
            replace(glad_glUniformMatrix2fv, static_cast<PFNGLUNIFORMMATRIX2FVPROC>(&matrix<2>));
            replace(glad_glUniformMatrix3fv, static_cast<PFNGLUNIFORMMATRIX3FVPROC>(&matrix<3>));
            replace(glad_glUniformMatrix4fv, static_cast<PFNGLUNIFORMMATRIX4FVPROC>(&matrix<4>));
        }
        ~UniformRecorder() {
            lifetime->abandon();
            for (auto saved = restore_.rbegin(); saved != restore_.rend(); ++saved)
                (*saved)();
            active_ = nullptr;
        }
        CE::Rendering::OpenGL::Program program() {
            return CE::Rendering::OpenGL::Program({lifetime, CE::Rendering::OpenGL::ResourceKind::Program, 41});
        }
    };
}

TEST(opengl_uniforms, vectors) {
    UniformRecorder native;
    auto program = native.program();
    program.set_uniform_vec<2>("value", glm::vec2(1, 2));
    EXPECT_EQ(native.values, (std::vector<float>{1, 2}));
    program.set_uniform_vec("value", glm::vec3(3, 4, 5));
    EXPECT_EQ(native.values, (std::vector<float>{3, 4, 5}));
    program.set_uniform_vec("value", glm::vec4(6, 7, 8, 9));
    EXPECT_EQ(native.values, (std::vector<float>{6, 7, 8, 9}));
    EXPECT_EQ(native.locations, (std::vector<int>{7, 7, 7}));
    EXPECT_EQ(native.queries, 1);
}

TEST(opengl_uniforms, matrices) {
    UniformRecorder native;
    auto program = native.program();
    const glm::mat2 matrix2(1, 2, 3, 4);
    program.set_uniform_matrix<2>("value", matrix2);
    EXPECT_EQ(native.values, (std::vector<float>{1, 2, 3, 4}));
    const glm::mat3 matrix3(1, 2, 3, 4, 5, 6, 7, 8, 9);
    program.set_uniform_matrix("value", matrix3);
    EXPECT_EQ(native.values, (std::vector<float>{1, 2, 3, 4, 5, 6, 7, 8, 9}));
    const glm::mat4 matrix4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    CE::Rendering::Resources::Program& resource = program;
    resource.set_uniform_matrix("value", matrix4);
    EXPECT_EQ(native.values, (std::vector<float>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}));
    EXPECT_EQ(native.matrix_count, 1);
    EXPECT_FALSE(native.transpose);
    EXPECT_EQ(native.locations, (std::vector<int>{7, 7, 7}));
}

TEST(opengl_uniforms, scalars) {
    UniformRecorder native;
    auto program = native.program();
    program.set_uniform_value<float>("value", 1.25f);
    EXPECT_EQ(native.values, (std::vector<float>{1.25f}));
    program.set_uniform_value("value", -9);
    EXPECT_EQ(native.signed_value, -9);
    program.set_uniform_value("value", std::numeric_limits<unsigned int>::max());
    EXPECT_EQ(native.unsigned_value, std::numeric_limits<unsigned int>::max());
    program.set_uniform_value("value", true);
    EXPECT_EQ(native.signed_value, 1);
    program.set_uniform_value("value", false);
    EXPECT_EQ(native.signed_value, 0);
    EXPECT_EQ(native.locations.size(), 5u);
    EXPECT_EQ(native.queries, 1);
}

TEST(opengl_uniforms, context) {
    UniformRecorder native;
    auto program = native.program();
    native.current = false;
    EXPECT_THROW(program.set_uniform_vec("value", glm::vec3(1)), CE::Exceptions::failed_operation);
    EXPECT_TRUE(native.locations.empty());
    EXPECT_EQ(native.queries, 0);
    native.current = true;
    EXPECT_THROW(program.set_uniform_value(nullptr, 1), CE::Exceptions::invalid_args);
    EXPECT_TRUE(native.locations.empty());
}
