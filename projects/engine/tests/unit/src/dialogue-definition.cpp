#include <cheryl/core/game-framework/dialogue.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <type_traits>
#include <utility>

namespace {
    namespace D = CE::Dialogue;

    D::Draft gate() {
        return {
            .id = {"gate-encounter"},
            .revision = 1,
            .entry = {"approach"},
            .nodes = {
                {{"approach"},
                 "A guard waits.",
                 D::NodeKind::Prompt,
                 {{{"introduce"}, "Introduce yourself", {"gate"}, {}, D::EffectKey{"introduce"}},
                  {{"silent"}, "Stay silent", {"gate"}, {}, {}}}},
                {{"gate"},
                 "The gate is closed.",
                 D::NodeKind::Prompt,
                 {{{"pass"}, "Ask to enter", {"admitted"}, D::ConditionKey{"introduced"}, {}}, {{"leave"}, "Leave", {"departed"}, {}, {}}}},
                {{"admitted"}, "You enter", D::NodeKind::Terminal, {}},
                {{"departed"}, "You depart", D::NodeKind::Terminal, {}}
            }
        };
    }

    D::OperationKeys operations() {
        return {{{"introduced"}}, {{"introduce"}}};
    }

    void expect_error(
        const D::CompileResult& result,
        D::DiagnosticCode code,
        const std::string& field,
        const std::string& value,
        std::optional<std::size_t> node = {},
        std::optional<std::size_t> choice = {},
        std::optional<std::size_t> operation = {}
    ) {
        ASSERT_FALSE(result.has_value());
        ASSERT_EQ(result.error().size(), 1u);
        const auto& error = result.error().front();
        EXPECT_EQ(error.code, code);
        EXPECT_EQ(error.field, field);
        EXPECT_EQ(error.value, value);
        EXPECT_EQ(error.node_index, node);
        EXPECT_EQ(error.choice_index, choice);
        EXPECT_EQ(error.operation_index, operation);
    }
}

TEST(dialogue_definition, consumer) {
    static_assert(!std::is_default_constructible_v<D::Definition>);
    static_assert(!std::is_constructible_v<D::Definition, const D::Draft&, std::size_t>);
    static_assert(!std::is_copy_constructible_v<D::Definition>);
    static_assert(!std::is_move_constructible_v<D::Definition>);
    static_assert(!std::is_copy_assignable_v<D::Definition>);
    static_assert(!std::is_move_assignable_v<D::Definition>);
    static_assert(!std::is_aggregate_v<D::Definition>);
    static_assert(std::is_const_v<D::DefinitionHandle::element_type>);
    static_assert(std::is_same_v<decltype(std::declval<const D::Definition&>().nodes()[0]), const D::Node&>);
    static_assert(!std::is_constructible_v<D::NodeId, D::DefinitionId>);
    static_assert(!std::is_constructible_v<D::ChoiceId, D::NodeId>);
    static_assert(!std::is_constructible_v<D::ConditionKey, D::ChoiceId>);
    static_assert(!std::is_constructible_v<D::EffectKey, D::ConditionKey>);

    auto result = D::compile(gate(), operations());
    ASSERT_TRUE(result.has_value());
    const D::DefinitionHandle definition = *result;
    ASSERT_NE(definition, nullptr);
    EXPECT_EQ(definition->id(), D::DefinitionId{"gate-encounter"});
    EXPECT_EQ(definition->revision(), 1u);
    EXPECT_EQ(definition->entry().id, D::NodeId{"approach"});
    ASSERT_EQ(definition->nodes().size(), 4u);
    EXPECT_EQ(definition->nodes()[2].id, D::NodeId{"admitted"});
    const auto* choice = definition->find_choice({"gate"}, {"pass"});
    ASSERT_NE(choice, nullptr);
    EXPECT_EQ(choice->text, "Ask to enter");
    EXPECT_EQ(choice->target, D::NodeId{"admitted"});
    EXPECT_EQ(choice->condition, (D::ConditionKey{"introduced"}));
    EXPECT_FALSE(choice->effect.has_value());
}

