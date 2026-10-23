#include <cheryl/core/game-framework/dialogue.h>

#include "../src/definition-codec.h"
#include "../src/definition-report.h"
#include "support.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <utility>

namespace {
    namespace G = GateConsole;
    namespace D = CE::Dialogue;
    constexpr std::string_view advisory =
        "Advisory topology: conditions are ignored; no proof of gameplay reachability, effect history, termination or guaranteed completion.\n";
    std::string fixture(std::string_view name) {
        return std::string(DIALOGUE_CONSOLE_FIXTURES) + "/" + std::string(name) + ".dialogue.json";
    }
    std::vector<std::string> names(const std::vector<D::NodeId>& ids) {
        std::vector<std::string> result;
        for (const auto& id : ids)
            result.push_back(id.value);
        return result;
    }
    D::Node terminal(std::string id) {
        return {{std::move(id)}, "", D::NodeKind::Terminal, {}};
    }
    D::Node prompt(std::string id, std::initializer_list<std::string> targets) {
        D::Node node{{std::move(id)}, "", D::NodeKind::Prompt, {}};
        for (const auto& target : targets)
            node.choices.push_back({{"c" + std::to_string(node.choices.size())}, "", {target}, {}, {}});
        return node;
    }
    D::DefinitionHandle compiled(const D::Draft& draft) {
        return D::compile(draft, G::operation_keys()).value();
    }
    G::DefinitionReport report(const D::Draft& draft) {
        return G::inspect_definition(*compiled(draft)).value();
    }
    D::Draft full_graph() {
        D::Draft draft{{"capacity"}, 1, {"n0"}, {}};
        for (std::size_t index = 0; index < G::maximum_definition_nodes; ++index) {
            auto node = terminal("n" + std::to_string(index));
            if (index + 1 < G::maximum_definition_nodes) {
                node.kind = D::NodeKind::Prompt;
                for (std::size_t edge = 0; edge < (index == 0 ? 16U : 8U); ++edge)
                    node.choices.push_back({{"c" + std::to_string(edge)}, "", {"n" + std::to_string(index + 1)}, {}, {}});
            }
            draft.nodes.push_back(std::move(node));
        }
        return draft;
    }
}

TEST(dialogue_console_report, consumer) {
    auto definition = G::load_definition(fixture("gate"));
    ASSERT_TRUE(definition);
    auto inspected = G::inspect_definition(**definition);
    ASSERT_TRUE(inspected);
    const std::string literal = "Definition: \"gate-encounter\"\nRevision: 1\nEntry: \"approach\"\nNodes: 4\nChoices: 4\nTerminals: 2\n"
                                "Entry-reachable (4): [\"approach\", \"gate\", \"admitted\", \"departed\"]\n"
                                "Unreachable (0): []\nNo terminal route (0): []\n";
    EXPECT_EQ(G::format_definition_report(*inspected), literal + std::string(advisory));
}

TEST(dialogue_console_report, crossroads) {
    const auto definition = G::load_definition(fixture("crossroads")).value();
    const auto inspected = G::inspect_definition(*definition).value();
    EXPECT_EQ(
        G::format_definition_report(inspected),
        "Definition: \"crossroads\"\nRevision: 7\nEntry: \"arrival\"\nNodes: 2\nChoices: 1\nTerminals: 1\n"
        "Entry-reachable (2): [\"arrival\", \"camp\"]\nUnreachable (0): []\nNo terminal route (0): []\n" +
            std::string(advisory)
    );
}

TEST(dialogue_console_report, branch_rejoin) {
    // Nonfirst entry and reverse authored order deliberately differ from BFS
    // order. The rejoin cycle can reach a terminal; the separate cycle cannot.
    const D::Draft draft{
        {"branches"},
        4,
        {"start"},
        {terminal("end"), prompt("right", {"join"}), prompt("start", {"left", "right"}), prompt("join", {"end", "left"}),
         prompt("left", {"join"}), terminal("orphan"), prompt("trap-a", {"trap-b"}), prompt("trap-b", {"trap-a"})}
    };
    const auto inspected = report(draft);
    EXPECT_EQ(inspected.entry.value, "start");
    EXPECT_EQ(inspected.nodes, 8);
    EXPECT_EQ(inspected.choices, 8);
    EXPECT_EQ(inspected.terminals, 2);
    EXPECT_EQ(names(inspected.reachable), (std::vector<std::string>{"end", "right", "start", "join", "left"}));
    EXPECT_EQ(names(inspected.unreachable), (std::vector<std::string>{"orphan", "trap-a", "trap-b"}));
    EXPECT_EQ(names(inspected.no_terminal_route), (std::vector<std::string>{"trap-a", "trap-b"}));
    EXPECT_EQ(G::format_definition_report(inspected), G::format_definition_report(report(draft)));
}

