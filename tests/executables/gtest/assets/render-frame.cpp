#include <gtest/gtest.h>

#include <assets/submission/draw2d.h>
#include <core/rendering/render-frame.h>

#include <chrono>
#include <memory>
#include <utility>
#include <variant>

namespace {
    class FrameGeometry final : public CE::Assets::Geometry2D {
    public:
        CE::Assets::VertexLayout2D vertex_layout() const noexcept override { return CE::Assets::VertexLayout2D::Position3UV2; }
        CE::Assets::PrimitiveTopology topology() const noexcept override { return CE::Assets::PrimitiveTopology::TriangleStrip; }
        std::size_t vertex_count() const noexcept override { return 16; }
        void bind() const override { FAIL() << "Frame preparation must not bind geometry"; }
        void draw(
            std::size_t,
            std::size_t
        ) const override {
            FAIL() << "Frame preparation must not draw geometry";
        }
    };
    class FramePipeline final : public CE::Assets::Pipeline {
    public:
        FramePipeline()
        : Pipeline(make_definition()) {}

    private:
        static CE::Assets::PipelineDefinition make_definition() {
            using namespace CE::Assets;
            PipelineDefinition definition;
            definition.program_sources = {"frame.vert", "frame.frag"};
            definition.topology = PrimitiveTopology::TriangleStrip;
            definition.parameters = {{"model", ParameterType::Mat4, true, ParameterSemantic::Model},
                {"view", ParameterType::Mat4, true, ParameterSemantic::View}};
            return definition;
        }
    };
    CE::RenderAPIs::DrawPacket2D make_packet(
        const CE::Assets::Sprite& sprite,
        CE::Assets::CellIndex cell,
        const glm::mat4& model = glm::mat4{1.0f},
        const CE::Assets::ShaderPass& pass = CE::Assets::ShaderPass{}
    ) {
        CE::RenderAPIs::DrawStyle2D style;
        style.material = std::make_shared<CE::Assets::Material>(CE::Assets::MaterialDefinition{std::make_shared<FramePipeline>(), {}});
        style.model_matrix = model;
        CE::Assets::SubmissionContext2D context;
        context.pass = pass;
        return CE::Assets::resolve_sprite(sprite, cell, style, context);
    }

    std::shared_ptr<CE::Assets::Sprite> make_sprite() {
        using namespace std::chrono_literals;
        CE::Assets::SpriteDefinition definition;
        definition.grid.frame = {16, 16};
        definition.grid.rows = 1;
        definition.grid.columns = 4;
        definition.animations.push_back({.name = "walk", .frames = {{0, 100ms}, {1, 100ms}, {2, 100ms}, {3, 100ms}}, .loop = true});
        definition.animations.push_back({.name = "fall", .frames = {{0, 100ms}, {1, 100ms}, {2, 100ms}}, .loop = false});
        // CPU metadata is sufficient for resolution; these resources cannot issue GPU work.
        return std::make_shared<CE::Assets::Sprite>(CE::Assets::SpriteData{
            .geometry = std::make_shared<FrameGeometry>(), .texture = nullptr, .definition = std::move(definition)});
    }
}

TEST(
    sprite_playback,
    two_entities_share_frames_but_keep_separate_clocks
) {
    using namespace std::chrono_literals;
    auto sprite = make_sprite();
    auto first = sprite->animation("walk");
    auto second = sprite->animation("walk");

    first.advance(150ms);
    second.advance(250ms);
    EXPECT_EQ(first.cell(), 1u);
    EXPECT_EQ(second.cell(), 2u);
    EXPECT_EQ(first.definition().frames.data(), second.definition().frames.data());

    first.advance(800ms); // Skip two complete cycles without changing either clip definition.
    EXPECT_EQ(first.cell(), 1u);
    EXPECT_EQ(second.cell(), 2u);
    sprite.reset(); // The playback values retain their definition after the asset is released.
    EXPECT_EQ(first.definition().frames.size(), 4u);
}

TEST(
    sprite_playback,
    nonlooping_clip_stays_on_its_last_frame
) {
    using namespace std::chrono_literals;
    auto sprite = make_sprite();
    auto fall = sprite->animation("fall");

    fall.advance(5s);
    EXPECT_EQ(fall.cell(), 2u);
    fall.advance(1s);
    EXPECT_EQ(fall.index(), 2u);
    fall[0];
    EXPECT_EQ(fall.cell(), 0u);
}