TEST(dialogue_definition, ownership) {
    D::DefinitionHandle retained;
    std::weak_ptr<const D::Definition> lifetime;
    {
        auto draft = gate();
        auto keys = operations();
        auto* original_node = &draft.nodes[0];
        auto* original_choice = &draft.nodes[0].choices[0];
        auto result = D::compile(std::move(draft), keys);
        ASSERT_TRUE(result.has_value());
        retained = *result;
        lifetime = retained;
        EXPECT_NE(&retained->nodes()[0], original_node);
        EXPECT_NE(&retained->nodes()[0].choices[0], original_choice);

        // These aliases remain valid in the draft even though compile received std::move.
        original_node->text = "Changed source text";
        original_choice->effect->value = "changed-effect";
        std::swap(original_node->choices[0], original_node->choices[1]);
        draft.id.value = "changed-definition";
        draft.revision = 99;
        draft.entry.value = "departed";
        draft.nodes[1].id.value = "changed-gate";
        keys.conditions[0].value = "changed-condition";
        draft.nodes.clear();
    }
    ASSERT_NE(retained, nullptr);
    EXPECT_EQ(retained->id().value, "gate-encounter");
    EXPECT_EQ(retained->revision(), 1u);
    EXPECT_EQ(retained->entry().text, "A guard waits.");
    ASSERT_EQ(retained->entry().choices.size(), 2u);
    EXPECT_EQ(retained->entry().choices[0].id.value, "introduce");
    EXPECT_EQ(retained->entry().choices[1].id.value, "silent");
    ASSERT_TRUE(retained->entry().choices[0].effect.has_value());
    EXPECT_EQ(retained->entry().choices[0].effect->value, "introduce");
    const auto* gate_node = retained->find_node({"gate"});
    ASSERT_NE(gate_node, nullptr);
    ASSERT_TRUE(gate_node->choices[0].condition.has_value());
    EXPECT_EQ(gate_node->choices[0].condition->value, "introduced");

    auto editable_copy = *gate_node;
    editable_copy.text = "Edited copy";
    editable_copy.choices.clear();
    EXPECT_EQ(gate_node->text, "The gate is closed.");
    EXPECT_EQ(gate_node->choices.size(), 2u);
    auto second_owner = retained;
    retained.reset();
    EXPECT_FALSE(lifetime.expired());
    EXPECT_EQ(gate_node->id.value, "gate");
    second_owner.reset();
    EXPECT_TRUE(lifetime.expired()); // Borrowed node access has ended.
}

TEST(dialogue_definition, lookup) {
    const auto result = D::compile(gate(), operations());
    ASSERT_TRUE(result.has_value());
    const auto& definition = *result;
    EXPECT_EQ(definition->find_node({"gate"}), &definition->nodes()[1]);
    EXPECT_EQ(definition->find_choice({"gate"}, {"leave"}), &definition->nodes()[1].choices[1]);
    EXPECT_EQ(definition->find_node({"Gate"}), nullptr);
    EXPECT_EQ(definition->find_node({"missing"}), nullptr);
    EXPECT_EQ(definition->find_node({}), nullptr);
    EXPECT_EQ(definition->find_choice({"missing"}, {"pass"}), nullptr);
    EXPECT_EQ(definition->find_choice({"gate"}, {"Pass"}), nullptr);
    EXPECT_EQ(definition->find_choice({"admitted"}, {"pass"}), nullptr);
    EXPECT_EQ(definition->find_choice({"gate"}, {"introduce"}), nullptr);
    EXPECT_EQ(definition->nodes().size(), 4u);
}

