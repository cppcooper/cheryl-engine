#include <cheryl/core/game-framework/dialogue-route.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <queue>
#include <utility>

namespace {
    namespace D = CE::Dialogue;

    D::Node node(std::string id, std::vector<std::string> targets = {}) {
        D::Node result{{std::move(id)}, "", targets.empty() ? D::NodeKind::Terminal : D::NodeKind::Prompt, {}};
        for (std::size_t index = 0; index < targets.size(); ++index)
            result.choices.push_back({{"c" + std::to_string(index)}, "", {targets[index]}, {}, {}});
        return result;
    }
    D::DefinitionHandle freeze(const D::Draft& draft) {
        return D::compile(draft, {{{"closed"}}, {{"effect"}}}).value();
    }
    D::DefinitionHandle literal() {
        return freeze(
            {{"literal"},
             9,
             {"elsewhere"},
             {node("end"), node("a", {"end"}), node("start", {"z", "a"}), node("z", {"end"}), node("elsewhere", {"elsewhere"})}}
        );
    }
    template <class Id> std::vector<std::string> names(const std::vector<Id>& ids) {
        std::vector<std::string> result;
        for (const auto& id : ids)
            result.push_back(id.value);
        return result;
    }
    void refused(
        const D::Definition& definition,
        const D::NodeId& from,
        const D::NodeId& to,
        const D::RouteLimits& limits,
        D::RouteError error
    ) {
        const auto result = D::find_route(definition, from, to, limits);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), error);
    }

    // Separate unbudgeted tiny-fixture reference: linear name search, queued owned
    // paths and a visited set. It uses neither Engine indices nor parent reconstruction.
    std::optional<std::pair<std::vector<std::string>, std::vector<std::string>>>
    reference(const D::Draft& draft, std::size_t from, std::size_t to) {
        struct Path {
            std::size_t last;
            std::vector<std::string> nodes;
            std::vector<std::string> choices;
        };
        std::queue<Path> queue;
        std::vector<bool> visited(draft.nodes.size(), false);
        queue.push({from, {draft.nodes[from].id.value}, {}});
        visited[from] = true;
        while (!queue.empty()) {
            auto path = std::move(queue.front());
            queue.pop();
            if (path.last == to)
                return std::pair{path.nodes, path.choices};
            for (const auto& choice : draft.nodes[path.last].choices) {
                const auto found =
                    std::find_if(draft.nodes.begin(), draft.nodes.end(), [&](const auto& row) { return row.id == choice.target; });
                const auto next = static_cast<std::size_t>(found - draft.nodes.begin());
                if (visited[next])
                    continue;
                visited[next] = true;
                auto extended = path;
                extended.last = next;
                extended.nodes.push_back(choice.target.value);
                extended.choices.push_back(choice.id.value);
                queue.push(std::move(extended));
            }
        }
        return {};
    }
}

TEST(dialogue_route, caller) {
    const auto definition = literal();
    const auto route = D::find_route(*definition, {"start"}, {"end"}, D::RouteLimits{});
    ASSERT_TRUE(route);
    EXPECT_EQ(route->definition.value, "literal");
    EXPECT_EQ(route->revision, 9);
    EXPECT_EQ(names(route->nodes), (std::vector<std::string>{"start", "z", "end"}));
    EXPECT_EQ(names(route->choices), (std::vector<std::string>{"c0", "c0"}));
    EXPECT_EQ(definition->entry().id.value, "elsewhere");
    for (std::size_t index = 0; index < route->choices.size(); ++index) {
        const auto* choice = definition->find_choice(route->nodes[index], route->choices[index]);
        ASSERT_NE(choice, nullptr);
        EXPECT_EQ(choice->target, route->nodes[index + 1]);
    }
}

TEST(dialogue_route, ties) {
    D::Draft draft{{"ties"}, 1, {"s"}, {node("a", {"end"}), node("end"), node("s", {"s", "z", "z", "a"}), node("z", {"end"})}};
    auto& choices = draft.nodes[2].choices;
    choices[1].id = {"z-first"};
    choices[1].condition = D::ConditionKey{"closed"};
    choices[1].effect = D::EffectKey{"effect"};
    choices[2].id = {"a-parallel"};
    const auto old = D::find_route(*freeze(draft), {"s"}, {"end"}).value();
    EXPECT_EQ(names(old.nodes), (std::vector<std::string>{"s", "z", "end"}));
    EXPECT_EQ(names(old.choices), (std::vector<std::string>{"z-first", "c0"}));
    std::swap(choices[1], choices[3]);
    const auto changed = D::find_route(*freeze(draft), {"s"}, {"end"}).value();
    EXPECT_EQ(names(changed.nodes), (std::vector<std::string>{"s", "a", "end"}));
    EXPECT_EQ(names(old.choices), (std::vector<std::string>{"z-first", "c0"}));
}

