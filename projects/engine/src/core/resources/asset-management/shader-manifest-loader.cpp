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

namespace CE::Assets {
    namespace {
        using json = nlohmann::json;
        namespace fs = std::filesystem;

        [[noreturn]] void fail(const fs::path& source, const std::string& location, const std::string& message) {
            throw Exceptions::runtime_exception(CE_HERE, "Shader assets '" + source.string() + "' at " + location + ": " + message);
        }

        void object(const json& value, const fs::path& source, const std::string& location) {
            if (!value.is_object())
                fail(source, location, "expected an object");
        }

        void fields(const json& value, const fs::path& source, const std::string& location,
                    const std::initializer_list<std::string_view> allowed) {
            object(value, source, location);
            for (const auto& item : value.items())
                if (std::ranges::find(allowed, item.key()) == allowed.end())
                    fail(source, location, "unknown property '" + item.key() + "'");
        }

        const json& required(const json& value, const std::string& key, const fs::path& source, const std::string& location) {
            object(value, source, location);
            if (!value.contains(key))
                fail(source, location, "missing property '" + key + "'");
            return value.at(key);
        }

        std::string text(const json& value, const fs::path& source, const std::string& location) {
            if (!value.is_string() || value.get_ref<const std::string&>().empty())
                fail(source, location, "expected a nonempty string");
            return value.get<std::string>();
        }

        template <typename T> T choice(const json& value, const fs::path& source, const std::string& location,
                                     const std::initializer_list<std::pair<std::string_view, T>> choices) {
            const auto name = text(value, source, location);
            for (const auto& [key, result] : choices)
                if (key == name)
                    return result;
            fail(source, location, "unsupported value '" + name + "'");
        }

        bool boolean(const json& value, const fs::path& source, const std::string& location) {
            if (!value.is_boolean())
                fail(source, location, "expected a boolean");
            return value.get<bool>();
        }

        std::string qualified(const json& value, const fs::path& source, const std::string& location) {
            const auto id = text(value, source, location);
            if (!is_shader_asset_id(id))
                fail(source, location, "expected a qualified lowercase namespace:name identifier");
            return id;
        }

        float number(const json& value, const fs::path& source, const std::string& location) {
            if (!value.is_number())
                fail(source, location, "expected a finite float");
            const auto result = value.get<double>();
            if (!std::isfinite(result) || std::abs(result) > std::numeric_limits<float>::max())
                fail(source, location, "number is outside the finite float range");
            return static_cast<float>(result);
        }

