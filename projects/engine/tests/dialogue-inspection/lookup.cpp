#include <cheryl/core/game-framework/dialogue.h>

#include <gtest/gtest.h>

#include <memory>

namespace {
    namespace D = CE::Dialogue;
}

TEST(dialogue_lookup_consumer, caller) {
    const auto definition =
        D::compile(
            {{"public-lookup"},
             1,
             {"gate"},
             {{{"z-end"}, "", D::NodeKind::Terminal, {}},
              {{"gate"}, "", D::NodeKind::Prompt, {{{"wait"}, "Wait", {"gate"}, {}, {}}, {{"pass"}, "Pass", {"z-end"}, {}, {}}}},
              {{"a-start"}, "", D::NodeKind::Prompt, {{{"pass"}, "Approach", {"gate"}, {}, {}}}}}},
            {}
        )
            .value();
    const auto* choice = definition->find_choice({"gate"}, {"pass"});
    ASSERT_NE(choice, nullptr);
    EXPECT_EQ(choice, &definition->nodes()[1].choices[1]);
    EXPECT_EQ(choice->text, "Pass");
    EXPECT_EQ(definition->find_node({"gate"}), &definition->entry());
    EXPECT_EQ(definition->find_choice({"a-start"}, {"pass"}), &definition->nodes()[2].choices[0]);
    EXPECT_EQ(definition->find_choice({"a-start"}, {"wait"}), nullptr);
    EXPECT_EQ(definition->find_choice({"missing"}, {"pass"}), nullptr);
    EXPECT_EQ(definition->nodes()[0].id.value, "z-end");
    EXPECT_EQ(definition->nodes()[1].choices[0].id.value, "wait");
}

TEST(dialogue_lookup_consumer, retained) {
    const D::NodeId owner{std::string(256, 'n') + std::string("\0owner", 6)};
    const D::ChoiceId key{std::string(256, 'c') + std::string("\0key", 4)};
    D::DefinitionHandle retained;
    std::weak_ptr<const D::Definition> weak;
    const D::Choice* pointer = nullptr;
    {
        D::Draft draft{
            {"retained-lookup"},
            1,
            owner,
            {{{"end"}, "", D::NodeKind::Terminal, {}},
             {owner, "", D::NodeKind::Prompt, {{{"z"}, "", {"end"}, {}, {}}, {key, "Retained", {"end"}, {}, {}}}}}
        };
        const auto compiled = D::compile(draft, {}).value();
        weak = compiled;
        retained = compiled;
        pointer = compiled->find_choice(owner, key);
        ASSERT_EQ(pointer, &compiled->nodes()[1].choices[1]);
        draft.nodes.clear();
    }
    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(retained->find_choice(owner, key), pointer);
    EXPECT_EQ(pointer->text, "Retained");
    retained.reset();
    EXPECT_TRUE(weak.expired());
}
