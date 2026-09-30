#include <gtest/gtest.h>

#include <core/rendering/idraw.h>

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

        CE::Assets::ShaderPass camera;
        int raw_uniform_writes = 0;

        void bind_pass(const CE::Assets::ShaderPass& pass) override {
            ++uses;
            camera = pass;
        }

        void bind_draw(const CE::Assets::ShaderDraw& draw) override {
            alpha = draw.alpha;
            scale = draw.scale;
            model = draw.model;
        }

        void use() override { ++uses; }
        void set_uniform_value(const char*, float) override { ++raw_uniform_writes; }
        void set_uniform_value(const char*, int) override { ++raw_uniform_writes; }
        void set_uniform_value(const char*, unsigned int) override { ++raw_uniform_writes; }
        void set_uniform_value(const char*, bool) override { ++raw_uniform_writes; }
        void set_uniform_matrix(const char*, const glm::mat4&) override { ++raw_uniform_writes; }
    };

    struct RecordingDrawable final : CE::Assets::iDraw {
        void draw(const CE::DrawInfo& info) override { info.use_shader(); }
    };
}

TEST(draw_contract, material_without_glsl) {
    // Supply a recording shader with distinct alpha, scale, and model values.
    auto material = std::make_shared<RecordingShader>();
    CE::DrawInfo info;
    info.material = material;
    info.alpha = 0.4f;
    info.scale = 2.0f;
    info.model_matrix[3][0] = 42.0f;
    info.camera.view[3][1] = 19.0f;

    // The drawable delegates to DrawInfo; inspect what it sent to the shader.
    RecordingDrawable drawable;
    drawable.draw(info);
    EXPECT_EQ(material->uses, 1);
    EXPECT_EQ(material->raw_uniform_writes, 0);
    EXPECT_FLOAT_EQ(material->camera.view[3][1], 19.0f);
    EXPECT_FLOAT_EQ(material->alpha, 0.4f);
    EXPECT_FLOAT_EQ(material->scale, 2.0f);
    EXPECT_FLOAT_EQ(material->model[3][0], 42.0f);
}
