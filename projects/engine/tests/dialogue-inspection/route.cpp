#include <cheryl/core/game-framework/dialogue-route.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <limits>
#include <memory>
#include <type_traits>

#ifdef DIALOGUE_INSPECTION_ENGINE
#include <dlfcn.h>
#endif
#ifdef CE_BUILDING_ENGINE
#error This consumer must import actual Engine route code.
#endif

namespace {
    namespace D = CE::Dialogue;
    D::DefinitionHandle graph() {
        return D::compile(
                   {{"route-consumer"},
                    3,
                    {"start"},
                    {{{"end"}, "", D::NodeKind::Terminal, {}}, {{"start"}, "", D::NodeKind::Prompt, {{{"go"}, "", {"end"}, {}, {}}}}}},
                   {}
        )
            .value();
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
}

TEST(dialogue_route_consumer, caller) {
    static_assert(!std::is_invocable_v<
                  decltype(&D::find_route), const D::DefinitionHandle&, const D::NodeId&, const D::NodeId&, const D::RouteLimits&>);
    const auto definition = graph();
    const auto route = D::find_route(*definition, {"start"}, {"end"}, D::RouteLimits{});
    ASSERT_TRUE(route);
    EXPECT_EQ(route->definition.value, "route-consumer");
    EXPECT_EQ(route->revision, 3);
    ASSERT_EQ(route->nodes.size(), 2);
    EXPECT_EQ(route->nodes[0].value, "start");
    EXPECT_EQ(route->nodes[1].value, "end");
    ASSERT_EQ(route->choices.size(), 1);
    EXPECT_EQ(route->choices[0].value, "go");
    const auto* choice = definition->find_choice(route->nodes[0], route->choices[0]);
    ASSERT_NE(choice, nullptr);
    EXPECT_EQ(choice->target, route->nodes[1]);
#ifdef DIALOGUE_INSPECTION_ENGINE
    Dl_info owner{};
    ASSERT_NE(dladdr(reinterpret_cast<const void*>(&D::find_route), &owner), 0);
    ASSERT_NE(owner.dli_fname, nullptr);
    EXPECT_TRUE(std::filesystem::equivalent(owner.dli_fname, DIALOGUE_INSPECTION_ENGINE));
#endif
}

TEST(dialogue_route_consumer, retained) {
    D::Route owned;
    std::weak_ptr<const D::Definition> weak;
    {
        const auto definition = graph();
        weak = definition;
        owned = D::find_route(*definition, {"start"}, {"end"}).value();
    }
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(owned.definition.value, "route-consumer");
    EXPECT_EQ(owned.nodes[0].value, "start");
    EXPECT_EQ(owned.choices[0].value, "go");
}

TEST(dialogue_route_consumer, refusals) {
    const auto definition = graph();
    const auto old = D::find_route(*definition, {"start"}, {"end"}).value();
    const auto exact = D::RouteLimits{2, 1, 1, old.usage.copied_id_bytes, old.usage.storage_bytes, old.usage.work};
    ASSERT_TRUE(D::find_route(*definition, {"start"}, {"end"}, exact));
    auto short_limit = exact;
    --short_limit.work;
    refused(*definition, {"start"}, {"end"}, short_limit, D::RouteError::WorkLimit);
    short_limit = exact;
    short_limit.nodes = std::numeric_limits<std::size_t>::max();
    refused(*definition, {"start"}, {"end"}, short_limit, D::RouteError::InvalidLimits);
    refused(*definition, {"unknown"}, {"end"}, {}, D::RouteError::UnknownFrom);
    refused(*definition, {"start"}, {"unknown"}, {}, D::RouteError::UnknownTo);
    refused(*definition, {"end"}, {"start"}, {}, D::RouteError::Unreachable);
    EXPECT_EQ(old.nodes[0].value, "start");
    EXPECT_EQ(old.choices[0].value, "go");
}
