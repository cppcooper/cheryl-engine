#include <core/game-framework/dialogue.h>

#include <algorithm>
#include <numeric>
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
    : data_(draft), entry_index_(entry_index), node_lookup_(data_.nodes.size()), choice_lookup_(data_.nodes.size()) {
        // Index only our validated deep copy. Sorting ordinals leaves all authored
        // storage/order intact; no string views, caller aliases or lazy cache survive.
        std::iota(node_lookup_.begin(), node_lookup_.end(), std::size_t{0});
        std::sort(node_lookup_.begin(), node_lookup_.end(), [&](auto left, auto right) {
            return data_.nodes[left].id.value < data_.nodes[right].id.value;
        });
        for (std::size_t node = 0; node < data_.nodes.size(); ++node) {
            const auto& choices = data_.nodes[node].choices;
            auto& lookup = choice_lookup_[node];
            lookup.resize(choices.size());
            std::iota(lookup.begin(), lookup.end(), std::size_t{0});
            std::sort(lookup.begin(), lookup.end(), [&](auto left, auto right) {
                return choices[left].id.value < choices[right].id.value;
            });
        }
    }

    const Node* Definition::find_node(const NodeId& id) const noexcept {
        const auto found = std::lower_bound(node_lookup_.begin(), node_lookup_.end(), id.value, [&](auto ordinal, const auto& value) {
            return data_.nodes[ordinal].id.value < value;
        });
        if (found == node_lookup_.end() || data_.nodes[*found].id != id)
            return nullptr;
        return &data_.nodes[*found];
    }

    const Choice* Definition::find_choice(const NodeId& node, const ChoiceId& id) const noexcept {
        const auto found = find_node(node);
        if (!found)
            return nullptr;
        const auto& lookup = choice_lookup_[static_cast<std::size_t>(found - data_.nodes.data())];
        const auto choice = std::lower_bound(lookup.begin(), lookup.end(), id.value, [&](auto ordinal, const auto& value) {
            return found->choices[ordinal].id.value < value;
        });
        if (choice == lookup.end() || found->choices[*choice].id != id)
            return nullptr;
        return &found->choices[*choice];
    }
}
