#include <core/resources/asset-management/manifest-loader.h>
#include "manifest-parser.h"

#include <internals/exceptions.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <utility>
#include <unordered_set>

namespace CE::Assets::Manifests {
    namespace {
        using json = nlohmann::json;
        namespace fs = std::filesystem;

        [[noreturn]] void fail(const std::filesystem::path& source, const std::string& location, const std::string& message) {
            throw Exceptions::runtime_exception(CE_HERE, "Shader assets '" + source.string() + "' at " + location + ": " + message);
        }

        void object(const json& value, const std::filesystem::path& source, const std::string& location) {
            if (!value.is_object())
                fail(source, location, "expected an object");
        }

        void fields(
            const json& value,
            const std::filesystem::path& source,
            const std::string& location,
            const std::initializer_list<std::string_view> allowed
        ) {
            object(value, source, location);
            for (const auto& item : value.items())
                if (std::ranges::find(allowed, item.key()) == allowed.end())
                    fail(source, location, "unknown property '" + item.key() + "'");
        }

        const json& required(const json& value, const std::string& key, const std::filesystem::path& source, const std::string& location) {
            object(value, source, location);
            if (!value.contains(key))
                fail(source, location, "missing property '" + key + "'");
            return value.at(key);
        }

        std::string text(const json& value, const std::filesystem::path& source, const std::string& location) {
            if (!value.is_string() || value.get_ref<const std::string&>().empty())
                fail(source, location, "expected a nonempty string");
            return value.get<std::string>();
        }

        template <typename T>
        T choice(
            const json& value,
            const std::filesystem::path& source,
            const std::string& location,
            const std::initializer_list<std::pair<std::string_view, T>> choices
        ) {
            const auto name = text(value, source, location);
            for (const auto& [key, result] : choices)
                if (key == name)
                    return result;
            fail(source, location, "unsupported value '" + name + "'");
        }

        bool boolean(const json& value, const std::filesystem::path& source, const std::string& location) {
            if (!value.is_boolean())
                fail(source, location, "expected a boolean");
            return value.get<bool>();
        }

        std::string qualified(const json& value, const std::filesystem::path& source, const std::string& location) {
            const auto id = text(value, source, location);
            if (!CE::Assets::Shaders::is_qualified_id(id))
                fail(source, location, "expected a qualified lowercase namespace:name identifier");
            return id;
        }

        float number(const json& value, const std::filesystem::path& source, const std::string& location) {
            if (!value.is_number())
                fail(source, location, "expected a finite float");
            const auto result = value.get<double>();
            if (!std::isfinite(result) || std::abs(result) > std::numeric_limits<float>::max())
                fail(source, location, "number is outside the finite float range");
            return static_cast<float>(result);
        }

        CE::Assets::Shaders::Literal literal(
            const json& value,
            const CE::Rendering::Parameters::Type type,
            const std::filesystem::path& source,
            const std::string& location
        ) {
            if (type == CE::Rendering::Parameters::Type::Float)
                return number(value, source, location);
            if (type == CE::Rendering::Parameters::Type::Bool)
                return boolean(value, source, location);
            if (type == CE::Rendering::Parameters::Type::Int || type == CE::Rendering::Parameters::Type::UInt) {
                if (!value.is_number_integer())
                    fail(source, location, "expected an integer literal");
                if (type == CE::Rendering::Parameters::Type::Int) {
                    if ((value.is_number_unsigned() && value.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) ||
                        (!value.is_number_unsigned() && (value.get<std::int64_t>() < std::numeric_limits<int>::min() ||
                                                        value.get<std::int64_t>() > std::numeric_limits<int>::max())))
                        fail(source, location, "integer is outside the signed range");
                    return value.get<int>();
                }
                if ((!value.is_number_unsigned() && value.get<std::int64_t>() < 0) ||
                    value.get<std::uint64_t>() > std::numeric_limits<unsigned int>::max())
                    fail(source, location, "integer is outside the unsigned range");
                return value.get<unsigned int>();
            }
            const std::size_t count = type == CE::Rendering::Parameters::Type::Vec2   ? 2
                                      : type == CE::Rendering::Parameters::Type::Vec3 ? 3
                                      : type == CE::Rendering::Parameters::Type::Vec4 ? 4
                                      : type == CE::Rendering::Parameters::Type::Mat4 ? 16
                                                                                      : 0;
            if (count == 0)
                fail(source, location, "sampler parameters cannot contain default images");
            if (!value.is_array() || value.size() != count)
                fail(source, location, "expected an array with " + std::to_string(count) + " components");
            std::array<float, 16> parts{};
            for (std::size_t i = 0; i < count; ++i)
                parts[i] = number(value[i], source, location + '[' + std::to_string(i) + ']');
            if (type == CE::Rendering::Parameters::Type::Vec2)
                return glm::vec2(parts[0], parts[1]);
            if (type == CE::Rendering::Parameters::Type::Vec3)
                return glm::vec3(parts[0], parts[1], parts[2]);
            if (type == CE::Rendering::Parameters::Type::Vec4)
                return glm::vec4(parts[0], parts[1], parts[2], parts[3]);
            glm::mat4 result(0);
            for (glm::length_t column = 0; column < 4; ++column)
                for (glm::length_t row = 0; row < 4; ++row)
                    result[column][row] = parts[static_cast<std::size_t>(column * 4 + row)];
            return result;
        }