TEST(dialogue_definition, empty_ids) {
    auto draft = gate();
    draft.id.value.clear();
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::EmptyIdentity, "id", "");
    draft = gate();
    draft.entry.value.clear();
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::EmptyIdentity, "entry", "");
    draft = gate();
    draft.nodes[3].id.value.clear();
    draft.nodes[1].choices[1].target = {"admitted"};
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::EmptyIdentity, "id", "", 3);
    draft = gate();
    draft.nodes[0].choices[0].id.value.clear();
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::EmptyIdentity, "id", "", 0, 0);
    draft = gate();
    draft.nodes[0].choices[0].target.value.clear();
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::EmptyIdentity, "target", "", 0, 0);
}

TEST(dialogue_definition, revision) {
    auto draft = gate();
    draft.revision = 0;
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::InvalidRevision, "revision", "0");
}

TEST(dialogue_definition, duplicate_nodes) {
    auto draft = gate();
    draft.nodes.push_back(draft.nodes[2]);
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::DuplicateIdentity, "id", "admitted", 4);
}

TEST(dialogue_definition, duplicate_choices) {
    auto draft = gate();
    draft.nodes[1].choices.push_back(draft.nodes[1].choices[0]);
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::DuplicateIdentity, "id", "pass", 1, 2);
}

TEST(dialogue_definition, missing_references) {
    auto draft = gate();
    draft.entry = {"absent-entry"};
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::MissingReference, "entry", "absent-entry");
    draft = gate();
    draft.nodes[1].choices[0].target = {"admittted"};
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::MissingReference, "target", "admittted", 1, 0);
    draft.nodes.clear();
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::MissingReference, "entry", "approach");
}

TEST(dialogue_definition, unsupported_keys) {
    auto draft = gate();
    draft.nodes[1].choices[0].condition = D::ConditionKey{"introduce"}; // Supported only as an effect.
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::UnsupportedOperation, "condition", "introduce", 1, 0);
    draft = gate();
    draft.nodes[0].choices[0].effect = D::EffectKey{"introduced"}; // Supported only as a condition.
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::UnsupportedOperation, "effect", "introduced", 0, 0);
    draft = gate();
    draft.nodes[1].choices[0].condition = D::ConditionKey{};
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::EmptyIdentity, "condition", "", 1, 0);
    draft = gate();
    draft.nodes[0].choices[0].effect = D::EffectKey{};
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::EmptyIdentity, "effect", "", 0, 0);
}

TEST(dialogue_definition, supported_names) {
    auto keys = operations();
    keys.conditions.push_back(keys.conditions[0]);
    expect_error(D::compile(gate(), keys), D::DiagnosticCode::DuplicateIdentity, "supported_conditions", "introduced", {}, {}, 1);
    keys = operations();
    keys.effects.push_back(keys.effects[0]);
    expect_error(D::compile(gate(), keys), D::DiagnosticCode::DuplicateIdentity, "supported_effects", "introduce", {}, {}, 1);
    keys = operations();
    keys.conditions.push_back({});
    expect_error(D::compile(gate(), keys), D::DiagnosticCode::EmptyIdentity, "supported_conditions", "", {}, {}, 1);
    keys = operations();
    keys.effects.push_back({});
    expect_error(D::compile(gate(), keys), D::DiagnosticCode::EmptyIdentity, "supported_effects", "", {}, {}, 1);
}

TEST(dialogue_definition, node_shape) {
    auto draft = gate();
    draft.nodes[1].choices.clear();
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::InvalidNodeShape, "choices", "0", 1);
    draft = gate();
    draft.nodes[1].kind = D::NodeKind::Terminal;
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::InvalidNodeShape, "choices", "2", 1);
    draft = gate();
    draft.nodes[2].kind = static_cast<D::NodeKind>(99);
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::InvalidNodeShape, "kind", "99", 2);
}

