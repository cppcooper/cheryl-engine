#include <assets/resources/pipeline.h>

#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <memory>
#include <utility>
#include <vector>

#ifdef GL_VERSION_3_3
#error The pipeline and parameter contracts must not include OpenGL.
#endif

namespace {
    using CE::Exceptions::invalid_args;

    class RecordingPipeline final : public CE::Rendering::Resources::Pipeline {
    public:
        explicit RecordingPipeline(CE::Rendering::Pipelines::Definition definition)
        : CE::Rendering::Resources::Pipeline(std::move(definition)) {}
    };

    struct RecordingImage final : CE::Rendering::Resources::Image {
        mutable std::vector<std::uint32_t> bound_units;

        [[nodiscard]] CE::Rendering::Geometry::PixelSize pixel_size() const override { return {1, 1}; }
        void bind(std::uint32_t unit) const override { bound_units.push_back(unit); }
    };

    struct RecordingGeometry final : CE::Rendering::Geometry::Buffer2D {
        mutable std::size_t binds = 0;
        CE::Rendering::Geometry::Topology uploaded_topology = CE::Rendering::Geometry::Topology::Triangles;
        std::size_t uploaded_vertices = 6;
        CE::Rendering::Geometry::Layout2D layout = CE::Rendering::Geometry::Layout2D::Position3UV2;

        [[nodiscard]] CE::Rendering::Geometry::Layout2D vertex_layout() const noexcept override { return layout; }
        [[nodiscard]] CE::Rendering::Geometry::Topology topology() const noexcept override { return uploaded_topology; }
        [[nodiscard]] std::size_t vertex_count() const noexcept override { return uploaded_vertices; }
        void bind() const override { ++binds; }
        void draw(std::size_t, std::size_t) const override {}
    };

    CE::Rendering::Pipelines::Definition effect_definition() {
        CE::Rendering::Pipelines::Definition definition;
        definition.program_sources = {"effect.vert", "effect.frag"};
        definition.parameters = {
            {"camera", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Projection},
            {"transform", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Model},
            {"time", CE::Rendering::Parameters::Type::Float},
            {"color", CE::Rendering::Parameters::Type::Vec4, true, CE::Rendering::Parameters::Semantic::Custom, glm::vec4{1.0f}},
            {"intensity", CE::Rendering::Parameters::Type::Float, true, CE::Rendering::Parameters::Semantic::Custom, 1.0f},
            {"base", CE::Rendering::Parameters::Type::Sampler2D},
            {"mask", CE::Rendering::Parameters::Type::Sampler2D},
            {"optional", CE::Rendering::Parameters::Type::Int, false}
        };
        return definition;
    }
}

TEST(pipeline_parameters, retained_effect_parameters) {
    auto definition = effect_definition();
    auto pipeline = std::make_shared<RecordingPipeline>(definition);
    auto image = std::make_shared<RecordingImage>();
    std::weak_ptr<const CE::Rendering::Resources::Image> retained = image;
    CE::Rendering::Materials::Definition recipe{
        pipeline,
        {{"base", CE::Rendering::Parameters::ImageBinding{image, 0}},
         {"mask", CE::Rendering::Parameters::ImageBinding{image, 3}},
         {"intensity", 0.5f}}
    };
    CE::Rendering::Resources::Material material(recipe);
    CE::Rendering::Parameters::Set pass{{"time", 2.0f}, {"intensity", 0.25f}};
    CE::Rendering::Parameters::Set draw{{"intensity", 0.75f}};
    CE::Rendering::Parameters::PassSemantics camera;
    camera.projection[3][0] = 12.0f;
    CE::Rendering::Parameters::DrawSemantics transform;
    transform.model[3][1] = 9.0f;
    const auto resolved = material.resolve(camera, transform, pass, draw);
    pass["time"] = 100.0f;
    draw["intensity"] = 100.0f;
    recipe.defaults.clear();
    image.reset();
    EXPECT_FALSE(retained.expired());
    EXPECT_FLOAT_EQ(std::get<float>(resolved.at("time")), 2.0f);
    EXPECT_FLOAT_EQ(std::get<float>(resolved.at("intensity")), 0.75f);
    EXPECT_FLOAT_EQ(std::get<glm::mat4>(resolved.at("camera"))[3][0], 12.0f);
    EXPECT_FLOAT_EQ(std::get<glm::mat4>(resolved.at("transform"))[3][1], 9.0f);
    EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(resolved.at("mask")).unit, 3u);
    EXPECT_FALSE(resolved.contains("optional"));
    EXPECT_FLOAT_EQ(std::get<float>(material.resolve({}, {}, {{"time", 0.0f}}, {}).at("intensity")), 0.5f);
}