        ShaderLiteral literal(const json& value, const ParameterType type, const fs::path& source, const std::string& location) {
            if (type == ParameterType::Float)
                return number(value, source, location);
            if (type == ParameterType::Bool)
                return boolean(value, source, location);
            if (type == ParameterType::Int || type == ParameterType::UInt) {
                if (!value.is_number_integer())
                    fail(source, location, "expected an integer literal");
                if (type == ParameterType::Int) {
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
            const std::size_t count = type == ParameterType::Vec2 ? 2 : type == ParameterType::Vec3 ? 3 :
                                      type == ParameterType::Vec4 ? 4 : type == ParameterType::Mat4 ? 16 : 0;
            if (count == 0)
                fail(source, location, "sampler parameters cannot contain default images");
            if (!value.is_array() || value.size() != count)
                fail(source, location, "expected an array with " + std::to_string(count) + " components");
            std::array<float, 16> parts{};
            for (std::size_t i = 0; i < count; ++i)
                parts[i] = number(value[i], source, location + '[' + std::to_string(i) + ']');
            if (type == ParameterType::Vec2)
                return glm::vec2(parts[0], parts[1]);
            if (type == ParameterType::Vec3)
                return glm::vec3(parts[0], parts[1], parts[2]);
            if (type == ParameterType::Vec4)
                return glm::vec4(parts[0], parts[1], parts[2], parts[3]);
            glm::mat4 result(0);
            for (glm::length_t column = 0; column < 4; ++column)
                for (glm::length_t row = 0; row < 4; ++row)
                    result[column][row] = parts[static_cast<std::size_t>(column * 4 + row)];
            return result;
        }

        ShaderParameter parameter(const json& value, const fs::path& source, const std::string& location) {
            fields(value, source, location, {"key", "type", "required", "semantic", "default"});
            ShaderParameter result;
            result.key = text(required(value, "key", source, location), source, location + ".key");
            result.type = choice<ParameterType>(required(value, "type", source, location), source, location + ".type",
                {{"float", ParameterType::Float}, {"int", ParameterType::Int}, {"uint", ParameterType::UInt}, {"bool", ParameterType::Bool},
                 {"vec2", ParameterType::Vec2}, {"vec3", ParameterType::Vec3}, {"vec4", ParameterType::Vec4},
                 {"mat4", ParameterType::Mat4}, {"sampler2d", ParameterType::Sampler2D}});
            if (value.contains("required"))
                result.required = boolean(value.at("required"), source, location + ".required");
            if (value.contains("semantic"))
                result.semantic = choice<ParameterSemantic>(value.at("semantic"), source, location + ".semantic",
                    {{"custom", ParameterSemantic::Custom}, {"projection", ParameterSemantic::Projection}, {"view", ParameterSemantic::View},
                     {"model", ParameterSemantic::Model}, {"alpha", ParameterSemantic::Alpha}, {"scale", ParameterSemantic::Scale}});
            if (value.contains("default"))
                result.default_value = literal(value.at("default"), result.type, source, location + ".default");
            return result;
        }

        SamplerOptions sampling(const json& value, const fs::path& source, const std::string& location) {
            fields(value, source, location, {"minification", "magnification", "mipmaps", "wrap_u", "wrap_v", "anisotropy"});
            SamplerOptions result;
            const auto filter = [&](const std::string& key, ImageFilter& target) {
                if (value.contains(key))
                    target = choice<ImageFilter>(value.at(key), source, location + '.' + key,
                        {{"nearest", ImageFilter::Nearest}, {"linear", ImageFilter::Linear}});
            };
            filter("minification", result.minification);
            filter("magnification", result.magnification);
            if (value.contains("mipmaps"))
                result.mipmaps = choice<MipmapFilter>(value.at("mipmaps"), source, location + ".mipmaps",
                    {{"none", MipmapFilter::None}, {"nearest", MipmapFilter::Nearest}, {"linear", MipmapFilter::Linear}});
            const auto wrap = [&](const std::string& key, ImageWrap& target) {
                if (value.contains(key))
                    target = choice<ImageWrap>(value.at(key), source, location + '.' + key,
                        {{"clamp_to_edge", ImageWrap::ClampToEdge}, {"repeat", ImageWrap::Repeat}, {"mirrored_repeat", ImageWrap::MirroredRepeat}});
            };
            wrap("wrap_u", result.wrap_u);
            wrap("wrap_v", result.wrap_v);
            if (value.contains("anisotropy"))
                result.anisotropy = choice<ImageAnisotropy>(value.at("anisotropy"), source, location + ".anisotropy",
                    {{"disabled", ImageAnisotropy::Disabled}, {"maximum_supported", ImageAnisotropy::MaximumSupported}});
            return result;
        }

        ShaderMaterialRecipe material(const json& value, const std::string& id, const fs::path& source, const std::string& location) {
            fields(value, source, location, {"name", "program", "vertex_layout", "topology", "state", "parameters", "defaults", "sampling", "bindings"});
            ShaderMaterialRecipe result;
            result.id = id;
            result.program = qualified(required(value, "program", source, location), source, location + ".program");
            result.vertex_layout = choice<VertexLayout2D>(required(value, "vertex_layout", source, location), source, location + ".vertex_layout",
                {{"position3_uv2", VertexLayout2D::Position3UV2}, {"position3_uv2_color4", VertexLayout2D::Position3UV2Color4}});
            result.topology = choice<PrimitiveTopology>(required(value, "topology", source, location), source, location + ".topology",
                {{"triangles", PrimitiveTopology::Triangles}, {"triangle_strip", PrimitiveTopology::TriangleStrip}});
            if (value.contains("state")) {
                const auto& state = value.at("state");
                fields(state, source, location + ".state", {"blend", "depth", "depth_write", "cull"});
                if (state.contains("blend"))
                    result.state.blend = choice<BlendMode>(state.at("blend"), source, location + ".state.blend",
                        {{"opaque", BlendMode::Opaque}, {"straight_alpha", BlendMode::StraightAlpha},
                         {"premultiplied_alpha", BlendMode::PremultipliedAlpha}, {"additive", BlendMode::Additive}});
                if (state.contains("depth"))
                    result.state.depth = choice<DepthMode>(state.at("depth"), source, location + ".state.depth",
                        {{"disabled", DepthMode::Disabled}, {"less", DepthMode::Less}, {"less_equal", DepthMode::LessEqual}});
                if (state.contains("depth_write"))
                    result.state.depth_write = boolean(state.at("depth_write"), source, location + ".state.depth_write");
                if (state.contains("cull"))
                    result.state.cull = choice<CullMode>(state.at("cull"), source, location + ".state.cull",
                        {{"none", CullMode::None}, {"front", CullMode::Front}, {"back", CullMode::Back}});
            }
            const auto& parameters = required(value, "parameters", source, location);
            if (!parameters.is_array())
                fail(source, location + ".parameters", "expected an array");
            for (std::size_t i = 0; i < parameters.size(); ++i)
                result.parameters.push_back(parameter(parameters[i], source, location + ".parameters[" + std::to_string(i) + ']'));
            if (value.contains("defaults")) {
                object(value.at("defaults"), source, location + ".defaults");
                for (const auto& item : value.at("defaults").items()) {
                    const auto found = std::ranges::find(result.parameters, item.key(), &ShaderParameter::key);
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
            const ShaderProgramRecipe placeholder{result.program,
                {{ShaderStage::Vertex, "vertex", {}}, {ShaderStage::Fragment, "fragment", {}}}};
            try {
                validate_shader_material(result, placeholder);
            } catch (const std::exception& error) {
                fail(source, location, error.what());
            }
            return result;
        }
    }

    json ManifestDetail::read_document(std::istream& input, const fs::path& source) {
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

    ShaderAssetManifest ManifestDetail::parse_shader(const json& root, const fs::path& source) {
        fields(root, source, "$", {"$schema", "asset_class", "version", "namespace", "programs", "materials"});
        if (text(required(root, "asset_class", source, "$"), source, "$.asset_class") != "shader")
            fail(source, "$.asset_class", "expected shader asset class");
        if (text(required(root, "version", source, "$"), source, "$.version") != "1.0")
            fail(source, "$.version", "unsupported shader asset version");
        if (root.contains("$schema"))
            static_cast<void>(text(root.at("$schema"), source, "$.$schema"));
        ShaderAssetManifest result;
        result.source = source;
        result.name_space = text(required(root, "namespace", source, "$"), source, "$.namespace");
        if (!is_shader_asset_id(result.name_space + ":probe"))
            fail(source, "$.namespace", "expected a lowercase namespace");
        std::unordered_set<std::string> ids;
        const auto id_for = [&](const json& value, const std::string& location) {
            const auto id = result.name_space + ':' + text(required(value, "name", source, location), source, location + ".name");
            if (!is_shader_asset_id(id) || !ids.emplace(id).second)
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
            ShaderProgramRecipe recipe;
            recipe.id = id_for(value, location);
            const auto& stages = required(value, "stages", source, location);
            fields(stages, source, location + ".stages", {"vertex", "fragment"});
            for (const auto& [name, stage] : {std::pair{"vertex", ShaderStage::Vertex}, std::pair{"fragment", ShaderStage::Fragment}}) {
                const auto path = text(required(stages, name, source, location + ".stages"), source, location + ".stages." + name);
                const fs::path relative(path);
                if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory() ||
                    path.find('\\') != std::string::npos || path.find(':') != std::string::npos)
                    fail(source, location + ".stages." + name, "expected a relative path with forward slashes");
                recipe.sources.push_back({stage, (source.parent_path() / relative).lexically_normal(), {}});
            }
            validate_shader_program(recipe);
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

    ShaderAssetManifest ManifestLoader::load_shader(const fs::path& file) {
        std::ifstream input(file);
        if (!input)
            fail(file, "$", "unable to open definition");
        return parse_shader(input, file);
    }

    ShaderAssetManifest ManifestLoader::parse_shader(std::istream& input, const fs::path& source) {
        return ManifestDetail::parse_shader(ManifestDetail::read_document(input, source), source);
    }
}
