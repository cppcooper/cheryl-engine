#include <core/rendering/draw-packet.h>
#include <gtest/gtest.h>

#include <memory>

#ifdef GL_VERSION_3_3
#error The draw contract must not include OpenGL.
#endif

namespace {
    class ContractGeometry final : public CE::Assets::Geometry2D {
    public:
        CE::Assets::VertexLayout2D vertex_layout() const noexcept override { return CE::Assets::VertexLayout2D::Position3UV2; }
        CE::Assets::PrimitiveTopology topology() const noexcept override { return CE::Assets::PrimitiveTopology::Triangles; }
        std::size_t vertex_count() const noexcept override { return 6; }
        void bind() const override { FAIL() << "Resolution must not bind geometry"; }
        void draw(
            std::size_t,
            std::size_t
        ) const override {
            FAIL() << "Resolution must not issue native draws";
        }
    };
    class ContractPipeline final : public CE::Assets::Pipeline {
    public:
        ContractPipeline()
        : Pipeline(make_definition()) {}

    private:
        static CE::Assets::PipelineDefinition make_definition() {
            using namespace CE::Assets;
            PipelineDefinition definition;
            definition.program_sources = {"contract.vert", "contract.frag"};
            definition.parameters = {{"view", ParameterType::Mat4, true, ParameterSemantic::View},
                {"model", ParameterType::Mat4, true, ParameterSemantic::Model},
                {"alpha", ParameterType::Float, true, ParameterSemantic::Alpha},
                {"scale", ParameterType::Float, true, ParameterSemantic::Scale}};
            return definition;
        }
    };
}

TEST(
    draw_contract,
    material_without_glsl_resolves_owned_semantics_without_native_calls
) {
    CE::RenderAPIs::DrawStyle2D style;
    style.material = std::make_shared<CE::Assets::Material>(CE::Assets::MaterialDefinition{std::make_shared<ContractPipeline>(), {}});
    style.alpha = 0.4f;
    style.scale = 2.0f;
    style.model_matrix[3][0] = 42.0f;
    CE::Assets::ShaderPass pass;
    pass.view[3][1] = 19.0f;
    const auto packet = CE::RenderAPIs::resolve_draw_packet(std::make_shared<ContractGeometry>(), 0, 6, style, pass, {}, {});
    pass.view[3][1] = 100.0f;
    style.model_matrix[3][0] = 0.0f;
    style.material.reset();
    ASSERT_TRUE(packet.material);
    EXPECT_FLOAT_EQ(std::get<glm::mat4>(packet.parameters.at("view"))[3][1], 19.0f);
    EXPECT_FLOAT_EQ(std::get<float>(packet.parameters.at("alpha")), 0.4f);
    EXPECT_FLOAT_EQ(std::get<float>(packet.parameters.at("scale")), 2.0f);
    EXPECT_FLOAT_EQ(std::get<glm::mat4>(packet.parameters.at("model"))[3][0], 42.0f);
}