TEST(dialogue_route, shortest) {
    const auto definition = freeze(
        {{"shortest"},
         1,
         {"long"},
         {node("s", {"long", "short"}), node("end"), node("long", {"middle"}), node("short", {"end"}), node("middle", {"end"})}}
    );
    const auto route = D::find_route(*definition, {"s"}, {"end"}).value();
    EXPECT_EQ(names(route.nodes), (std::vector<std::string>{"s", "short", "end"}));
    EXPECT_EQ(names(route.choices), (std::vector<std::string>{"c1", "c0"}));
    const auto prompt = D::find_route(*definition, {"s"}, {"middle"}).value();
    EXPECT_EQ(names(prompt.nodes), (std::vector<std::string>{"s", "long", "middle"}));
}

TEST(dialogue_route, cycles) {
    const auto definition =
        freeze({{"cycles"}, 1, {"a"}, {node("a", {"a", "b", "b"}), node("b", {"a", "c"}), node("c", {"b"}), node("end")}});
    const auto route = D::find_route(*definition, {"a"}, {"c"}).value();
    EXPECT_EQ(names(route.nodes), (std::vector<std::string>{"a", "b", "c"}));
    EXPECT_EQ(names(route.choices), (std::vector<std::string>{"c1", "c1"}));
    refused(*definition, {"a"}, {"end"}, {}, D::RouteError::Unreachable);
    refused(*definition, {"end"}, {"a"}, {}, D::RouteError::Unreachable);
    const auto without_terminal = freeze({{"no-terminal"}, 1, {"b"}, {node("a", {"b"}), node("b", {"a"})}});
    EXPECT_EQ(names(D::find_route(*without_terminal, {"b"}, {"a"}).value().nodes), (std::vector<std::string>{"b", "a"}));
}

TEST(dialogue_route, equal) {
    const auto definition = literal();
    D::RouteLimits limits;
    limits.route_edges = 0;
    for (const auto& id : {"start", "end", "elsewhere"}) {
        const auto route = D::find_route(*definition, {id}, {id}, limits);
        ASSERT_TRUE(route);
        EXPECT_EQ(names(route->nodes), (std::vector<std::string>{id}));
        EXPECT_TRUE(route->choices.empty());
    }
    refused(*definition, {"start"}, {"end"}, limits, D::RouteError::OutputLimit);
    const auto terminal = freeze({{"terminal"}, 1, {"one"}, {node("one")}});
    limits.choices = 0;
    ASSERT_TRUE(D::find_route(*terminal, {"one"}, {"one"}, limits));
}

TEST(dialogue_route, endpoints) {
    const auto definition = literal();
    for (const auto& missing : {"", "Start", "star", "start-extra", "absent"}) {
        refused(*definition, {missing}, {"end"}, {}, D::RouteError::UnknownFrom);
        refused(*definition, {"start"}, {missing}, {}, D::RouteError::UnknownTo);
    }
    refused(*definition, {"missing-from"}, {"missing-to"}, {}, D::RouteError::UnknownFrom);
    D::RouteLimits limits;
    limits.work = 0;
    refused(*definition, {"missing"}, {"end"}, limits, D::RouteError::WorkLimit);
}

TEST(dialogue_route, owned) {
    const std::string prefix(4096, 'x');
    const auto from = prefix + std::string("\0\xff", 2);
    const auto to = prefix + std::string("\0\x80", 2);
    const auto choice = prefix + std::string("\0choice", 7);
    D::Route owned;
    std::weak_ptr<const D::Definition> weak;
    {
        D::Draft draft{{std::string("owned\0id", 8)}, 17, {to}, {node(to), node(from, {to})}};
        draft.nodes[1].choices[0].id = {choice};
        const auto definition = freeze(draft);
        weak = definition;
        owned = D::find_route(*definition, {from}, {to}).value();
        draft.nodes[1].choices[0].id.value = "changed";
        draft.nodes.clear();
        draft.id.value.clear();
        refused(*definition, {prefix}, {to}, {}, D::RouteError::UnknownFrom);
    }
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(owned.definition.value, std::string("owned\0id", 8));
    EXPECT_EQ(owned.revision, 17);
    EXPECT_EQ(names(owned.nodes), (std::vector<std::string>{from, to}));
    EXPECT_EQ(names(owned.choices), (std::vector<std::string>{choice}));
    auto independent = owned;
    independent.nodes[0].value.clear();
    EXPECT_EQ(owned.nodes[0].value, from);
}

