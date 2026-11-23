#include <cheryl/core/game-framework/dialogue-inspection.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <limits>
#include <memory>

#ifdef DIALOGUE_INSPECTION_ENGINE
#include <dlfcn.h>
#endif
#ifdef CE_BUILDING_ENGINE
#error This consumer must import the real Engine, not export or copy its implementation.
#endif

namespace {
    namespace D = CE::Dialogue;
    D::DefinitionHandle definition() {
        return D::compile(
                   {{"public"},
                    9,
                    {"loop"},
                    {{{"end"}, "", D::NodeKind::Terminal, {}}, {{"loop"}, "", D::NodeKind::Prompt, {{{"repeat"}, "", {"loop"}, {}, {}}}}}},
                   {}
        )
            .value();
    }
}

TEST(dialogue_inspection_consumer, public_header) {
    const auto frozen = definition();
    const auto inspected = D::inspect(*frozen, D::InspectionLimits{});
    ASSERT_TRUE(inspected);
    EXPECT_EQ(inspected->definition.value, "public");
    EXPECT_EQ(inspected->revision, 9);
    EXPECT_EQ(inspected->entry.value, "loop");
    EXPECT_EQ(inspected->nodes, 2);
    EXPECT_EQ(inspected->choices, 1);
    EXPECT_EQ(inspected->terminals, 1);
    ASSERT_EQ(inspected->reachable.size(), 1);
    EXPECT_EQ(inspected->reachable[0].value, "loop");
    EXPECT_EQ(inspected->unreachable[0].value, "end");
    EXPECT_EQ(inspected->no_terminal_route[0].value, "loop");
#ifdef DIALOGUE_INSPECTION_ENGINE
    Dl_info owner{};
    ASSERT_NE(dladdr(reinterpret_cast<const void*>(&D::inspect), &owner), 0);
    ASSERT_NE(owner.dli_fname, nullptr);
    EXPECT_TRUE(std::filesystem::equivalent(owner.dli_fname, DIALOGUE_INSPECTION_ENGINE));
#endif
}

TEST(dialogue_inspection_consumer, retained) {
    D::InspectionReport owned;
    std::weak_ptr<const D::Definition> weak;
    {
        const auto frozen = definition();
        weak = frozen;
        owned = D::inspect(*frozen).value();
    }
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(owned.entry.value, "loop");
    EXPECT_EQ(owned.no_terminal_route[0].value, "loop");
    EXPECT_GT(owned.usage.copied_id_bytes, 0);
}

TEST(dialogue_inspection_consumer, typed_limits) {
    const auto frozen = definition();
    const auto old = D::inspect(*frozen).value();
    D::InspectionLimits limits;
    limits.work = old.usage.work - 1;
    const auto failed = D::inspect(*frozen, limits);
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), D::InspectionError::WorkLimit);
    limits.work = old.usage.work;
    ASSERT_TRUE(D::inspect(*frozen, limits));
    limits.nodes = std::numeric_limits<std::size_t>::max();
    const auto invalid = D::inspect(*frozen, limits);
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error(), D::InspectionError::InvalidLimits);
    EXPECT_EQ(old.definition.value, "public");
    EXPECT_EQ(old.no_terminal_route[0].value, "loop");
}
