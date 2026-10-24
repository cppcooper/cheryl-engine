#include <cheryl/core/game-framework/dialogue.h>

#include <gtest/gtest.h>

#include <memory>
#include <utility>

namespace {
    namespace D = CE::Dialogue;
    std::string numbered(std::string prefix, std::size_t value) {
        return prefix + (value < 10 ? "0" : "") + std::to_string(value);
    }
    D::Draft shuffled() {
        D::Draft draft{{"lookup"}, 7, {"node13"}, {}};
        for (std::size_t row = 0; row < 17; ++row) {
            const auto value = (7 * row + 9) % 17;
            D::Node node{{numbered("node", value)}, "node text " + std::to_string(value), D::NodeKind::Terminal, {}};
            if (value != 0) {
                node.kind = D::NodeKind::Prompt;
                for (std::size_t choice = 0; choice < 31; ++choice) {
                    const auto name = numbered("choice", (11 * choice + 5) % 31);
                    node.choices.push_back({{name}, "text " + name, {"node00"}, {}, {}});
                }
            }
            draft.nodes.push_back(std::move(node));
        }
        return draft;
    }
}

TEST(dialogue_lookup, shuffled) {
    const auto draft = shuffled();
    const auto definition = D::compile(draft, {}).value();
    static_assert(noexcept(definition->find_node(D::NodeId{})));
    static_assert(noexcept(definition->find_choice(D::NodeId{}, D::ChoiceId{})));
    const auto* choice = definition->find_choice({"node13"}, {"choice05"});
    ASSERT_NE(choice, nullptr);
    EXPECT_EQ(choice->text, "text choice05");
    EXPECT_EQ(definition->entry().id.value, "node13");
    ASSERT_EQ(definition->nodes().size(), draft.nodes.size());
    for (std::size_t row = 0; row < draft.nodes.size(); ++row) {
        const auto& frozen = definition->nodes()[row];
        EXPECT_EQ(frozen.id, draft.nodes[row].id);
        EXPECT_EQ(frozen.text, draft.nodes[row].text);
        EXPECT_EQ(definition->find_node(frozen.id), &frozen);
        ASSERT_EQ(frozen.choices.size(), draft.nodes[row].choices.size());
        for (std::size_t index = 0; index < frozen.choices.size(); ++index) {
            EXPECT_EQ(frozen.choices[index].id, draft.nodes[row].choices[index].id);
            EXPECT_EQ(definition->find_choice(frozen.id, frozen.choices[index].id), &frozen.choices[index]);
        }
    }
}

TEST(dialogue_lookup, misses) {
    const auto definition = D::compile(shuffled(), {}).value();
    for (const auto& id : {"", "a-before", "node", "node035", "node13-extra", "Node13", "z-after"}) {
        EXPECT_EQ(definition->find_node({id}), nullptr);
        EXPECT_EQ(definition->find_choice({id}, {"choice05"}), nullptr);
    }
    for (const auto& id : {"", "a-before", "choice", "choice035", "choice05-extra", "Choice05", "z-after"})
        EXPECT_EQ(definition->find_choice({"node13"}, {id}), nullptr);
    EXPECT_EQ(definition->find_choice({"node00"}, {"choice05"}), nullptr);
    const auto terminal = D::compile({{"single"}, 1, {"one"}, {{{"one"}, "", D::NodeKind::Terminal, {}}}}, {}).value();
    EXPECT_EQ(terminal->find_node({"one"}), &terminal->entry());
    EXPECT_EQ(terminal->find_node({"two"}), nullptr);
    EXPECT_EQ(terminal->find_choice({"one"}, {"any"}), nullptr);
}

TEST(dialogue_lookup, binary_ids) {
    const std::string prefix(4096, 'n');
    const std::vector<std::string> ids{
        prefix + std::string("\0z", 2), prefix + std::string("\0a", 2), prefix + std::string("\0\xff", 2),
        prefix + std::string("\0\x80", 2), prefix
    };
    D::Draft draft{{"binary"}, 1, {ids[2]}, {}};
    for (const auto& id : ids) {
        D::Node node{{id}, "", D::NodeKind::Prompt, {}};
        for (const auto& name : ids)
            node.choices.push_back({{name}, "", {ids[0]}, {}, {}});
        draft.nodes.push_back(std::move(node));
    }
    const auto definition = D::compile(draft, {}).value();
    EXPECT_EQ(&definition->entry(), &definition->nodes()[2]);
    for (std::size_t row = 0; row < ids.size(); ++row) {
        EXPECT_EQ(definition->find_node({ids[row]}), &definition->nodes()[row]);
        for (std::size_t choice = 0; choice < ids.size(); ++choice)
            EXPECT_EQ(definition->find_choice({ids[row]}, {ids[choice]}), &definition->nodes()[row].choices[choice]);
    }
    for (const auto& missing : {prefix + std::string("\0", 1), prefix + std::string("\0b", 2), prefix + std::string("\0z\0", 3)}) {
        EXPECT_EQ(definition->find_node({missing}), nullptr);
        EXPECT_EQ(definition->find_choice({ids[0]}, {missing}), nullptr);
    }
}

