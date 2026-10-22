#include <core/game-framework/dialogue.h>

#include <string_view>
#include <unordered_set>
#include <utility>

namespace CE::Dialogue {
    CompileResult compile(const Draft& draft, const OperationKeys& operations) {
        std::vector<Diagnostic> errors;
        const auto report = [&](DiagnosticCode code, std::string field, std::string value, std::optional<std::size_t> node = {},
                                std::optional<std::size_t> choice = {}, std::optional<std::size_t> operation = {}) {
            errors.push_back({code, draft.id, draft.revision, node, choice, operation, std::move(field), std::move(value)});
        };
        if (draft.id.value.empty())
            report(DiagnosticCode::EmptyIdentity, "id", draft.id.value);
        if (draft.revision == 0)
            report(DiagnosticCode::InvalidRevision, "revision", "0");

        std::unordered_set<std::string_view> conditions;
        std::unordered_set<std::string_view> effects;
        const auto supported = [&](const auto& names, auto& index, const char* field) {
            for (std::size_t position = 0; position < names.size(); ++position) {
                const auto& name = names[position].value;
                if (name.empty())
                    report(DiagnosticCode::EmptyIdentity, field, name, {}, {}, position);
                else if (!index.insert(name).second)
                    report(DiagnosticCode::DuplicateIdentity, field, name, {}, {}, position);
            }
        };
        supported(operations.conditions, conditions, "supported_conditions");
        supported(operations.effects, effects, "supported_effects");

        std::unordered_set<std::string_view> all_nodes;
        std::size_t entry_index = 0;
        for (std::size_t index = 0; index < draft.nodes.size(); ++index) {
            const auto& id = draft.nodes[index].id.value;
            if (!id.empty())
                all_nodes.insert(id);
            if (id == draft.entry.value)
                entry_index = index;
        }
        if (draft.entry.value.empty())
            report(DiagnosticCode::EmptyIdentity, "entry", draft.entry.value);
        else if (!all_nodes.contains(draft.entry.value))
            report(DiagnosticCode::MissingReference, "entry", draft.entry.value);

        std::unordered_set<std::string_view> seen_nodes;
        for (std::size_t node_index = 0; node_index < draft.nodes.size(); ++node_index) {
            const auto& node = draft.nodes[node_index];
            if (node.id.value.empty())
                report(DiagnosticCode::EmptyIdentity, "id", node.id.value, node_index);
            else if (!seen_nodes.insert(node.id.value).second)
                report(DiagnosticCode::DuplicateIdentity, "id", node.id.value, node_index);

            if (node.kind != NodeKind::Prompt && node.kind != NodeKind::Terminal)
                report(DiagnosticCode::InvalidNodeShape, "kind", std::to_string(static_cast<int>(node.kind)), node_index);
            else if ((node.kind == NodeKind::Prompt && node.choices.empty()) || (node.kind == NodeKind::Terminal && !node.choices.empty()))
                report(DiagnosticCode::InvalidNodeShape, "choices", std::to_string(node.choices.size()), node_index);

            std::unordered_set<std::string_view> seen_choices;
            for (std::size_t choice_index = 0; choice_index < node.choices.size(); ++choice_index) {
                const auto& choice = node.choices[choice_index];
                if (choice.id.value.empty())
                    report(DiagnosticCode::EmptyIdentity, "id", choice.id.value, node_index, choice_index);
                else if (!seen_choices.insert(choice.id.value).second)
                    report(DiagnosticCode::DuplicateIdentity, "id", choice.id.value, node_index, choice_index);
                if (choice.target.value.empty())
                    report(DiagnosticCode::EmptyIdentity, "target", choice.target.value, node_index, choice_index);
                else if (!all_nodes.contains(choice.target.value))
                    report(DiagnosticCode::MissingReference, "target", choice.target.value, node_index, choice_index);
                if (choice.condition) {
                    if (choice.condition->value.empty())
                        report(DiagnosticCode::EmptyIdentity, "condition", choice.condition->value, node_index, choice_index);
                    else if (!conditions.contains(choice.condition->value))
                        report(DiagnosticCode::UnsupportedOperation, "condition", choice.condition->value, node_index, choice_index);
                }
                if (choice.effect) {
                    if (choice.effect->value.empty())
                        report(DiagnosticCode::EmptyIdentity, "effect", choice.effect->value, node_index, choice_index);
                    else if (!effects.contains(choice.effect->value))
                        report(DiagnosticCode::UnsupportedOperation, "effect", choice.effect->value, node_index, choice_index);
                }
            }
        }
        if (!errors.empty())
            return std::unexpected(std::move(errors));

        // Copy instead of adopting vectors/strings: pointers into a moved draft must
        // never become mutable aliases into the published definition.
        return DefinitionHandle(new const Definition(draft, entry_index));
    }

    Definition::Definition(const Draft& draft, const std::size_t entry_index)
    : data_(draft), entry_index_(entry_index) {}

    const Node* Definition::find_node(const NodeId& id) const noexcept {
        for (const auto& node : data_.nodes) {
            if (node.id == id)
                return &node;
        }
        return nullptr;
    }

    const Choice* Definition::find_choice(const NodeId& node, const ChoiceId& id) const noexcept {
        if (const auto found = find_node(node)) {
            for (const auto& choice : found->choices) {
                if (choice.id == id)
                    return &choice;
            }
        }
        return nullptr;
    }
}