TEST(pipeline_parameters, invalid_parameter_overrides) {
    const CE::Rendering::Parameters::Contract contract{
        {"weight", CE::Rendering::Parameters::Type::Float, true, CE::Rendering::Parameters::Semantic::Custom, 1.0f},
        {"model", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Model}
    };
    EXPECT_THROW(
        static_cast<void>(CE::Rendering::Parameters::resolve(contract, {}, {}, {{"weight", 1}}, {}, {{"weight", 2.0f}})), invalid_args
    );
    EXPECT_THROW(
        static_cast<void>(CE::Rendering::Parameters::resolve(contract, {}, {}, {}, {{"model", glm::mat4{1.0f}}}, {})), invalid_args
    );
    EXPECT_THROW(static_cast<void>(CE::Rendering::Parameters::resolve(contract, {}, {}, {}, {}, {{"typo", 2.0f}})), invalid_args);
    EXPECT_FLOAT_EQ(std::get<float>(CE::Rendering::Parameters::resolve(contract, {}, {}, {}, {}, {}).at("weight")), 1.0f);
}

TEST(pipeline_parameters, invalid_sampler_bindings) {
    const CE::Rendering::Parameters::Contract contract{
        {"first", CE::Rendering::Parameters::Type::Sampler2D}, {"second", CE::Rendering::Parameters::Type::Sampler2D}
    };
    auto image = std::make_shared<RecordingImage>();
    EXPECT_THROW(static_cast<void>(CE::Rendering::Parameters::resolve(contract, {}, {}, {}, {}, {})), invalid_args);
    EXPECT_THROW(
        static_cast<void>(
            CE::Rendering::Parameters::resolve(contract, {}, {}, {}, {{"first", CE::Rendering::Parameters::ImageBinding{}}}, {})
        ),
        invalid_args
    );
    const CE::Rendering::Parameters::Set bindings{
        {"first", CE::Rendering::Parameters::ImageBinding{image, 0}}, {"second", CE::Rendering::Parameters::ImageBinding{image, 0}}
    };
    EXPECT_THROW(static_cast<void>(CE::Rendering::Parameters::resolve(contract, {}, {}, {}, bindings, {})), invalid_args);
}

TEST(pipeline_parameters, invalid_parameter_schema) {
    EXPECT_THROW(
        CE::Rendering::Parameters::validate_contract(
            {{"same", CE::Rendering::Parameters::Type::Float}, {"same", CE::Rendering::Parameters::Type::Float}}
        ),
        invalid_args
    );
    EXPECT_THROW(
        CE::Rendering::Parameters::validate_contract(
            {{"model", CE::Rendering::Parameters::Type::Float, true, CE::Rendering::Parameters::Semantic::Model}}
        ),
        invalid_args
    );
    EXPECT_THROW(
        CE::Rendering::Parameters::validate_contract(
            {{"value", CE::Rendering::Parameters::Type::Float, true, CE::Rendering::Parameters::Semantic::Custom, 1}}
        ),
        invalid_args
    );
    EXPECT_THROW(static_cast<void>(CE::Rendering::Resources::Material(CE::Rendering::Materials::Definition{})), invalid_args);
}

TEST(pipeline_generations, retained_snapshots) {
    auto definition = effect_definition();
    auto current = std::make_shared<RecordingPipeline>(definition);
    const auto old = current;
    definition.parameters[4].default_value = 8.0f;
    current = std::make_shared<RecordingPipeline>(definition);
    EXPECT_NE(old.get(), current.get());
    EXPECT_FLOAT_EQ(std::get<float>(*old->definition().parameters[4].default_value), 1.0f);
    EXPECT_FLOAT_EQ(std::get<float>(*current->definition().parameters[4].default_value), 8.0f);
    definition.state.depth_write = true;
    EXPECT_THROW(current = std::make_shared<RecordingPipeline>(definition), invalid_args);
    EXPECT_FLOAT_EQ(std::get<float>(*current->definition().parameters[4].default_value), 8.0f);
}

