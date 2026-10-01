#include <assets/resources/pipeline.h>

#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <memory>
#include <utility>

#ifdef GL_VERSION_3_3
#error The pipeline and parameter contracts must not include OpenGL.
#endif

namespace {
    using namespace CE::Assets;
    using CE::Exceptions::invalid_args;

    class RecordingPipeline final : public Pipeline {
    public:
        explicit RecordingPipeline(PipelineDefinition definition)
        : Pipeline(std::move(definition)) {}
    };

    struct RecordingImage final : Image {
        [[nodiscard]] PixelSize pixel_size() const override { return {1, 1}; }
    };

    PipelineDefinition effect_definition() {
        PipelineDefinition definition;
        definition.program_sources = {"effect.vert", "effect.frag"};
        definition.parameters = {
            {"camera", ParameterType::Mat4, true, ParameterSemantic::Projection},
            {"transform", ParameterType::Mat4, true, ParameterSemantic::Model},
            {"time", ParameterType::Float},
            {"color", ParameterType::Vec4, true, ParameterSemantic::Custom, glm::vec4{1.0f}},
            {"intensity", ParameterType::Float, true, ParameterSemantic::Custom, 1.0f},
            {"base", ParameterType::Sampler2D},
            {"mask", ParameterType::Sampler2D},
            {"optional", ParameterType::Int, false}
        };
        return definition;
    }
}

TEST(pipeline_parameters, two_image_effect_copies_values_and_retains_images) {
    auto definition = effect_definition();
    auto pipeline = std::make_shared<RecordingPipeline>(definition);
    auto image = std::make_shared<RecordingImage>();
    std::weak_ptr<const Image> retained = image;
    MaterialDefinition recipe{pipeline, {{"base", ImageBinding{image, 0}}, {"mask", ImageBinding{image, 3}}, {"intensity", 0.5f}}};
    Material material(recipe);
    ParameterSet pass{{"time", 2.0f}, {"intensity", 0.25f}};
    ParameterSet draw{{"intensity", 0.75f}};
    ShaderPass camera;
    camera.projection[3][0] = 12.0f;
    ShaderDraw transform;
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
    EXPECT_EQ(std::get<ImageBinding>(resolved.at("mask")).unit, 3u);
    EXPECT_FALSE(resolved.contains("optional"));
    EXPECT_FLOAT_EQ(std::get<float>(material.resolve({}, {}, {{"time", 0.0f}}, {}).at("intensity")), 0.5f);
}

TEST(pipeline_parameters, invalid_hidden_values_and_engine_overrides_are_rejected) {
    const ParameterContract contract{
        {"weight", ParameterType::Float, true, ParameterSemantic::Custom, 1.0f},
        {"model", ParameterType::Mat4, true, ParameterSemantic::Model}
    };
    EXPECT_THROW(static_cast<void>(resolve_parameters(contract, {}, {}, {{"weight", 1}}, {}, {{"weight", 2.0f}})), invalid_args);
    EXPECT_THROW(static_cast<void>(resolve_parameters(contract, {}, {}, {}, {{"model", glm::mat4{1.0f}}}, {})), invalid_args);
    EXPECT_THROW(static_cast<void>(resolve_parameters(contract, {}, {}, {}, {}, {{"typo", 2.0f}})), invalid_args);
    EXPECT_FLOAT_EQ(std::get<float>(resolve_parameters(contract, {}, {}, {}, {}, {}).at("weight")), 1.0f);
}

TEST(pipeline_parameters, required_missing_null_and_colliding_samplers_are_rejected) {
    const ParameterContract contract{{"first", ParameterType::Sampler2D}, {"second", ParameterType::Sampler2D}};
    auto image = std::make_shared<RecordingImage>();
    EXPECT_THROW(static_cast<void>(resolve_parameters(contract, {}, {}, {}, {}, {})), invalid_args);
    EXPECT_THROW(static_cast<void>(resolve_parameters(contract, {}, {}, {}, {{"first", ImageBinding{}}}, {})), invalid_args);
    const ParameterSet bindings{{"first", ImageBinding{image, 0}}, {"second", ImageBinding{image, 0}}};
    EXPECT_THROW(static_cast<void>(resolve_parameters(contract, {}, {}, {}, bindings, {})), invalid_args);
}

TEST(pipeline_parameters, invalid_schema_is_rejected_before_material_publication) {
    EXPECT_THROW(validate_parameter_contract({{"same", ParameterType::Float}, {"same", ParameterType::Float}}), invalid_args);
    EXPECT_THROW(validate_parameter_contract({{"model", ParameterType::Float, true, ParameterSemantic::Model}}), invalid_args);
    EXPECT_THROW(validate_parameter_contract({{"value", ParameterType::Float, true, ParameterSemantic::Custom, 1}}), invalid_args);
    EXPECT_THROW(static_cast<void>(Material(MaterialDefinition{})), invalid_args);
}

TEST(pipeline_generations, caller_mutations_and_replacement_preserve_retained_snapshots) {
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
