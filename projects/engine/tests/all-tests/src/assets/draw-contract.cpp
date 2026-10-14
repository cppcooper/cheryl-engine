#include <core/rendering/draw-packet.h>
#include <internals/exceptions.h>
#include <gtest/gtest.h>

#include <memory>

#ifdef GL_VERSION_3_3
#error The draw contract must not include OpenGL.
#endif

namespace {
    class ContractGeometry final : public CE::Rendering::Geometry::Buffer2D {
    public:
        CE::Rendering::Geometry::Layout2D vertex_layout() const noexcept override {
            return CE::Rendering::Geometry::Layout2D::Position3UV2;
        }
        CE::Rendering::Geometry::Topology topology() const noexcept override { return CE::Rendering::Geometry::Topology::Triangles; }
        std::size_t vertex_count() const noexcept override { return 6; }
        void bind() const override { FAIL() << "Resolution must not bind geometry"; }
        void draw(std::size_t, std::size_t) const override { FAIL() << "Resolution must not issue native draws"; }
    };
    class ContractPipeline final : public CE::Rendering::Resources::Pipeline {
    public:
        ContractPipeline()
        : CE::Rendering::Resources::Pipeline(make_definition()) {}

    private:
        static CE::Rendering::Pipelines::Definition make_definition() {
            CE::Rendering::Pipelines::Definition definition;
            definition.program_sources = {"contract.vert", "contract.frag"};
            definition.parameters = {
                {"view", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::View},
                {"model", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Model},
                {"alpha", CE::Rendering::Parameters::Type::Float, true, CE::Rendering::Parameters::Semantic::Alpha},
                {"scale", CE::Rendering::Parameters::Type::Float, true, CE::Rendering::Parameters::Semantic::Scale}
            };
            return definition;
        }
    };
}

TEST(draw_contract, material_resolution) {
    CE::Rendering::DrawStyle2D style;
    style.material = std::make_shared<CE::Rendering::Resources::Material>(
        CE::Rendering::Materials::Definition{std::make_shared<ContractPipeline>(), {}}
    );
    style.alpha = 0.4f;
    style.scale = 2.0f;
    style.model_matrix[3][0] = 42.0f;
    CE::Rendering::Parameters::PassSemantics pass;
    pass.view[3][1] = 19.0f;
    const auto packet = CE::Rendering::resolve_draw_packet(std::make_shared<ContractGeometry>(), 0, 6, style, pass, {}, {});
    pass.view[3][1] = 100.0f;
    style.model_matrix[3][0] = 0.0f;
    style.material.reset();
    ASSERT_TRUE(packet.material);
    EXPECT_FLOAT_EQ(std::get<glm::mat4>(packet.parameters.at("view"))[3][1], 19.0f);
    EXPECT_FLOAT_EQ(std::get<float>(packet.parameters.at("alpha")), 0.4f);
    EXPECT_FLOAT_EQ(std::get<float>(packet.parameters.at("scale")), 2.0f);
    EXPECT_FLOAT_EQ(std::get<glm::mat4>(packet.parameters.at("model"))[3][0], 42.0f);
}

TEST(draw_contract, copied_clip) {
    CE::Rendering::DrawStyle2D style;
    style.material = std::make_shared<CE::Rendering::Resources::Material>(
        CE::Rendering::Materials::Definition{std::make_shared<ContractPipeline>(), {}}
    );
    style.clip = CE::Rendering::ClipRegion2D{{10, 20, 40, 60}, 100, 100};
    const auto packet = CE::Rendering::resolve_draw_packet(std::make_shared<ContractGeometry>(), 0, 6, style, {}, {}, {});
    style.clip->rectangle.right = 80;
    ASSERT_TRUE(packet.clip);
    EXPECT_EQ(packet.clip->rectangle.right, 40);
    style.clip->logical_width = 0;
    EXPECT_THROW(
        (void)CE::Rendering::resolve_draw_packet(std::make_shared<ContractGeometry>(), 0, 6, style, {}, {}, {}),
        CE::Exceptions::invalid_args
    );
}
