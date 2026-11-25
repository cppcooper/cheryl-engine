#include <cheryl/tools/console/setting.h>
#include <cheryl/tools/console/parser.h>
#include <cheryl/tools/console/session.h>
#include <gtest/gtest.h>

namespace CE::Tools::Console {
    TEST(ConsoleCore, SettingTypedAndTextRetainOneOwnerEffect) {
        Authority authority;
        const auto context = authority.issue(3);
        Registry registry;
        std::uint64_t current = 4, writes = 0;
        UnsignedSetting setting(registry, "simulation.limit", "Owner tick limit", 4, 16,
            {[&] { return current; }, [&](std::uint64_t value) { current = value; ++writes; return true; }}, 1, 2);
        auto typed = registry.bind("simulation.limit.set", {Value::unsigned_integer(6)}, context);
        auto text = bind_text(registry, context, "simulation.limit.set 6");
        ASSERT_TRUE(typed); ASSERT_TRUE(text);
        EXPECT_TRUE(typed->same_body(*text));
        Session session(context);
        ASSERT_TRUE(session.submit(1, *typed));
        EXPECT_EQ(current, 4u);
        ASSERT_TRUE(session.pump());
        ASSERT_TRUE(session.submit(1, *text));
        ASSERT_TRUE(session.pump());
        EXPECT_EQ(current, 6u); EXPECT_EQ(writes, 1u);
        EXPECT_FALSE(bind_text(registry, context, "simulation.limit.set 17"));
        EXPECT_EQ(current, 6u);
        const auto result = session.lookup(1);
        ASSERT_TRUE(result);
        EXPECT_EQ((*result)->stage, Stage::Succeeded);
        const auto& fields = std::get<Value::Children>((*result)->value.data);
        EXPECT_EQ(std::get<std::uint64_t>(fields[0].data), 4u);
        EXPECT_EQ(std::get<std::uint64_t>(fields[1].data), 6u);
        session.close();
        EXPECT_EQ((*result)->stage, Stage::Succeeded);
    }
    TEST(ConsoleCore, SettingObserverCannotWriteAndOwnerCanRefuse) {
        Authority authority;
        const auto gameplay = authority.issue(3), observer = authority.issue(1);
        Registry registry;
        std::uint64_t current = 4;
        UnsignedSetting setting(registry, "simulation.limit", "Owner tick limit", 4, 16,
            {[&] { return current; }, [](std::uint64_t) { return false; }}, 1, 2);
        EXPECT_FALSE(registry.bind("simulation.limit.set", {Value::unsigned_integer(6)}, observer));
        EXPECT_TRUE(registry.describe("simulation.limit.read", observer));
        const auto request = bind_text(registry, gameplay, "simulation.limit.set 6");
        ASSERT_TRUE(request);
        Session session(gameplay);
        ASSERT_TRUE(session.submit(1, *request)); ASSERT_TRUE(session.pump());
        const auto result = session.lookup(1);
        ASSERT_TRUE(result); EXPECT_EQ((*result)->stage, Stage::Refused);
        EXPECT_EQ(current, 4u);
    }
    TEST(ConsoleLifetime, SettingRetirementRejectsUnadmittedRequest) {
        Authority authority;
        const auto context = authority.issue(3);
        Registry registry;
        std::uint64_t writes = 0;
        auto setting = std::make_unique<UnsignedSetting>(registry, "simulation.limit", "Owner tick limit", 4, 16,
            UnsignedSettingOwner{[] { return 4u; }, [&](std::uint64_t) { ++writes; return true; }}, 1, 2);
        auto request = bind_text(registry, context, "simulation.limit.set 6");
        ASSERT_TRUE(request);
        setting.reset();
        Session session(context);
        EXPECT_FALSE(session.submit(1, *request));
        EXPECT_EQ(writes, 0u);
    }
}
