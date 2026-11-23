#include <cheryl/core/game-framework/dialogue-inspection.h>

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <utility>

namespace {
    namespace D = CE::Dialogue;
    D::Node end(std::string id) {
        return {{std::move(id)}, "", D::NodeKind::Terminal, {}};
    }
    D::Node node(std::string id, std::initializer_list<std::string> targets) {
        D::Node result{{std::move(id)}, "", D::NodeKind::Prompt, {}};
        for (const auto& target : targets)
            result.choices.push_back({{"c" + std::to_string(result.choices.size())}, "", {target}, {}, {}});
        return result;
    }
    D::DefinitionHandle freeze(const D::Draft& draft) {
        return D::compile(draft, {{{"condition"}}, {{"effect"}}}).value();
    }
    D::DefinitionHandle simple() {
        return freeze({{"definition"}, 2, {"entry"}, {node("entry", {"end"}), end("end")}});
    }
    std::vector<std::string> names(const std::vector<D::NodeId>& values) {
        std::vector<std::string> result;
        for (const auto& value : values)
            result.push_back(value.value);
        return result;
    }
    void refused(const D::Definition& definition, D::InspectionLimits limits, D::InspectionError error) {
        const auto result = D::inspect(definition, limits);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), error);
    }
}

TEST(dialogue_inspection, literal) {
    D::Draft draft{
        {"branches"},
        3,
        {"entry"},
        {end("done"), node("right", {"join"}), node("entry", {"left", "right"}), node("join", {"done", "left"}), node("left", {"join"}),
         end("orphan"), node("closed", {"closed"})}
    };
    draft.nodes[2].choices[0].condition = D::ConditionKey{"condition"};
    draft.nodes[2].choices[0].effect = D::EffectKey{"effect"};
    // Operation names are data. No handlers, Session or application state exist.
    const auto definition = freeze(draft);
    const auto report = D::inspect(*definition).value();
    EXPECT_EQ(report.definition.value, "branches");
    EXPECT_EQ(report.revision, 3);
    EXPECT_EQ(report.entry.value, "entry");
    EXPECT_EQ(report.nodes, 7);
    EXPECT_EQ(report.choices, 7);
    EXPECT_EQ(report.terminals, 2);
    EXPECT_EQ(names(report.reachable), (std::vector<std::string>{"done", "right", "entry", "join", "left"}));
    EXPECT_EQ(names(report.unreachable), (std::vector<std::string>{"orphan", "closed"}));
    EXPECT_EQ(names(report.no_terminal_route), (std::vector<std::string>{"closed"}));
}