TEST(dialogue_route, advisory) {
    auto draft = D::Draft{{"advisory"}, 1, {"gate"}, {node("gate", {"end"}), node("end")}};
    draft.nodes[0].choices[0].condition = D::ConditionKey{"closed"};
    draft.nodes[0].choices[0].effect = D::EffectKey{"effect"};
    const auto definition = freeze(draft);
    auto session = D::Session::start(definition, {"live"}).value();
    int predicates = 0, effects = 0;
    const D::ConditionHandler condition = [&](const auto&) {
        ++predicates;
        return false;
    };
    const D::EffectHandler effect = [&](const auto&) { ++effects; };
    const auto view = session.view(condition).value();
    const auto before = session.snapshot().value();
    ASSERT_EQ(predicates, 1);
    ASSERT_FALSE(view.choices[0].available);
    const auto route = D::find_route(*definition, {"gate"}, {"end"}).value();
    EXPECT_EQ(names(route.choices), (std::vector<std::string>{"c0"}));
    EXPECT_EQ(predicates, 1);
    EXPECT_EQ(effects, 0);
    EXPECT_EQ(session.snapshot().value(), before);
    const auto rejected = session.choose(view.choices[0].selection, condition, effect);
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error(), D::SessionError::Unavailable);
    EXPECT_EQ(effects, 0);
    EXPECT_EQ(session.snapshot().value(), before);
}

TEST(dialogue_route, budgets) {
    const auto definition = literal();
    const auto old = D::find_route(*definition, {"start"}, {"end"}).value();
    D::RouteLimits exact{5, 5, 2, old.usage.copied_id_bytes, old.usage.storage_bytes, old.usage.work};
    const auto result = D::find_route(*definition, {"start"}, {"end"}, exact);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->usage.work, old.usage.work);
    EXPECT_EQ(result->usage.copied_id_bytes, old.usage.copied_id_bytes);
    EXPECT_EQ(result->usage.storage_bytes, old.usage.storage_bytes);
    const std::pair<std::size_t D::RouteLimits::*, D::RouteError> fields[] = {
        {&D::RouteLimits::nodes, D::RouteError::NodeLimit},
        {&D::RouteLimits::choices, D::RouteError::ChoiceLimit},
        {&D::RouteLimits::route_edges, D::RouteError::OutputLimit},
        {&D::RouteLimits::copied_id_bytes, D::RouteError::CopiedIdLimit},
        {&D::RouteLimits::storage_bytes, D::RouteError::StorageLimit},
        {&D::RouteLimits::work, D::RouteError::WorkLimit}
    };
    for (const auto& [field, error] : fields) {
        auto short_limit = exact;
        --(short_limit.*field);
        refused(*definition, {"start"}, {"end"}, short_limit, error);
        short_limit.*field = 0;
        refused(*definition, {"start"}, {"end"}, short_limit, error);
    }
    EXPECT_EQ(names(old.nodes), (std::vector<std::string>{"start", "z", "end"}));
    EXPECT_EQ(names(old.choices), (std::vector<std::string>{"c0", "c0"}));
    EXPECT_EQ(definition->nodes()[0].id.value, "end");
}

TEST(dialogue_route, payload) {
    const auto definition = literal();
    const auto route = D::find_route(*definition, {"start"}, {"end"}).value();
    const auto copied = std::string("literalstartzendc0c0").size();
    EXPECT_EQ(route.usage.copied_id_bytes, copied);
    EXPECT_EQ(
        route.usage.storage_bytes,
        3 * 5 * sizeof(std::size_t) + sizeof(D::Route) + 3 * sizeof(D::NodeId) + 2 * sizeof(D::ChoiceId) + copied + 6
    );
    // Copy work fails after graph traversal while a minimum-sized scratch allowance
    // passes. Old results stay independently owned under either refusal.
    D::RouteLimits limits;
    limits.storage_bytes = route.usage.storage_bytes - 1;
    refused(*definition, {"start"}, {"end"}, limits, D::RouteError::StorageLimit);
    limits = {};
    limits.copied_id_bytes = std::string("literalstart").size();
    refused(*definition, {"start"}, {"end"}, limits, D::RouteError::CopiedIdLimit);
    EXPECT_EQ(route.definition.value, "literal");
}

