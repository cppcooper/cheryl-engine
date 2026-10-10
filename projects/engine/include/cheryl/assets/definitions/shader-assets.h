#pragma once

#include <assets/definitions/pipeline.h>
#include <assets/resources/sampler.h>

#include <map>
#include <string>
#include <string_view>
#include <variant>

namespace CE::Assets {
    enum class ShaderStage { Vertex, Fragment };

    // Owned literals deliberately cannot retain images, samplers or backend handles.
    using ShaderLiteral = std::variant<float, int, unsigned int, bool, glm::vec2, glm::vec3, glm::vec4, glm::mat4>;

    struct ShaderSource {
        ShaderStage stage = ShaderStage::Vertex;
        std::filesystem::path path;
        std::string bytes;
    };

    struct ShaderProgramRecipe {
        std::string id;
        std::vector<ShaderSource> sources;
    };

    struct ShaderParameter {
        std::string key;
        ParameterType type = ParameterType::Float;
        bool required = true;
        ParameterSemantic semantic = ParameterSemantic::Custom;
        std::optional<ShaderLiteral> default_value = {};
    };

    struct ShaderMaterialRecipe {
        std::string id;
        std::string program;
        VertexLayout2D vertex_layout = VertexLayout2D::Position3UV2;
        PrimitiveTopology topology = PrimitiveTopology::Triangles;
        PipelineState2D state;
        std::vector<ShaderParameter> parameters;
        std::map<std::string, ShaderLiteral, std::less<>> defaults;
        // FX policies address sampler parameters, never image assets.
        std::map<std::string, SamplerOptions, std::less<>> sampling;
        // Serialized backend payloads are interpreted only by that backend's builder.
        std::map<std::string, std::string, std::less<>> bindings;
    };

    struct ShaderAssetManifest {
        std::filesystem::path source;
        std::string name_space;
        std::vector<ShaderProgramRecipe> programs;
        std::vector<ShaderMaterialRecipe> materials;
    };

    [[nodiscard]] bool is_shader_asset_id(std::string_view id);
    [[nodiscard]] ParameterValue shader_parameter_value(const ShaderLiteral& literal);
    [[nodiscard]] ParameterSet shader_material_defaults(const ShaderMaterialRecipe& recipe);
    [[nodiscard]] PipelineDefinition shader_pipeline_definition(const ShaderMaterialRecipe& recipe, const ShaderProgramRecipe& program);
    void validate_shader_program(const ShaderProgramRecipe& recipe);
    void validate_shader_material(const ShaderMaterialRecipe& recipe, const ShaderProgramRecipe& program);
}