        CE::Assets::Shaders::ParameterRecipe
        parameter(const json& value, const std::filesystem::path& source, const std::string& location) {
            fields(value, source, location, {"key", "type", "required", "semantic", "default"});
            CE::Assets::Shaders::ParameterRecipe result;
            result.key = text(required(value, "key", source, location), source, location + ".key");
            result.type = choice<CE::Rendering::Parameters::Type>(
                required(value, "type", source, location), source, location + ".type",
                {{"float", CE::Rendering::Parameters::Type::Float},
                 {"int", CE::Rendering::Parameters::Type::Int},
                 {"uint", CE::Rendering::Parameters::Type::UInt},
                 {"bool", CE::Rendering::Parameters::Type::Bool},
                 {"vec2", CE::Rendering::Parameters::Type::Vec2},
                 {"vec3", CE::Rendering::Parameters::Type::Vec3},
                 {"vec4", CE::Rendering::Parameters::Type::Vec4},
                 {"mat4", CE::Rendering::Parameters::Type::Mat4},
                 {"sampler2d", CE::Rendering::Parameters::Type::Sampler2D}}
            );
            if (value.contains("required"))
                result.required = boolean(value.at("required"), source, location + ".required");
            if (value.contains("semantic"))
                result.semantic = choice<CE::Rendering::Parameters::Semantic>(
                    value.at("semantic"), source, location + ".semantic",
                    {{"custom", CE::Rendering::Parameters::Semantic::Custom},
                     {"projection", CE::Rendering::Parameters::Semantic::Projection},
                     {"view", CE::Rendering::Parameters::Semantic::View},
                     {"model", CE::Rendering::Parameters::Semantic::Model},
                     {"alpha", CE::Rendering::Parameters::Semantic::Alpha},
                     {"scale", CE::Rendering::Parameters::Semantic::Scale}}
                );
            if (value.contains("default"))
                result.default_value = literal(value.at("default"), result.type, source, location + ".default");
            return result;
        }

        CE::Rendering::Sampling::Options sampling(const json& value, const std::filesystem::path& source, const std::string& location) {
            fields(value, source, location, {"minification", "magnification", "mipmaps", "wrap_u", "wrap_v", "anisotropy"});
            CE::Rendering::Sampling::Options result;
            const auto filter = [&](const std::string& key, CE::Rendering::Sampling::Filter& target) {
                if (value.contains(key))
                    target = choice<CE::Rendering::Sampling::Filter>(
                        value.at(key), source, location + '.' + key,
                        {{"nearest", CE::Rendering::Sampling::Filter::Nearest}, {"linear", CE::Rendering::Sampling::Filter::Linear}}
                    );
            };
            filter("minification", result.minification);
            filter("magnification", result.magnification);
            if (value.contains("mipmaps"))
                result.mipmaps = choice<CE::Rendering::Sampling::MipmapFilter>(
                    value.at("mipmaps"), source, location + ".mipmaps",
                    {{"none", CE::Rendering::Sampling::MipmapFilter::None},
                     {"nearest", CE::Rendering::Sampling::MipmapFilter::Nearest},
                     {"linear", CE::Rendering::Sampling::MipmapFilter::Linear}}
                );
            const auto wrap = [&](const std::string& key, CE::Rendering::Sampling::Wrap& target) {
                if (value.contains(key))
                    target = choice<CE::Rendering::Sampling::Wrap>(
                        value.at(key), source, location + '.' + key,
                        {{"clamp_to_edge", CE::Rendering::Sampling::Wrap::ClampToEdge},
                         {"repeat", CE::Rendering::Sampling::Wrap::Repeat},
                         {"mirrored_repeat", CE::Rendering::Sampling::Wrap::MirroredRepeat}}
                    );
            };
            wrap("wrap_u", result.wrap_u);
            wrap("wrap_v", result.wrap_v);
            if (value.contains("anisotropy"))
                result.anisotropy = choice<CE::Rendering::Sampling::Anisotropy>(
                    value.at("anisotropy"), source, location + ".anisotropy",
                    {{"disabled", CE::Rendering::Sampling::Anisotropy::Disabled},
                     {"maximum_supported", CE::Rendering::Sampling::Anisotropy::MaximumSupported}}
                );
            return result;
        }

