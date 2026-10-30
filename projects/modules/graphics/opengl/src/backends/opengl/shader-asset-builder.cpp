#include "gl-internal.h"
#include <backends/opengl/resource-provider.h>

#include "program-builder.h"

#include <backends/opengl/renderer.h>
#include <internals/exceptions.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace CE::Rendering::OpenGL {
    namespace {
        using json = nlohmann::json;

        [[noreturn]] void fail(const std::string& id, const std::string& message) {
            throw Exceptions::invalid_args(CE_HERE, "OpenGL shader asset '" + id + "': " + message);
        }

        void fields(const json& object, const std::string& id, const std::initializer_list<std::string_view> allowed) {
            if (!object.is_object())
                fail(id, "expected a bindings object");
            for (const auto& item : object.items())
                if (std::ranges::find(allowed, item.key()) == allowed.end())
                    fail(id, "unknown binding property '" + item.key() + "'");
        }

        std::string name(const json& value, const std::string& id) {
            if (!value.is_string() || value.get_ref<const std::string&>().empty())
                fail(id, "binding names must be nonempty strings");
            return value.get<std::string>();
        }

        PipelineBindings decode_bindings(const CE::Assets::Shaders::MaterialRecipe& recipe) {
            const auto selected = recipe.bindings.find("opengl");
            if (selected == recipe.bindings.end())
                fail(recipe.id, "missing OpenGL bindings");
            try {
                std::vector<std::unordered_set<std::string>> keys;
                const auto payload = json::parse(selected->second, [&](const int, const json::parse_event_t event, json& parsed) {
                    if (event == json::parse_event_t::object_start)
                        keys.emplace_back();
                    else if (event == json::parse_event_t::object_end)
                        keys.pop_back();
                    else if (event == json::parse_event_t::key && !keys.back().insert(parsed.get<std::string>()).second)
                        fail(recipe.id, "duplicate binding property");
                    return true;
                });
                fields(payload, recipe.id, {"attributes", "parameters"});
                PipelineBindings result;
                if (payload.contains("attributes")) {
                    const auto& attributes = payload.at("attributes");
                    fields(attributes, recipe.id, {"position", "texcoord", "color"});
                    if (attributes.contains("position"))
                        result.position_attribute = name(attributes.at("position"), recipe.id);
                    if (attributes.contains("texcoord"))
                        result.uv_attribute = name(attributes.at("texcoord"), recipe.id);
                    if (attributes.contains("color"))
                        result.color_attribute = name(attributes.at("color"), recipe.id);
                }
                if (!payload.contains("parameters") || !payload.at("parameters").is_object())
                    fail(recipe.id, "missing parameter bindings object");
                std::unordered_set<std::string> uniforms;
                for (const auto& item : payload.at("parameters").items()) {
                    if (std::ranges::find(recipe.parameters, item.key(), &CE::Assets::Shaders::ParameterRecipe::key) ==
                        recipe.parameters.end())
                        fail(recipe.id, "binding is outside the parameter contract: '" + item.key() + "'");
                    auto uniform = name(item.value(), recipe.id);
                    if (!uniforms.emplace(uniform).second)
                        fail(recipe.id, "duplicate uniform mapping '" + uniform + "'");
                    result.parameters.push_back({item.key(), std::move(uniform), {}});
                }
                if (result.parameters.size() != recipe.parameters.size())
                    fail(recipe.id, "every contract parameter requires a uniform mapping");
                return result;
            } catch (const json::exception& error) {
                fail(recipe.id, "invalid bindings JSON: " + std::string(error.what()));
            }
        }
    }

    void ResourceProvider::validate_program(const CE::Assets::Shaders::ProgramRecipe& recipe) const {
        CE::Assets::Shaders::validate_program(recipe);
        for (const auto& source : recipe.sources)
            if (source.bytes.empty() || source.bytes.size() > static_cast<std::size_t>(std::numeric_limits<GLint>::max()))
                fail(recipe.id, "prepared source is empty or exceeds OpenGL's length range: '" + source.path.string() + "'");
    }

    void ResourceProvider::validate_material(
        const CE::Assets::Shaders::MaterialRecipe& recipe,
        const CE::Assets::Shaders::ProgramRecipe& program
    ) const {
        CE::Assets::Shaders::validate_material(recipe, program);
        static_cast<void>(decode_bindings(recipe));
    }

    std::shared_ptr<CE::Rendering::Resources::Program> ResourceProvider::build_program(const CE::Assets::Shaders::ProgramRecipe& recipe) {
        validate_program(recipe);
        return Detail::link_owned_program(renderer_.resources(), recipe);
    }

    std::shared_ptr<const CE::Rendering::Resources::Material> ResourceProvider::build_material(
        const CE::Assets::Shaders::MaterialRecipe& recipe,
        const CE::Assets::Shaders::ProgramRecipe& program,
        const std::shared_ptr<CE::Rendering::Resources::Program>& executable
    ) {
        validate_material(recipe, program);
        auto native = std::dynamic_pointer_cast<Program>(executable);
        if (!native || native->resource_domain() != renderer_.resources().get())
            fail(recipe.id, "selected program belongs to another backend/provider domain");
        CE::Rendering::Materials::Definition definition;
        definition.pipeline = std::make_shared<Pipeline>(
            CE::Assets::Shaders::pipeline_definition(recipe, program), std::move(native), decode_bindings(recipe)
        );
        definition.defaults = CE::Assets::Shaders::defaults(recipe);
        for (const auto& [key, options] : recipe.sampling)
            definition.sampling.emplace(key, create_sampler(options));
        return build_material(std::move(definition));
    }
}
