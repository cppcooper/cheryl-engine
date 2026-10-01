#include <assets/resources/parameters.h>

#include <internals/exceptions.h>

#include <algorithm>
#include <initializer_list>
#include <set>
#include <type_traits>

namespace CE::Assets {
    namespace {
        void validate_value(
            const ParameterDefinition& definition,
            const ParameterValue& value
        ) {
            if (parameter_type(value) != definition.type)
                throw Exceptions::invalid_args(CE_HERE, "Parameter type mismatch: " + definition.key);
            if (const auto* binding = std::get_if<ImageBinding>(&value); binding && !binding->image)
                throw Exceptions::invalid_args(CE_HERE, "Sampler has no image: " + definition.key);
        }

        ParameterValue semantic_value(
            const ParameterSemantic semantic,
            const ShaderPass& pass,
            const ShaderDraw& draw
        ) {
            switch (semantic) {
                case ParameterSemantic::Projection: return pass.projection;
                case ParameterSemantic::View: return pass.view;
                case ParameterSemantic::Model: return draw.model;
                case ParameterSemantic::Alpha: return draw.alpha;
                case ParameterSemantic::Scale: return draw.scale;
                default: throw Exceptions::invalid_args(CE_HERE, "Unknown engine parameter semantic");
            }
        }
    }

    ParameterType parameter_type(const ParameterValue& value) {
        return std::visit([](const auto& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, float>) return ParameterType::Float;
            else if constexpr (std::is_same_v<T, int>) return ParameterType::Int;
            else if constexpr (std::is_same_v<T, unsigned int>) return ParameterType::UInt;
            else if constexpr (std::is_same_v<T, bool>) return ParameterType::Bool;
            else if constexpr (std::is_same_v<T, glm::vec2>) return ParameterType::Vec2;
            else if constexpr (std::is_same_v<T, glm::vec3>) return ParameterType::Vec3;
            else if constexpr (std::is_same_v<T, glm::vec4>) return ParameterType::Vec4;
            else if constexpr (std::is_same_v<T, glm::mat4>) return ParameterType::Mat4;
            else return ParameterType::Sampler2D;
        }, value);
    }

    void validate_parameter_contract(const ParameterContract& contract) {
        std::set<std::string> keys;
        std::set<ParameterSemantic> semantics;
        for (const auto& definition : contract) {
            if (definition.key.empty() || !keys.insert(definition.key).second)
                throw Exceptions::invalid_args(CE_HERE, "Parameter keys must be nonempty and unique");
            if (definition.type < ParameterType::Float || definition.type > ParameterType::Sampler2D)
                throw Exceptions::invalid_args(CE_HERE, "Unknown parameter type: " + definition.key);
            if (definition.semantic != ParameterSemantic::Custom) {
                const auto expected = parameter_type(semantic_value(definition.semantic, {}, {}));
                if (definition.type != expected || !semantics.insert(definition.semantic).second || definition.default_value)
                    throw Exceptions::invalid_args(CE_HERE, "Invalid or duplicate engine parameter: " + definition.key);
            }
            if (definition.default_value)
                validate_value(definition, *definition.default_value);
        }
    }

    void validate_parameter_values(
        const ParameterContract& contract,
        const ParameterSet& values
    ) {
        validate_parameter_contract(contract);
        for (const auto& [key, value] : values) {
            const auto definition = std::find_if(contract.begin(), contract.end(), [&](const auto& item) { return item.key == key; });
            if (definition == contract.end())
                throw Exceptions::invalid_args(CE_HERE, "Unknown parameter: " + key);
            if (definition->semantic != ParameterSemantic::Custom)
                throw Exceptions::invalid_args(CE_HERE, "Engine parameter cannot be overridden: " + key);
            validate_value(*definition, value);
        }
    }

    void validate_resolved_parameters(
        const ParameterContract& contract,
        const ParameterSet& values
    ) {
        validate_parameter_contract(contract);
        std::set<std::uint32_t> units;
        for (const auto& [key, value] : values) {
            const auto definition = std::find_if(contract.begin(), contract.end(), [&](const auto& item) { return item.key == key; });
            if (definition == contract.end())
                throw Exceptions::invalid_args(CE_HERE, "Unknown resolved parameter: " + key);
            validate_value(*definition, value);
            if (const auto* binding = std::get_if<ImageBinding>(&value); binding && !units.insert(binding->unit).second)
                throw Exceptions::invalid_args(CE_HERE, "Sampler units must be distinct: " + key);
        }
        for (const auto& definition : contract) {
            if (definition.required && !values.contains(definition.key))
                throw Exceptions::invalid_args(CE_HERE, "Required parameter is missing: " + definition.key);
        }
    }

    ParameterSet resolve_parameters(
        const ParameterContract& contract,
        const ShaderPass& pass_semantics,
        const ShaderDraw& draw_semantics,
        const ParameterSet& pass_values,
        const ParameterSet& material_values,
        const ParameterSet& draw_values
    ) {
        validate_parameter_contract(contract);
        // Validate each source even if a later override would hide a bad value.
        validate_parameter_values(contract, pass_values);
        validate_parameter_values(contract, material_values);
        validate_parameter_values(contract, draw_values);
        ParameterSet resolved;
        for (const auto& definition : contract) {
            if (definition.semantic != ParameterSemantic::Custom)
                resolved.emplace(definition.key, semantic_value(definition.semantic, pass_semantics, draw_semantics));
            else if (definition.default_value)
                resolved.emplace(definition.key, *definition.default_value);
        }
        for (const auto* layer : {&pass_values, &material_values, &draw_values}) {
            for (const auto& [key, value] : *layer)
                resolved.insert_or_assign(key, value);
        }
        validate_resolved_parameters(contract, resolved);
        return resolved;
    }
}