TEST(dialogue_route, invalid_limits) {
    const auto definition = literal();
    ASSERT_TRUE(D::find_route(*definition, {"start"}, {"end"}, D::maximum_route_limits));
    for (const auto field :
         {&D::RouteLimits::nodes, &D::RouteLimits::choices, &D::RouteLimits::route_edges, &D::RouteLimits::copied_id_bytes,
          &D::RouteLimits::storage_bytes, &D::RouteLimits::work}) {
        D::RouteLimits limits;
        limits.*field = D::maximum_route_limits.*field + 1;
        refused(*definition, {"absent"}, {"end"}, limits, D::RouteError::InvalidLimits);
        limits.*field = std::numeric_limits<std::size_t>::max();
        refused(*definition, {"start"}, {"end"}, limits, D::RouteError::InvalidLimits);
    }
}

TEST(dialogue_route, larger) {
    D::Draft draft{{"larger"}, 1, {"n256"}, {}};
    for (std::size_t index = 0; index < 257; ++index)
        draft.nodes.push_back(node("n" + std::to_string(index)));
    auto definition = freeze(draft);
    refused(*definition, {"n256"}, {"n256"}, {}, D::RouteError::NodeLimit);
    D::RouteLimits limits;
    limits.nodes = 257;
    ASSERT_TRUE(D::find_route(*definition, {"n256"}, {"n256"}, limits));
    draft.nodes[0] = node("n0", std::vector<std::string>(2049, "n256"));
    definition = freeze(draft);
    refused(*definition, {"n0"}, {"n256"}, limits, D::RouteError::ChoiceLimit);
    limits.choices = 2049;
    const auto route = D::find_route(*definition, {"n0"}, {"n256"}, limits).value();
    EXPECT_EQ(names(route.nodes), (std::vector<std::string>{"n0", "n256"}));
    EXPECT_EQ(names(route.choices), (std::vector<std::string>{"c0"}));
}

TEST(dialogue_route, lookup_work) {
    const auto graph = [](std::string prefix) {
        return freeze({{"work"}, 1, {prefix + "s"}, {node(prefix + "s", {prefix + "t"}), node(prefix + "t")}});
    };
    const auto short_ids = graph("");
    const auto long_ids = graph(std::string(4096, 'x'));
    const auto early = D::find_route(*short_ids, {"s"}, {"t"}).value();
    const auto late = D::find_route(*long_ids, {std::string(4096, 'x') + "s"}, {std::string(4096, 'x') + "t"}).value();
    EXPECT_GT(late.usage.work, early.usage.work);
    D::RouteLimits limits;
    limits.work = early.usage.work;
    refused(*long_ids, {std::string(4096, 'x') + "s"}, {std::string(4096, 'x') + "t"}, limits, D::RouteError::WorkLimit);
    // Unknown huge query keys also reserve byte work before the allocation-free lookup.
    refused(*short_ids, {std::string(4096, 'x')}, {"t"}, limits, D::RouteError::WorkLimit);
}

TEST(dialogue_route, reference) {
    std::size_t compared = 0;
    for (std::size_t graph = 0; graph < 8; ++graph) {
        D::Draft draft{{"reference"}, 1, {"n5"}, {}};
        for (std::size_t row = 0; row < 8; ++row) {
            const auto index = (row * 3 + 5) % 8;
            std::vector<std::string> targets;
            if ((index + graph) % 5)
                for (std::size_t edge = 0; edge < (index + graph) % 4 + 1; ++edge)
                    targets.push_back("n" + std::to_string((index * 7 + edge * 3 + graph) % 8));
            draft.nodes.push_back(node("n" + std::to_string(index), targets));
        }
        const auto definition = freeze(draft);
        for (std::size_t from = 0; from < 8; ++from)
            for (std::size_t to = 0; to < 8; ++to) {
                const auto expected = reference(draft, from, to);
                const auto actual = D::find_route(*definition, draft.nodes[from].id, draft.nodes[to].id);
                if (expected) {
                    ASSERT_TRUE(actual);
                    EXPECT_EQ(names(actual->nodes), expected->first);
                    EXPECT_EQ(names(actual->choices), expected->second);
                } else {
                    ASSERT_FALSE(actual);
                    EXPECT_EQ(actual.error(), D::RouteError::Unreachable);
                }
                ++compared;
            }
    }
    EXPECT_EQ(compared, 512);
}