        CE::Assets::Shaders::MaterialRecipe
        material(const json& value, const std::string& id, const std::filesystem::path& source, const std::string& location) {
            fields(value, source, location, {"name", "program", "vertex_layout", "topology", "state", "parameters", "defaults", "sampling", "bindings"});
            CE::Assets::Shaders::MaterialRecipe result;
            result.id = id;
            result.program = qualified(required(value, "program", source, location), source, location + ".program");
            result.vertex_layout = choice<CE::Rendering::Geometry::Layout2D>(
                required(value, "vertex_layout", source, location), source, location + ".vertex_layout",
                {{"position3_uv2", CE::Rendering::Geometry::Layout2D::Position3UV2},
                 {"position3_uv2_color4", CE::Rendering::Geometry::Layout2D::Position3UV2Color4}}
            );
            result.topology = choice<CE::Rendering::Geometry::Topology>(
                required(value, "topology", source, location), source, location + ".topology",
                {{"triangles", CE::Rendering::Geometry::Topology::Triangles},
                 {"triangle_strip", CE::Rendering::Geometry::Topology::TriangleStrip}}
            );
            if (value.contains("state")) {
                const auto& state = value.at("state");
                fields(state, source, location + ".state", {"blend", "depth", "depth_write", "cull"});
                if (state.contains("blend"))
                    result.state.blend = choice<CE::Rendering::Pipelines::Blend>(
                        state.at("blend"), source, location + ".state.blend",
                        {{"opaque", CE::Rendering::Pipelines::Blend::Opaque},
                         {"straight_alpha", CE::Rendering::Pipelines::Blend::StraightAlpha},
                         {"premultiplied_alpha", CE::Rendering::Pipelines::Blend::PremultipliedAlpha},
                         {"additive", CE::Rendering::Pipelines::Blend::Additive}}
                    );
                if (state.contains("depth"))
                    result.state.depth = choice<CE::Rendering::Pipelines::Depth>(
                        state.at("depth"), source, location + ".state.depth",
                        {{"disabled", CE::Rendering::Pipelines::Depth::Disabled},
                         {"less", CE::Rendering::Pipelines::Depth::Less},
                         {"less_equal", CE::Rendering::Pipelines::Depth::LessEqual}}
                    );
                if (state.contains("depth_write"))
                    result.state.depth_write = boolean(state.at("depth_write"), source, location + ".state.depth_write");
                if (state.contains("cull"))
                    result.state.cull = choice<CE::Rendering::Pipelines::Cull>(
                        state.at("cull"), source, location + ".state.cull",
                        {{"none", CE::Rendering::Pipelines::Cull::None},
                         {"front", CE::Rendering::Pipelines::Cull::Front},
                         {"back", CE::Rendering::Pipelines::Cull::Back}}
                    );
            }
            const auto& parameters = required(value, "parameters", source, location);
            if (!parameters.is_array())
                fail(source, location + ".parameters", "expected an array");
            for (std::size_t i = 0; i < parameters.size(); ++i)
                result.parameters.push_back(parameter(parameters[i], source, location + ".parameters[" + std::to_string(i) + ']'));
            if (value.contains("defaults")) {
                object(value.at("defaults"), source, location + ".defaults");
                for (const auto& item : value.at("defaults").items()) {
                    const auto found = std::ranges::find(result.parameters, item.key(), &CE::Assets::Shaders::ParameterRecipe::key);
                    if (found == result.parameters.end())
                        fail(source, location + ".defaults." + item.key(), "unknown parameter");
                    result.defaults.emplace(item.key(), literal(item.value(), found->type, source, location + ".defaults." + item.key()));
                }
            }
            if (value.contains("sampling")) {
                object(value.at("sampling"), source, location + ".sampling");
                for (const auto& item : value.at("sampling").items())
                    result.sampling.emplace(item.key(), sampling(item.value(), source, location + ".sampling." + item.key()));
            }
            const auto& bindings = required(value, "bindings", source, location);
            object(bindings, source, location + ".bindings");
            for (const auto& item : bindings.items()) {
                object(item.value(), source, location + ".bindings." + item.key());
                result.bindings.emplace(item.key(), item.value().dump());
            }
            // Cross-document program resolution follows parsing; common contract
            // checks do not require source bytes or native construction.
            const CE::Assets::Shaders::ProgramRecipe placeholder{
                result.program, {{CE::Assets::Shaders::Stage::Vertex, "vertex", {}}, {CE::Assets::Shaders::Stage::Fragment, "fragment", {}}}
            };
            try {
                CE::Assets::Shaders::validate_material(result, placeholder);
            } catch (const std::exception& error) {
                fail(source, location, error.what());
            }
            return result;
        }
    }