TEST(
    render_frame,
    published_values_do_not_follow_simulation_changes
) {
    using namespace std::chrono_literals;
    auto sprite = make_sprite();
    auto playback = sprite->animation("walk");
    playback.advance(150ms);

    glm::mat4 view{1.0f};
    view[3][0] = -40.0f;
    glm::mat4 model{1.0f};
    model[3][0] = 5.0f;
    CE::RenderAPIs::RenderFrame frame;
    {
        CE::RenderAPIs::RenderFrameWriter writer(frame);
        auto world = writer.begin_pass(glm::mat4{1.0f}, view);
        world.add(make_packet(*sprite, playback.cell(), model, world.semantics()));
    }

    playback.advance(100ms);
    view[3][0] = -80.0f;
    model[3][0] = 6.0f;
    sprite.reset();
    const auto& draw = frame.passes()[0].draws[0];
    EXPECT_EQ(draw.first_vertex, 4u);
    ASSERT_TRUE(draw.geometry);
    EXPECT_FLOAT_EQ(frame.passes()[0].view[3][0], -40.0f);
    EXPECT_FLOAT_EQ(std::get<glm::mat4>(draw.parameters.at("model"))[3][0], 5.0f);
}

TEST(
    render_frame,
    recycled_slot_keeps_storage_and_releases_old_assets
) {
    auto sprite = make_sprite();
    std::weak_ptr<const CE::Assets::Geometry2D> retained = sprite->geometry;
    CE::RenderAPIs::RenderFrame frame;
    const CE::RenderAPIs::RenderPass* pass_storage = nullptr;
    const CE::RenderAPIs::DrawPacket2D* draw_storage = nullptr;
    {
        CE::RenderAPIs::RenderFrameWriter writer(frame);
        auto first = writer.begin_pass(glm::mat4{1.0f}, glm::mat4{1.0f});
        first.reserve_draws(2);
        auto second = writer.begin_pass(glm::mat4{1.0f}, glm::mat4{1.0f});
        // Adding another pass may grow the outer vector; the first writer still addresses pass zero.
        first.add(make_packet(*sprite, 0));
        first.add(make_packet(*sprite, 1));
        second.add(make_packet(*sprite, 2));
        ASSERT_EQ(frame.passes().size(), 2u);
        EXPECT_EQ(frame.passes()[0].draws.size(), 2u);
        EXPECT_EQ(frame.passes()[1].draws.size(), 1u);
        EXPECT_EQ(frame.passes()[0].draws[0].authored_order, 0u);
        EXPECT_EQ(frame.passes()[0].draws[1].authored_order, 1u);
        EXPECT_EQ(frame.passes()[1].draws[0].authored_order, 0u);
        pass_storage = frame.passes().data();
        draw_storage = frame.passes()[0].draws.data();
    }

    sprite.reset();
    ASSERT_FALSE(retained.expired());
    frame.recycle(); // Runtime calls this after rendering, while the graphics context is current.
    EXPECT_TRUE(retained.expired());
    EXPECT_TRUE(frame.passes().empty());

    {
        CE::RenderAPIs::RenderFrameWriter writer(frame);
        glm::mat4 next_view{1.0f};
        next_view[3][0] = -6.0f;
        auto first = writer.begin_pass(glm::mat4{1.0f}, next_view);
        first.add(make_packet(*make_sprite(), 2));
    }
    ASSERT_EQ(frame.passes().size(), 1u);
    EXPECT_EQ(frame.passes().data(), pass_storage);
    EXPECT_EQ(frame.passes()[0].draws.data(), draw_storage);
    EXPECT_EQ(frame.passes()[0].draws.size(), 1u);
    EXPECT_FLOAT_EQ(frame.passes()[0].view[3][0], -6.0f);
}

TEST(
    render_frame,
    invalid_group_does_not_publish_a_partial_sequence
) {
    const auto sprite = make_sprite();
    CE::RenderAPIs::RenderFrame frame;
    CE::RenderAPIs::RenderFrameWriter writer(frame);
    auto pass = writer.begin_pass(glm::mat4{1.0f}, glm::mat4{1.0f});
    auto good = make_packet(*sprite, 0);
    auto bad = make_packet(*sprite, 1);
    bad.first_vertex = 16;
    std::vector<CE::RenderAPIs::DrawPacket2D> group{good, bad};
    EXPECT_THROW(pass.add(std::move(group)), CE::Exceptions::invalid_args);
    EXPECT_TRUE(frame.passes()[0].draws.empty());
    pass.add(std::move(good));
    EXPECT_EQ(frame.passes()[0].draws.size(), 1u);
}

TEST(
    render_frame,
    pass_constraints_reject_conflicting_pipeline_state
) {
    const auto sprite = make_sprite();
    CE::RenderAPIs::RenderFrame frame;
    CE::RenderAPIs::RenderFrameWriter writer(frame);
    CE::Assets::PassConstraints2D constraints;
    constraints.blend = CE::Assets::BlendMode::Opaque;
    auto pass = writer.begin_pass(glm::mat4{1.0f}, glm::mat4{1.0f}, constraints);
    EXPECT_THROW(pass.add(make_packet(*sprite, 0)), CE::Exceptions::invalid_args);
    EXPECT_TRUE(frame.passes()[0].draws.empty());
}