TEST(material_resources, shared_image_units) {
    CE::Rendering::Pipelines::Definition definition;
    definition.program_sources = {"sprite.vert", "sprite.frag"};
    definition.parameters = {{"image", CE::Rendering::Parameters::Type::Sampler2D}};
    auto pipeline = std::make_shared<RecordingPipeline>(definition);
    auto image = std::make_shared<RecordingImage>();
    CE::Rendering::Resources::Material first({pipeline, {{"image", CE::Rendering::Parameters::ImageBinding{image, 1}}}});
    CE::Rendering::Resources::Material second({pipeline, {{"image", CE::Rendering::Parameters::ImageBinding{image, 4}}}});
    RecordingGeometry geometry;
    geometry.bind();
    EXPECT_TRUE(image->bound_units.empty());
    for (const auto* material : {&first, &second}) {
        const auto parameters = material->resolve({}, {}, {}, {});
        const auto& binding = std::get<CE::Rendering::Parameters::ImageBinding>(parameters.at("image"));
        binding.image->bind(binding.unit);
    }
    EXPECT_EQ(geometry.binds, 1u);
    EXPECT_EQ(image->bound_units, (std::vector<std::uint32_t>{1, 4}));
    EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(first.definition().defaults.at("image")).unit, 1u);
    EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(second.definition().defaults.at("image")).unit, 4u);
}

TEST(pipeline_geometry, invalid_geometry) {
    auto definition = effect_definition();
    RecordingPipeline pipeline(definition);
    RecordingGeometry geometry;
    EXPECT_NO_THROW(pipeline.validate_draw(geometry, 0, 6, {}));
    EXPECT_THROW(pipeline.validate_draw(geometry, 0, 0, {}), invalid_args);
    EXPECT_THROW(pipeline.validate_draw(geometry, 4, 3, {}), invalid_args);
    EXPECT_THROW(pipeline.validate_draw(geometry, 0, 4, {}), invalid_args);
    geometry.layout = CE::Rendering::Geometry::Layout2D::Unsupported;
    EXPECT_THROW(pipeline.validate_draw(geometry, 0, 6, {}), invalid_args);
    geometry.layout = CE::Rendering::Geometry::Layout2D::Position3UV2;
    geometry.uploaded_topology = CE::Rendering::Geometry::Topology::TriangleStrip;
    EXPECT_THROW(pipeline.validate_draw(geometry, 0, 4, {}), invalid_args);
    definition.topology = CE::Rendering::Geometry::Topology::TriangleStrip;
    RecordingPipeline strip(definition);
    EXPECT_NO_THROW(strip.validate_draw(geometry, 0, 4, {}));
    EXPECT_THROW(strip.validate_draw(geometry, 0, 2, {}), invalid_args);
    EXPECT_EQ(geometry.binds, 0u);
}

TEST(pipeline_geometry, colored_layout) {
    auto definition = effect_definition();
    definition.vertex_layout = CE::Rendering::Geometry::Layout2D::Position3UV2Color4;
    RecordingPipeline pipeline(definition);
    RecordingGeometry geometry;
    EXPECT_THROW(pipeline.validate_draw(geometry, 0, 6, {}), invalid_args);
    geometry.layout = CE::Rendering::Geometry::Layout2D::Position3UV2Color4;
    EXPECT_NO_THROW(pipeline.validate_draw(geometry, 0, 6, {}));
    EXPECT_EQ(geometry.binds, 0u);
}

TEST(pipeline_state, conflicting_pass_constraints) {
    auto definition = effect_definition();
    definition.state = {
        CE::Rendering::Pipelines::Blend::Opaque, CE::Rendering::Pipelines::Depth::LessEqual, true, CE::Rendering::Pipelines::Cull::Back
    };
    RecordingPipeline pipeline(definition);
    RecordingGeometry geometry;
    EXPECT_THROW(pipeline.validate_draw(geometry, 0, 6, {}), invalid_args);
    CE::Rendering::Pipelines::PassConstraints2D constraints;
    constraints.depth = CE::Rendering::Pipelines::Depth::LessEqual;
    constraints.depth_write = true;
    constraints.blend = CE::Rendering::Pipelines::Blend::Opaque;
    constraints.cull = CE::Rendering::Pipelines::Cull::Back;
    EXPECT_NO_THROW(pipeline.validate_draw(geometry, 0, 6, constraints));
    constraints.blend = CE::Rendering::Pipelines::Blend::StraightAlpha;
    EXPECT_THROW(pipeline.validate_draw(geometry, 0, 6, constraints), invalid_args);
    EXPECT_EQ(pipeline.definition().state.blend, CE::Rendering::Pipelines::Blend::Opaque);
    EXPECT_EQ(geometry.binds, 0u);
}