TEST(dialogue_definition, independent_domains) {
    auto draft = gate();
    draft.id.value = "gate";                 // Definition and node names occupy different domains.
    draft.nodes[0].choices[0].id = {"pass"}; // Same choice name in a different node.
    draft.nodes[0].choices[0].effect = D::EffectKey{"same"};
    draft.nodes[1].choices[0].condition = D::ConditionKey{"same"};
    const D::OperationKeys keys{{{"same"}}, {{"same"}}};
    const auto first = D::compile(draft, keys);
    const auto second = D::compile(draft, keys); // No process-global definition registry.
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_NE(first->get(), second->get());
    EXPECT_NE((*first)->find_choice({"approach"}, {"pass"}), nullptr);
    EXPECT_NE((*first)->find_choice({"gate"}, {"pass"}), nullptr);
}

TEST(dialogue_definition, cycles) {
    auto draft = gate();
    draft.nodes[1].choices[1].target = {"approach"};
    draft.nodes[0].choices[1].target = {"approach"};
    draft.nodes[2].text.clear(); // Empty text and unreachable valid nodes are permitted.
    const auto result = D::compile(draft, operations());
    ASSERT_TRUE(result.has_value());
    const auto* choice = (*result)->find_choice({"gate"}, {"leave"});
    ASSERT_NE(choice, nullptr);
    EXPECT_EQ(choice->target.value, "approach");
}

TEST(dialogue_definition, entry_order) {
    auto draft = gate();
    draft.entry = {"gate"};
    const auto result = D::compile(draft, operations());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(&(*result)->entry(), &(*result)->nodes()[1]);
    EXPECT_EQ((*result)->nodes()[0].id.value, "approach");
    draft.entry = {"admitted"};
    const auto terminal = D::compile(draft, operations());
    ASSERT_TRUE(terminal.has_value());
    EXPECT_EQ((*terminal)->entry().kind, D::NodeKind::Terminal);
}

TEST(dialogue_definition, unreachable) {
    auto draft = gate();
    draft.nodes[0].choices[0].target = {"admitted"};
    draft.nodes[0].choices[1].target = {"departed"};
    draft.nodes[1].choices[0].target = {"unreachable-bad-target"};
    expect_error(D::compile(draft, operations()), D::DiagnosticCode::MissingReference, "target", "unreachable-bad-target", 1, 0);
}

TEST(dialogue_definition, diagnostics) {
    std::vector<D::Diagnostic> retained;
    {
        auto draft = gate();
        auto keys = operations();
        draft.id = {"broken-definition"};
        draft.revision = 0;
        keys.conditions.push_back({"introduced"});
        keys.effects.push_back({});
        draft.entry = {"absent"};
        draft.nodes[0].choices[0].target = {"nowhere"};
        draft.nodes[1].choices[1].id = {"pass"};
        draft.nodes[3].kind = D::NodeKind::Prompt;
        const auto first = D::compile(draft, keys);
        const auto second = D::compile(draft, keys);
        ASSERT_FALSE(first.has_value()); // The error variant has no partial definition handle.
        ASSERT_FALSE(second.has_value());
        EXPECT_EQ(first.error(), second.error());
        retained = first.error();
        draft.nodes.clear();
        draft.id.value.clear();
        keys.conditions.clear();
    }
    const std::vector<D::Diagnostic> expected{
        {D::DiagnosticCode::InvalidRevision, {"broken-definition"}, 0, {}, {}, {}, "revision", "0"},
        {D::DiagnosticCode::DuplicateIdentity, {"broken-definition"}, 0, {}, {}, 1, "supported_conditions", "introduced"},
        {D::DiagnosticCode::EmptyIdentity, {"broken-definition"}, 0, {}, {}, 1, "supported_effects", ""},
        {D::DiagnosticCode::MissingReference, {"broken-definition"}, 0, {}, {}, {}, "entry", "absent"},
        {D::DiagnosticCode::MissingReference, {"broken-definition"}, 0, 0, 0, {}, "target", "nowhere"},
        {D::DiagnosticCode::DuplicateIdentity, {"broken-definition"}, 0, 1, 1, {}, "id", "pass"},
        {D::DiagnosticCode::InvalidNodeShape, {"broken-definition"}, 0, 3, {}, {}, "choices", "0"}
    };
    EXPECT_EQ(retained, expected);
}