    json Detail::read_document(std::istream& input, const std::filesystem::path& source) {
        std::vector<std::unordered_set<std::string>> keys;
        try {
            return json::parse(input, [&](const int, const json::parse_event_t event, json& parsed) {
                if (event == json::parse_event_t::object_start)
                    keys.emplace_back();
                else if (event == json::parse_event_t::object_end)
                    keys.pop_back();
                else if (event == json::parse_event_t::key && !keys.back().insert(parsed.get<std::string>()).second)
                    fail(source, "$", "duplicate JSON property '" + parsed.get<std::string>() + "'");
                return true;
            });
        } catch (const json::exception& error) {
            fail(source, "$", "unable to parse JSON: " + std::string(error.what()));
        }
    }

    CE::Assets::Shaders::Manifest Detail::parse_shader(const json& root, const std::filesystem::path& source) {
        fields(root, source, "$", {"$schema", "asset_class", "version", "namespace", "programs", "materials"});
        if (text(required(root, "asset_class", source, "$"), source, "$.asset_class") != "shader")
            fail(source, "$.asset_class", "expected shader asset class");
        if (text(required(root, "version", source, "$"), source, "$.version") != "2.0")
            fail(source, "$.version", "unsupported shader asset version");
        if (root.contains("$schema"))
            static_cast<void>(text(root.at("$schema"), source, "$.$schema"));
        static_cast<void>(Detail::graphics_directory(source));
        CE::Assets::Shaders::Manifest result;
        result.source = source;
        result.name_space = text(required(root, "namespace", source, "$"), source, "$.namespace");
        if (!CE::Assets::Shaders::is_qualified_id(result.name_space + ":probe"))
            fail(source, "$.namespace", "expected a lowercase namespace");
        std::unordered_set<std::string> ids;
        const auto id_for = [&](const json& value, const std::string& location) {
            const auto id = result.name_space + ':' + text(required(value, "name", source, location), source, location + ".name");
            if (!CE::Assets::Shaders::is_qualified_id(id) || !ids.emplace(id).second)
                fail(source, location + ".name", "invalid or duplicate shader asset ID '" + id + "'");
            return id;
        };
        const auto& programs = required(root, "programs", source, "$");
        if (!programs.is_array())
            fail(source, "$.programs", "expected an array");
        for (std::size_t i = 0; i < programs.size(); ++i) {
            const auto location = "$.programs[" + std::to_string(i) + ']';
            const auto& value = programs[i];
            fields(value, source, location, {"name", "stages"});
            CE::Assets::Shaders::ProgramRecipe recipe;
            recipe.id = id_for(value, location);
            const auto& stages = required(value, "stages", source, location);
            fields(stages, source, location + ".stages", {"vertex", "fragment"});
            for (const auto& [name, stage] :
                 {std::pair{"vertex", CE::Assets::Shaders::Stage::Vertex}, std::pair{"fragment", CE::Assets::Shaders::Stage::Fragment}}) {
                const auto path = text(required(stages, name, source, location + ".stages"), source, location + ".stages." + name);
                recipe.sources.push_back({stage, Detail::graphics_path(source, path, location + ".stages." + name), {}});
            }
            CE::Assets::Shaders::validate_program(recipe);
            result.programs.push_back(std::move(recipe));
        }
        const auto& materials = required(root, "materials", source, "$");
        if (!materials.is_array() || (materials.empty() && programs.empty()))
            fail(source, "$.materials", "expected an array in a nonempty shader asset document");
        for (std::size_t i = 0; i < materials.size(); ++i) {
            const auto location = "$.materials[" + std::to_string(i) + ']';
            result.materials.push_back(material(materials[i], id_for(materials[i], location), source, location));
        }
        return result;
    }

    CE::Assets::Shaders::Manifest Reader::load_shader(const std::filesystem::path& file) {
        std::ifstream input(file);
        if (!input)
            fail(file, "$", "unable to open definition");
        return parse_shader(input, file);
    }

    CE::Assets::Shaders::Manifest Reader::parse_shader(std::istream& input, const std::filesystem::path& source) {
        return Detail::parse_shader(Detail::read_document(input, source), source);
    }
}
