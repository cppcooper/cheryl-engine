#pragma once

#include <assets/definitions/pipeline.h>
#include <assets/resources/sampler.h>

#include <map>
#include <string>
#include <string_view>
#include <variant>

namespace CE::Assets::Shaders {
    enum class Stage { Vertex, Fragment };

    // Owned literals deliberately cannot retain images, samplers or backend handles.
    using Literal = std::variant<float, int, unsigned int, bool, glm::vec2, glm::vec3, glm::vec4, glm::mat4>;

    struct Source {
        Stage stage = Stage::Vertex;
        std::filesystem::path path;
        std::string bytes;
    };

    struct ProgramRecipe {
        std::string id;
        std::vector<Source> sources;
    };

    struct ParameterRecipe {
        std::string key;
        CE::Rendering::Parameters::Type type = CE::Rendering::Parameters::Type::Float;
        bool required = true;
        CE::Rendering::Parameters::Semantic semantic = CE::Rendering::Parameters::Semantic::Custom;
        std::optional<Literal> default_value = {};
    };

    struct MaterialRecipe {
        std::string id;
        std::string program;
        CE::Rendering::Geometry::Layout2D vertex_layout = CE::Rendering::Geometry::Layout2D::Position3UV2;
        CE::Rendering::Geometry::Topology topology = CE::Rendering::Geometry::Topology::Triangles;
        CE::Rendering::Pipelines::State2D state;
        std::vector<ParameterRecipe> parameters;
        std::map<std::string, Literal, std::less<>> defaults;
        // FX policies address sampler parameters, never image assets.
        std::map<std::string, CE::Rendering::Sampling::Options, std::less<>> sampling;
        // Serialized backend payloads are interpreted only by that backend's builder.
        std::map<std::string, std::string, std::less<>> bindings;
    };

    struct Manifest {
        std::filesystem::path source;
        std::string name_space;
        std::vector<ProgramRecipe> programs;
        std::vector<MaterialRecipe> materials;
    };

    [[nodiscard]] bool is_qualified_id(std::string_view id);
    [[nodiscard]] CE::Rendering::Parameters::Value parameter_value(const Literal& literal);
    [[nodiscard]] CE::Rendering::Parameters::Set defaults(const MaterialRecipe& recipe);
    [[nodiscard]] CE::Rendering::Pipelines::Definition pipeline_definition(const MaterialRecipe& recipe, const ProgramRecipe& program);
    void validate_program(const ProgramRecipe& recipe);
    void validate_material(const MaterialRecipe& recipe, const ProgramRecipe& program);
}
