#include <gtest/gtest.h>

#include <assets/abstracts/idraw.h>

#include <memory>
#include <string_view>

#ifdef GL_VERSION_3_3
#error The draw contract must not include OpenGL.
#endif

namespace {
    /** Records DrawInfo's material values without compiling a shader program. */
    struct RecordingShader final : CE::Assets::Shader {
        int uses = 0;
        float alpha = 0.0f;
        float scale = 0.0f;
        glm::mat4 model{0.0f};

        void use() override { ++uses; }
        void set_uniform_value(const char* name, float value) override {
            if (std::string_view(name) == "in_Alpha")
                alpha = value;
            if (std::string_view(name) == "in_Scale")
                scale = value;
        }
        void set_uniform_value(const char*, int) override {}
        void set_uniform_value(const char*, unsigned int) override {}
        void set_uniform_value(const char*, bool) override {}
        void set_uniform_matrix(const char* name, const glm::mat4& value) override {
            if (std::string_view(name) == "modelMatrix")
                model = value;
        }
    };

    struct RecordingDrawable final : CE::Assets::iDraw {
        void draw(const CE::DrawInfo& info) override { info.use_shader(); }
    };
}

TEST(draw_contract, applies_material_parameters_without_a_glsl_program) {
    // Supply a recording shader with distinct alpha, scale, and model values.
    auto material = std::make_shared<RecordingShader>();
    CE::DrawInfo info;
    info.material = material;
    info.alpha = 0.4f;
    info.scale = 2.0f;
    info.model_matrix[3][0] = 42.0f;

    // The drawable delegates to DrawInfo; inspect what it sent to the shader.
    RecordingDrawable drawable;
    drawable.draw(info);
    EXPECT_EQ(material->uses, 1);
    EXPECT_FLOAT_EQ(material->alpha, 0.4f);
    EXPECT_FLOAT_EQ(material->scale, 2.0f);
    EXPECT_FLOAT_EQ(material->model[3][0], 42.0f);
}