TEST(dialogue_inspection, terminal_free) {
    const auto definition = freeze({{"cycles"}, 1, {"b"}, {node("a", {"b"}), node("b", {"a"}), node("c", {"c"})}});
    const auto report = D::inspect(*definition).value();
    EXPECT_EQ(report.entry.value, "b");
    EXPECT_EQ(report.terminals, 0);
    EXPECT_EQ(names(report.reachable), (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(names(report.unreachable), (std::vector<std::string>{"c"}));
    EXPECT_EQ(names(report.no_terminal_route), (std::vector<std::string>{"a", "b", "c"}));
}

TEST(dialogue_inspection, parallel_self) {
    const auto definition = freeze({{"edges"}, 1, {"start"}, {end("end"), node("start", {"start", "end", "end"})}});
    const auto report = D::inspect(*definition).value();
    EXPECT_EQ(report.choices, 3);
    EXPECT_EQ(report.terminals, 1);
    EXPECT_EQ(names(report.reachable), (std::vector<std::string>{"end", "start"}));
    EXPECT_TRUE(report.no_terminal_route.empty());
    const auto terminal = freeze({{"single"}, 1, {"last"}, {end("earlier"), end("last")}});
    const auto report2 = D::inspect(*terminal).value();
    EXPECT_EQ(report2.choices, 0);
    EXPECT_EQ(report2.terminals, 2);
    EXPECT_EQ(names(report2.reachable), (std::vector<std::string>{"last"}));
    EXPECT_EQ(names(report2.unreachable), (std::vector<std::string>{"earlier"}));
    EXPECT_TRUE(report2.no_terminal_route.empty());
}

TEST(dialogue_inspection, ownership) {
    D::InspectionReport report;
    std::weak_ptr<const D::Definition> weak;
    {
        D::Draft draft{{"owned"}, 8, {"start"}, {node("start", {"start"}), end("unused")}};
        auto definition = freeze(draft);
        weak = definition;
        report = D::inspect(*definition).value();
        draft.id.value = "changed";
        draft.nodes.clear();
    }
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(report.definition.value, "owned");
    EXPECT_EQ(report.revision, 8);
    EXPECT_EQ(report.entry.value, "start");
    EXPECT_EQ(names(report.reachable), (std::vector<std::string>{"start"}));
    EXPECT_EQ(names(report.unreachable), (std::vector<std::string>{"unused"}));
    EXPECT_EQ(names(report.no_terminal_route), (std::vector<std::string>{"start"}));
    auto independent = report;
    report.no_terminal_route.clear();
    EXPECT_EQ(names(independent.no_terminal_route), (std::vector<std::string>{"start"}));
}

TEST(dialogue_inspection, nodes_choices) {
    const auto definition = freeze({{"limit"}, 1, {"n"}, {node("n", {"n", "end", "end"}), end("end")}});
    D::InspectionLimits limits;
    limits.nodes = 2;
    limits.choices = 3;
    ASSERT_TRUE(D::inspect(*definition, limits));
    limits.nodes = 1;
    refused(*definition, limits, D::InspectionError::NodeLimit);
    limits.nodes = 2;
    limits.choices = 2;
    refused(*definition, limits, D::InspectionError::ChoiceLimit);
    limits.choices = 0;
    refused(*definition, limits, D::InspectionError::ChoiceLimit);
    limits.nodes = 0;
    refused(*definition, limits, D::InspectionError::NodeLimit);
    const auto terminal = freeze({{"empty-edges"}, 1, {"end"}, {end("end")}});
    limits.nodes = 1;
    ASSERT_TRUE(D::inspect(*terminal, limits)); // Zero edges is a usable budget.
}

TEST(dialogue_inspection, copied_ids) {
    const auto definition = freeze({{"report"}, 1, {"loop"}, {node("loop", {"loop"}), end("end")}});
    const auto old = D::inspect(*definition).value();
    // Definition + entry + both partition IDs + no-terminal-route loop again.
    EXPECT_EQ(old.usage.copied_id_bytes, 6 + 4 + 4 + 3 + 4);
    D::InspectionLimits limits;
    limits.copied_id_bytes = old.usage.copied_id_bytes;
    ASSERT_TRUE(D::inspect(*definition, limits));
    --limits.copied_id_bytes;
    refused(*definition, limits, D::InspectionError::CopiedIdLimit);
    EXPECT_EQ(old.definition.value, "report");
    EXPECT_EQ(names(old.no_terminal_route), (std::vector<std::string>{"loop"}));
    EXPECT_EQ(definition->entry().id.value, "loop");
    limits.copied_id_bytes = 0;
    refused(*definition, limits, D::InspectionError::CopiedIdLimit);
    const auto long_id = std::string(256, 'x');
    const auto longer = freeze({{long_id}, 1, {long_id}, {end(long_id)}});
    EXPECT_EQ(D::inspect(*longer)->usage.copied_id_bytes, 3 * long_id.size());
}

TEST(dialogue_inspection, storage) {
    const auto definition = simple();
    const auto report = D::inspect(*definition).value();
    EXPECT_EQ(report.usage.copied_id_bytes, 10 + 5 + 5 + 3);
    const auto slots = report.nodes + report.no_terminal_route.size();
    const auto words = 2 * report.choices + 6 * report.nodes + 3;
    EXPECT_EQ(
        report.usage.storage_bytes,
        words * sizeof(std::size_t) + sizeof(D::InspectionReport) + slots * sizeof(D::NodeId) + report.usage.copied_id_bytes + slots + 2
    );
    D::InspectionLimits limits;
    limits.storage_bytes = report.usage.storage_bytes;
    ASSERT_TRUE(D::inspect(*definition, limits));
    --limits.storage_bytes;
    refused(*definition, limits, D::InspectionError::StorageLimit);
    limits.storage_bytes = 0;
    refused(*definition, limits, D::InspectionError::StorageLimit);
    EXPECT_EQ(names(report.reachable), (std::vector<std::string>{"entry", "end"}));
    // Refuse after traversal when the additional no-terminal-route list makes
    // the final payload exceed a budget that admitted scratch/minimum output.
    const auto loop = freeze({{"loop"}, 1, {"a"}, {node("a", {"a"})}});
    const auto old_loop = D::inspect(*loop).value();
    limits.storage_bytes = old_loop.usage.storage_bytes;
    ASSERT_TRUE(D::inspect(*loop, limits));
    --limits.storage_bytes;
    refused(*loop, limits, D::InspectionError::StorageLimit);
    EXPECT_EQ(names(old_loop.no_terminal_route), (std::vector<std::string>{"a"}));
}

TEST(dialogue_inspection, work) {
    const auto definition = simple();
    const auto report = D::inspect(*definition).value();
    EXPECT_GT(report.usage.work, report.usage.copied_id_bytes);
    D::InspectionLimits limits;
    limits.work = report.usage.work;
    const auto exact = D::inspect(*definition, limits);
    ASSERT_TRUE(exact);
    EXPECT_EQ(exact->usage.work, report.usage.work);
    --limits.work;
    refused(*definition, limits, D::InspectionError::WorkLimit);
    limits.work = 0;
    refused(*definition, limits, D::InspectionError::WorkLimit);
    limits.work = 3; // Preflight succeeds; scratch initialization is refused.
    refused(*definition, limits, D::InspectionError::WorkLimit);
}

TEST(dialogue_inspection, invalid_limits) {
    const auto definition = simple();
    // Maximum configured allowances allocate only the small actual graph.
    ASSERT_TRUE(D::inspect(*definition, D::maximum_inspection_limits));
    for (const auto field :
         {&D::InspectionLimits::nodes, &D::InspectionLimits::choices, &D::InspectionLimits::copied_id_bytes,
          &D::InspectionLimits::storage_bytes, &D::InspectionLimits::work}) {
        auto limits = D::InspectionLimits{};
        limits.*field = D::maximum_inspection_limits.*field + 1;
        refused(*definition, limits, D::InspectionError::InvalidLimits);
        limits.*field = std::numeric_limits<std::size_t>::max();
        refused(*definition, limits, D::InspectionError::InvalidLimits);
    }
    ASSERT_TRUE(D::inspect(*definition));
}

TEST(dialogue_inspection, larger) {
    D::Draft draft{{"larger"}, 1, {"n256"}, {}};
    for (std::size_t index = 0; index < 257; ++index)
        draft.nodes.push_back(end("n" + std::to_string(index)));
    auto definition = freeze(draft);
    refused(*definition, {}, D::InspectionError::NodeLimit);
    D::InspectionLimits limits;
    limits.nodes = 257;
    const auto report = D::inspect(*definition, limits).value();
    EXPECT_EQ(report.terminals, 257);
    EXPECT_EQ(report.unreachable.size(), 256);
    EXPECT_EQ(report.entry.value, "n256");
    draft.nodes[0].kind = D::NodeKind::Prompt;
    for (std::size_t edge = 0; edge < 2049; ++edge)
        draft.nodes[0].choices.push_back({{"c" + std::to_string(edge)}, "", {"n256"}, {}, {}});
    definition = freeze(draft);
    refused(*definition, limits, D::InspectionError::ChoiceLimit);
    limits.choices = 2049;
    const auto edges = D::inspect(*definition, limits).value();
    EXPECT_EQ(edges.choices, 2049);
    EXPECT_TRUE(edges.no_terminal_route.empty());
}

TEST(dialogue_inspection, compared_bytes) {
    const auto graph = [](bool common) {
        std::vector<std::string> ids;
        for (const char letter : {'a', 'b', 'c'})
            ids.push_back(common ? std::string(31, 'x') + letter : letter + std::string(31, 'x'));
        return freeze({{"compare"}, 1, {ids[0]}, {node(ids[0], {ids[2]}), node(ids[1], {ids[2]}), end(ids[2])}});
    };
    const auto early = D::inspect(*graph(false)).value();
    const auto late = D::inspect(*graph(true)).value();
    EXPECT_EQ(early.usage.copied_id_bytes, late.usage.copied_id_bytes);
    EXPECT_EQ(early.usage.storage_bytes, late.usage.storage_bytes);
    EXPECT_GT(late.usage.work, early.usage.work);
    D::InspectionLimits limits;
    limits.work = early.usage.work;
    ASSERT_TRUE(D::inspect(*graph(false), limits));
    refused(*graph(true), limits, D::InspectionError::WorkLimit);
}
