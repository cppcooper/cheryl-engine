#include <gtest/gtest.h>

#include <assets/submission/draw2d.h>
#include <core/rendering/render-frame.h>

#include <chrono>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace {
    class FrameGeometry final : public CE::Rendering::Geometry::Buffer2D {
    public:
        CE::Rendering::Geometry::Layout2D vertex_layout() const noexcept override {
            return CE::Rendering::Geometry::Layout2D::Position3UV2;
        }
        CE::Rendering::Geometry::Topology topology() const noexcept override { return CE::Rendering::Geometry::Topology::TriangleStrip; }
        std::size_t vertex_count() const noexcept override { return 16; }
        void bind() const override { FAIL() << "Frame preparation must not bind geometry"; }
        void draw(std::size_t, std::size_t) const override { FAIL() << "Frame preparation must not draw geometry"; }
    };
    class FramePipeline final : public CE::Rendering::Resources::Pipeline {
    public:
        FramePipeline()
        : CE::Rendering::Resources::Pipeline(make_definition()) {}

    private:
        static CE::Rendering::Pipelines::Definition make_definition() {
            CE::Rendering::Pipelines::Definition definition;
            definition.program_sources = {"frame.vert", "frame.frag"};
            definition.topology = CE::Rendering::Geometry::Topology::TriangleStrip;
            definition.parameters = {
                {"model", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Model},
                {"view", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::View}
            };
            return definition;
        }
    };
    CE::Rendering::DrawPacket2D make_packet(
        const CE::Assets::Sprites::Asset& sprite,
        CE::Assets::Grids::CellIndex cell,
        const glm::mat4& model = glm::mat4{1.0f},
        const CE::Rendering::Parameters::PassSemantics& pass = CE::Rendering::Parameters::PassSemantics{}
    ) {
        CE::Rendering::DrawStyle2D style;
        style.material = std::make_shared<CE::Rendering::Resources::Material>(
            CE::Rendering::Materials::Definition{std::make_shared<FramePipeline>(), {}}
        );
        style.model_matrix = model;
        CE::Rendering::Submission::Context2D context;
        context.pass = pass;
        context.image = std::nullopt; // This fixture's pipeline has no image parameter.
        return CE::Rendering::Submission::resolve_sprite(sprite, cell, style, context);
    }

    std::shared_ptr<CE::Assets::Sprites::Asset> make_sprite() {
        using namespace std::chrono_literals;
        CE::Assets::Sprites::Definition definition;
        definition.grid.frame = {16, 16};
        definition.grid.rows = 1;
        definition.grid.columns = 4;
        definition.animations.push_back({.name = "walk", .frames = {{0, 100ms}, {1, 100ms}, {2, 100ms}, {3, 100ms}}, .loop = true});
        definition.animations.push_back({.name = "fall", .frames = {{0, 100ms}, {1, 100ms}, {2, 100ms}}, .loop = false});
        // CPU metadata is sufficient for resolution; these resources cannot issue GPU work.
        return std::make_shared<CE::Assets::Sprites::Asset>(CE::Assets::Sprites::Data{
            .geometry = std::make_shared<FrameGeometry>(), .texture = nullptr, .definition = std::move(definition)
        });
    }
}

TEST(sprite_playback, independent_clocks) {
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

TEST(sprite_playback, nonlooping_last_frame) {
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

TEST(render_frame, immutable_publication) {
    using namespace std::chrono_literals;
    auto sprite = make_sprite();
    auto playback = sprite->animation("walk");
    playback.advance(150ms);

    glm::mat4 view{1.0f};
    view[3][0] = -40.0f;
    glm::mat4 model{1.0f};
    model[3][0] = 5.0f;
    CE::Rendering::Frame frame;
    {
        CE::Rendering::FrameWriter writer(frame);
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

TEST(render_frame, slot_recycling) {
    auto sprite = make_sprite();
    std::weak_ptr<const CE::Rendering::Geometry::Buffer2D> retained = sprite->geometry;
    CE::Rendering::Frame frame;
    const CE::Rendering::Pass* pass_storage = nullptr;
    const CE::Rendering::DrawPacket2D* draw_storage = nullptr;
    {
        CE::Rendering::FrameWriter writer(frame);
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
        CE::Rendering::FrameWriter writer(frame);
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

TEST(render_frame, invalid_draw_group) {
    const auto sprite = make_sprite();
    CE::Rendering::Frame frame;
    CE::Rendering::FrameWriter writer(frame);
    auto pass = writer.begin_pass(glm::mat4{1.0f}, glm::mat4{1.0f});
    auto good = make_packet(*sprite, 0);
    auto bad = make_packet(*sprite, 1);
    bad.first_vertex = 16;
    std::vector<CE::Rendering::DrawPacket2D> group{good, bad};
    EXPECT_THROW(pass.add(std::move(group)), CE::Exceptions::invalid_args);
    EXPECT_TRUE(frame.passes()[0].draws.empty());
    pass.add(std::move(good));
    EXPECT_EQ(frame.passes()[0].draws.size(), 1u);
}

TEST(render_frame, conflicting_pass_state) {
    const auto sprite = make_sprite();
    CE::Rendering::Frame frame;
    CE::Rendering::FrameWriter writer(frame);
    CE::Rendering::Pipelines::PassConstraints2D constraints;
    constraints.blend = CE::Rendering::Pipelines::Blend::Opaque;
    auto pass = writer.begin_pass(glm::mat4{1.0f}, glm::mat4{1.0f}, constraints);
    auto packet = make_packet(*sprite, 0);
    EXPECT_THROW(pass.add(std::move(packet)), CE::Exceptions::invalid_args);
    EXPECT_TRUE(frame.passes()[0].draws.empty());
}