TEST(dialogue_lookup, ownership) {
    D::DefinitionHandle retained;
    std::weak_ptr<const D::Definition> weak;
    const D::Node* node = nullptr;
    const D::Choice* choice = nullptr;
    {
        auto draft = shuffled();
        const auto definition = D::compile(draft, {}).value();
        weak = definition;
        retained = definition;
        node = definition->find_node({"node13"});
        choice = definition->find_choice({"node13"}, {"choice05"});
        ASSERT_NE(node, nullptr);
        ASSERT_NE(choice, nullptr);
        draft.nodes[0].choices.clear();
        draft.nodes.clear();
        draft.id.value = "mutated";
    }
    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(node->text, "node text 13");
    EXPECT_EQ(choice->text, "text choice05");
    for (int iteration = 0; iteration < 100; ++iteration) {
        EXPECT_EQ(retained->find_node({"node13"}), node);
        EXPECT_EQ(retained->find_choice({"node13"}, {"choice05"}), choice);
        EXPECT_EQ(retained->find_choice({"node13"}, {"missing"}), nullptr);
    }
    retained.reset();
    EXPECT_TRUE(weak.expired()); // Borrowed pointers are no longer dereferenced.
}

TEST(dialogue_lookup, scope) {
    D::Draft draft{
        {"scopes"},
        1,
        {"z"},
        {{{"z"}, "", D::NodeKind::Prompt, {{{"same"}, "z says", {"a"}, {}, {}}, {{"only-z"}, "", {"z"}, {}, {}}}},
         {{"a"}, "", D::NodeKind::Prompt, {{{"same"}, "a says", {"z"}, {}, {}}}}}
    };
    const auto definition = D::compile(draft, {}).value();
    const auto* z = definition->find_choice({"z"}, {"same"});
    const auto* a = definition->find_choice({"a"}, {"same"});
    ASSERT_NE(z, nullptr);
    ASSERT_NE(a, nullptr);
    EXPECT_NE(z, a);
    EXPECT_EQ(z, &definition->nodes()[0].choices[0]);
    EXPECT_EQ(a, &definition->nodes()[1].choices[0]);
    EXPECT_EQ(z->text, "z says");
    EXPECT_EQ(a->text, "a says");
    EXPECT_EQ(definition->find_choice({"a"}, {"only-z"}), nullptr);
    draft.nodes[0].choices[1].id = {"same"};
    const auto duplicate = D::compile(draft, {});
    ASSERT_FALSE(duplicate);
    const std::vector<D::Diagnostic> expected{{D::DiagnosticCode::DuplicateIdentity, {"scopes"}, 1, 0, 1, {}, "id", "same"}};
    EXPECT_EQ(duplicate.error(), expected);
}

TEST(dialogue_lookup, diagnostic_order) {
    std::vector<D::Diagnostic> errors;
    {
        const D::Draft draft{
            {"bad-index"},
            0,
            {"absent"},
            {{{"z"},
              "",
              D::NodeKind::Prompt,
              {{{"b"}, "", {"missing"}, D::ConditionKey{"not-c"}, D::EffectKey{"not-e"}}, {{"b"}, "", {"a"}, {}, {}}}},
             {{"a"}, "", D::NodeKind::Terminal, {{{"x"}, "", {"z"}, {}, {}}}},
             {{"z"}, "", D::NodeKind::Prompt, {}}}
        };
        const auto result = D::compile(draft, {{{"c"}, {"c"}}, {{"e"}, {""}}});
        ASSERT_FALSE(result);
        errors = result.error();
    }
    using C = D::DiagnosticCode;
    const std::vector<D::Diagnostic> expected{
        {C::InvalidRevision, {"bad-index"}, 0, {}, {}, {}, "revision", "0"},
        {C::DuplicateIdentity, {"bad-index"}, 0, {}, {}, 1, "supported_conditions", "c"},
        {C::EmptyIdentity, {"bad-index"}, 0, {}, {}, 1, "supported_effects", ""},
        {C::MissingReference, {"bad-index"}, 0, {}, {}, {}, "entry", "absent"},
        {C::MissingReference, {"bad-index"}, 0, 0, 0, {}, "target", "missing"},
        {C::UnsupportedOperation, {"bad-index"}, 0, 0, 0, {}, "condition", "not-c"},
        {C::UnsupportedOperation, {"bad-index"}, 0, 0, 0, {}, "effect", "not-e"},
        {C::DuplicateIdentity, {"bad-index"}, 0, 0, 1, {}, "id", "b"},
        {C::InvalidNodeShape, {"bad-index"}, 0, 1, {}, {}, "choices", "1"},
        {C::DuplicateIdentity, {"bad-index"}, 0, 2, {}, {}, "id", "z"},
        {C::InvalidNodeShape, {"bad-index"}, 0, 2, {}, {}, "choices", "0"}
    };
    EXPECT_EQ(errors, expected);
}