TEST(dialogue_console_report, closed_cycle) {
    const auto inspected = report({{"loop"}, 1, {"a"}, {prompt("a", {"b"}), terminal("unreachable-end"), prompt("b", {"a"})}});
    EXPECT_EQ(inspected.terminals, 1);
    EXPECT_EQ(names(inspected.reachable), (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(names(inspected.unreachable), (std::vector<std::string>{"unreachable-end"}));
    EXPECT_EQ(names(inspected.no_terminal_route), (std::vector<std::string>{"a", "b"}));
    EXPECT_NE(G::format_definition_report(inspected).find("No terminal route (2): [\"a\", \"b\"]"), std::string::npos);
}

TEST(dialogue_console_report, terminal_and_loop) {
    const auto end = report({{"end"}, 1, {"done"}, {terminal("done")}});
    EXPECT_EQ(end.nodes, 1);
    EXPECT_EQ(end.choices, 0);
    EXPECT_EQ(end.terminals, 1);
    EXPECT_EQ(names(end.reachable), (std::vector<std::string>{"done"}));
    EXPECT_TRUE(end.no_terminal_route.empty());
    const auto loop = report({{"forever"}, 1, {"loop"}, {prompt("loop", {"loop"})}});
    EXPECT_EQ(loop.terminals, 0);
    EXPECT_EQ(loop.choices, 1);
    EXPECT_EQ(names(loop.no_terminal_route), (std::vector<std::string>{"loop"}));
}

TEST(dialogue_console_report, ignores_conditions) {
    auto draft = D::Draft{{"conditional"}, 1, {"wait"}, {prompt("wait", {"done"}), terminal("done")}};
    draft.nodes[0].choices[0].condition = D::ConditionKey{"introduced"};
    draft.nodes[0].choices[0].effect = D::EffectKey{"introduce"};
    const auto inspected = report(draft);
    // No Session, flags or handlers exist here. The initially unavailable edge
    // still supplies a structural route; the report cannot promise gameplay.
    EXPECT_EQ(inspected.reachable.size(), 2);
    EXPECT_TRUE(inspected.no_terminal_route.empty());
    EXPECT_NE(G::format_definition_report(inspected).find(advisory), std::string::npos);
}

TEST(dialogue_console_report, ownership) {
    G::DefinitionReport owned;
    std::weak_ptr<const D::Definition> weak;
    {
        auto bytes = GateTesting::contents(fixture("gate"));
        auto draft = G::decode_definition(bytes, "temporary").value();
        auto definition = compiled(draft);
        weak = definition;
        owned = G::inspect_definition(*definition).value();
        bytes.assign("destroyed");
        draft.id.value = "changed";
        draft.nodes.clear();
    }
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(owned.definition.value, "gate-encounter");
    EXPECT_EQ(owned.entry.value, "approach");
    EXPECT_EQ(names(owned.reachable), (std::vector<std::string>{"approach", "gate", "admitted", "departed"}));
    const auto literal = G::format_definition_report(owned);
    auto copy = owned;
    owned.definition.value.clear();
    owned.reachable.clear();
    EXPECT_EQ(G::format_definition_report(copy), literal);
}

TEST(dialogue_console_report, graph_capacity) {
    auto draft = full_graph();
    const auto inspected = report(draft);
    EXPECT_EQ(inspected.nodes, 256);
    EXPECT_EQ(inspected.choices, 2048);
    EXPECT_EQ(inspected.terminals, 1);
    EXPECT_EQ(inspected.reachable.size(), 256);
    EXPECT_TRUE(inspected.unreachable.empty());
    EXPECT_TRUE(inspected.no_terminal_route.empty());
    EXPECT_EQ(inspected.reachable.back().value, "n255");
    // Direct Engine compilation has no consumer caps. Exact graph capacity is
    // feasible here even though 2048 JSON choice rows exceed the loader byte cap.
    draft.nodes[1].choices.push_back({{"extra"}, "", {"n2"}, {}, {}});
    const auto excess_edges = G::inspect_definition(*compiled(draft));
    ASSERT_FALSE(excess_edges);
    EXPECT_EQ(excess_edges.error(), "Inspection exceeds 2048 choices");
    draft.nodes.push_back(terminal("n256"));
    const auto excess_nodes = G::inspect_definition(*compiled(draft));
    ASSERT_FALSE(excess_nodes);
    EXPECT_EQ(excess_nodes.error(), "Inspection exceeds 256 nodes");
    draft.nodes.clear();
    for (std::size_t index = 0; index < G::maximum_definition_nodes; ++index)
        draft.nodes.push_back(terminal("n" + std::to_string(index)));
    draft.entry = draft.nodes.back().id;
    const auto all_terminals = report(draft);
    EXPECT_EQ(all_terminals.terminals, 256);
    EXPECT_EQ(all_terminals.choices, 0);
    EXPECT_EQ(names(all_terminals.reachable), (std::vector<std::string>{"n255"}));
    EXPECT_EQ(all_terminals.unreachable.size(), 255);
    EXPECT_TRUE(all_terminals.no_terminal_route.empty());
}

TEST(dialogue_console_report, id_capacity) {
    const auto id = std::string(G::maximum_definition_id_bytes, 'x');
    D::Draft draft{{id}, 1, {id}, {terminal(id)}};
    const auto inspected = report(draft);
    EXPECT_EQ(inspected.definition.value, id);
    EXPECT_EQ(inspected.reachable[0].value, id);
    draft.id.value += 'x';
    const auto long_definition = G::inspect_definition(*compiled(draft));
    ASSERT_FALSE(long_definition);
    EXPECT_EQ(long_definition.error(), "Inspection definition ID exceeds 128 bytes");
    draft.id.value = "short";
    draft.entry.value += 'x';
    draft.nodes[0].id = draft.entry;
    const auto long_node = G::inspect_definition(*compiled(draft));
    ASSERT_FALSE(long_node);
    EXPECT_EQ(long_node.error(), "Inspection node ID exceeds 128 bytes");
}

TEST(dialogue_console_report, display_escaping) {
    const std::string id = "a\n\"\\" + std::string(1, '\0') + '\x7f';
    const auto inspected = report({{id}, 1, {id}, {terminal(id)}});
    const auto output = G::format_definition_report(inspected);
    EXPECT_TRUE(output.starts_with("Definition: \"a\\u000a\\\"\\\\\\u0000\\u007f\"\n"));
    EXPECT_EQ(output.find('\0'), std::string::npos);
    EXPECT_EQ(std::count(output.begin(), output.end(), '\n'), 10);
}
