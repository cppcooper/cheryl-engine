#include <assets/resources/parameters.h>

#include <internals/exceptions.h>

#include <algorithm>
#include <initializer_list>
#include <set>
#include <type_traits>

namespace CE::Rendering::Parameters {
    namespace {
        void validate_value(const Definition& definition, const Value& value) {
            if (type_of(value) != definition.type)
                throw Exceptions::invalid_args(CE_HERE, "Parameter type mismatch: " + definition.key);
            if (const auto* binding = std::get_if<ImageBinding>(&value); binding && !binding->image)
                throw Exceptions::invalid_args(CE_HERE, "Sampler has no image: " + definition.key);
        }

        Value semantic_value(const Semantic semantic, const PassSemantics& pass, const DrawSemantics& draw) {
            switch (semantic) {
                case Semantic::Projection:
                    return pass.projection;
                case Semantic::View:
                    return pass.view;
                case Semantic::Model:
                    return draw.model;
                case Semantic::Alpha:
                    return draw.alpha;
                case Semantic::Scale:
                    return draw.scale;
                default:
                    throw Exceptions::invalid_args(CE_HERE, "Unknown engine parameter semantic");
            }
        }
    }

    Type type_of(const Value& value) {
        return std::visit(
            [](const auto& item) {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, float>)
                    return Type::Float;
                else if constexpr (std::is_same_v<T, int>)
                    return Type::Int;
                else if constexpr (std::is_same_v<T, unsigned int>)
                    return Type::UInt;
                else if constexpr (std::is_same_v<T, bool>)
                    return Type::Bool;
                else if constexpr (std::is_same_v<T, glm::vec2>)
                    return Type::Vec2;
                else if constexpr (std::is_same_v<T, glm::vec3>)
                    return Type::Vec3;
                else if constexpr (std::is_same_v<T, glm::vec4>)
                    return Type::Vec4;
                else if constexpr (std::is_same_v<T, glm::mat4>)
                    return Type::Mat4;
                else
                    return Type::Sampler2D;
            },
            value
        );
    }

    void validate_contract(const Contract& contract) {
        std::set<std::string> keys;
        std::set<Semantic> semantics;
        for (const auto& definition : contract) {
            if (definition.key.empty() || !keys.insert(definition.key).second)
                throw Exceptions::invalid_args(CE_HERE, "Parameter keys must be nonempty and unique");
            if (definition.type < Type::Float || definition.type > Type::Sampler2D)
                throw Exceptions::invalid_args(CE_HERE, "Unknown parameter type: " + definition.key);
            if (definition.semantic != Semantic::Custom) {
                const auto expected = type_of(semantic_value(definition.semantic, {}, {}));
                if (definition.type != expected || !semantics.insert(definition.semantic).second || definition.default_value)
                    throw Exceptions::invalid_args(CE_HERE, "Invalid or duplicate engine parameter: " + definition.key);
            }
            if (definition.default_value)
                validate_value(definition, *definition.default_value);
        }
    }

    void validate_values(const Contract& contract, const Set& values) {
        validate_contract(contract);
        for (const auto& [key, value] : values) {
            const auto definition = std::find_if(contract.begin(), contract.end(), [&](const auto& item) { return item.key == key; });
            if (definition == contract.end())
                throw Exceptions::invalid_args(CE_HERE, "Unknown parameter: " + key);
            if (definition->semantic != Semantic::Custom)
                throw Exceptions::invalid_args(CE_HERE, "Engine parameter cannot be overridden: " + key);
            validate_value(*definition, value);
        }
    }

    void validate_resolved(const Contract& contract, const Set& values) {
        validate_contract(contract);
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

    Set resolve(
        const Contract& contract,
        const PassSemantics& pass_semantics,
        const DrawSemantics& draw_semantics,
        const Set& pass_values,
        const Set& material_values,
        const Set& draw_values
    ) {
        validate_contract(contract);
        // Validate each source even if a later override would hide a bad value.
        validate_values(contract, pass_values);
        validate_values(contract, material_values);
        validate_values(contract, draw_values);
        Set resolved;
        for (const auto& definition : contract) {
            if (definition.semantic != Semantic::Custom)
                resolved.emplace(definition.key, semantic_value(definition.semantic, pass_semantics, draw_semantics));
            else if (definition.default_value)
                resolved.emplace(definition.key, *definition.default_value);
        }
        for (const auto* layer : {&pass_values, &material_values, &draw_values}) {
            for (const auto& [key, value] : *layer)
                resolved.insert_or_assign(key, value);
        }
        validate_resolved(contract, resolved);
        return resolved;
    }
}
