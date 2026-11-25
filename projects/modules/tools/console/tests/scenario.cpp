#include <cheryl/tools/console/scenario.h>
#include <gtest/gtest.h>
#include <stdexcept>

namespace {
    namespace Core = CE::Tools::Console;
    namespace Scenario = Core::Scenario;
    class CounterWork final : public Core::Work {
        std::size_t& calls_;
    public:
        explicit CounterWork(std::size_t& calls) : calls_(calls) {}
        void step() noexcept override {
            ++calls_; result.value = Core::Value::unsigned_integer(8); result.stage = Core::Stage::Succeeded;
        }
        void close() noexcept override { result.stage = Core::Stage::Cancelled; }
    };
    struct Fixture {
        Core::Authority authority;
        Core::Context context = authority.issue(1);
        Core::Registry registry;
        std::size_t calls = 0;
        std::size_t entries = 0;
        Core::Registration registration;
        Fixture() {
            Core::Descriptor descriptor;
            descriptor.name = "count"; descriptor.domain = "test"; descriptor.rights = 1;
            descriptor.result.type = Core::Type::Unsigned;
            auto installed = registry.add<>(descriptor, [this](const auto&) {
                ++entries;
                return std::make_shared<CounterWork>(calls);
            });
            if (!installed) throw std::runtime_error("Scenario count fixture registration failed: code " +
                std::to_string(static_cast<int>(installed.error().code)));
            registration = std::move(*installed);
        }
    };
    Scenario::Disposition finish(Scenario::Run& run) {
        for (std::size_t step = 0; step < Scenario::max_statements + Scenario::max_steps + 1; ++step) {
            if (run.step() != Scenario::Disposition::Running) return run.disposition();
        }
        return Scenario::Disposition::Running;
    }
}

TEST(ConsoleScenario, RequiresExplicitRegistrationDomain) {
    Fixture fixture;
    auto descriptor = fixture.registry.describe("count", fixture.context);
    ASSERT_TRUE(descriptor);
    EXPECT_EQ(descriptor->domain, "test");
    descriptor->name = "invalid"; descriptor->domain.clear();
    auto installed = fixture.registry.add<>(*descriptor, [&fixture](const auto&) {
        return std::make_shared<CounterWork>(fixture.calls);
    });
    ASSERT_FALSE(installed);
    EXPECT_EQ(installed.error().code, Core::Code::Schema);
    EXPECT_EQ(fixture.calls, 0u);
}

TEST(ConsoleScenario, ValidatesEverythingBeforeCallingOwner) {
    Fixture fixture;
    for (const auto* tail : {"unknown -> later\nfinish", "expect $first.missing == 8\nfinish",
             "expect $first == false\nfinish", "test.run x y -> bad\nfinish", "finish", "expect $first == 8"}) {
        const auto source = std::string("script.version 1\nfixture.load unit\ncount -> first\n") + tail;
        EXPECT_FALSE(Scenario::prepare(fixture.registry, fixture.context, source));
        EXPECT_EQ(fixture.calls, 0u);
    }
}

TEST(ConsoleScenario, PreparesUnquotedEqualityWithoutCallingOwner) {
    Fixture fixture;
    constexpr std::string_view source =
        "script.version 1\nfixture.load unit\ncount -> result\nexpect $result == 8\nfinish\n";
    auto legacy = Scenario::prepare(fixture.registry, fixture.context, source);
    ASSERT_TRUE(legacy); EXPECT_EQ(legacy->assertions(), 1u); EXPECT_EQ(legacy->fixture(), "unit");
    Core::Session session(fixture.context);
    Scenario::Channels channels{{fixture.context, session}, std::nullopt};
    auto routed = Scenario::prepare(fixture.registry, channels, source);
    ASSERT_TRUE(routed); EXPECT_EQ(routed->assertions(), 1u); EXPECT_EQ(routed->fixture(), "unit");
    for (const auto* assertion : {"expect $result = 8", "expect $result === 8", "expect $result ==8",
             "expect $result ==", "expect $result == 8 extra", "expect $result == false"}) {
        SCOPED_TRACE(assertion);
        const auto invalid = std::string("script.version 1\nfixture.load unit\ncount -> result\n") + assertion + "\nfinish\n";
        EXPECT_FALSE(Scenario::prepare(fixture.registry, fixture.context, invalid));
        EXPECT_FALSE(Scenario::prepare(fixture.registry, channels, invalid));
    }
    EXPECT_EQ(fixture.entries, 0u); EXPECT_EQ(fixture.calls, 0u); EXPECT_EQ(session.retained(), 0u);
}

TEST(ConsoleScenario, RunsExactRetryAndStrictLiteral) {
    Fixture fixture;
    auto plan = Scenario::prepare(fixture.registry, fixture.context,
        "script.version 1\nfixture.load unit\ncount -> first\nretry $first -> duplicate\n"
        "await $duplicate max_steps=1 -> final\nexpect $final == 8\nfinish\n");
    ASSERT_TRUE(plan);
    Core::Session session(fixture.context);
    Scenario::Run run(*plan, fixture.registry, session);
    EXPECT_EQ(finish(run), Scenario::Disposition::Passed);
    EXPECT_EQ(fixture.calls, 1u);
    ASSERT_EQ(run.results().size(), 3u);
    EXPECT_EQ(run.results()[0].request_id, run.results()[2].request_id);
    EXPECT_EQ(session.retained(), 1u);
}

TEST(ConsoleScenario, WrongAssertionFailsWithoutRollback) {
    Fixture fixture;
    auto plan = Scenario::prepare(fixture.registry, fixture.context,
        "script.version 1\nfixture.load unit\ncount -> first\nexpect $first == 9\nfinish").value();
    Core::Session session(fixture.context);
    Scenario::Run run(plan, fixture.registry, session);
    EXPECT_EQ(finish(run), Scenario::Disposition::Failed);
    EXPECT_EQ(fixture.calls, 1u);
    ASSERT_EQ(run.assertions().size(), 1u);
    EXPECT_EQ(std::get<std::uint64_t>(run.assertions()[0].actual.data), 8u);
    EXPECT_EQ(std::get<std::uint64_t>(run.assertions()[0].expected.data), 9u);
    const auto failure = run.error();
    run.close(Scenario::Disposition::Incomplete);
    EXPECT_EQ(run.disposition(), Scenario::Disposition::Incomplete);
    EXPECT_EQ(run.error().failure.code, failure.failure.code);
    EXPECT_EQ(run.error().line, failure.line);
    EXPECT_EQ(fixture.calls, 1u);
}

TEST(ConsoleScenario, KeepsBoundGenerationAcrossReplacement) {
    Fixture fixture;
    auto plan = Scenario::prepare(fixture.registry, fixture.context,
        "script.version 1\nfixture.load unit\ncount -> first\nexpect $first == 8\nfinish").value();
    fixture.registration.reset();
    Core::Session session(fixture.context);
    Scenario::Run run(plan, fixture.registry, session);
    EXPECT_EQ(finish(run), Scenario::Disposition::Incomplete);
    EXPECT_EQ(fixture.calls, 0u);
}

TEST(ConsoleScenario, RejectsForeignAuthorityAndBounds) {
    Fixture fixture;
    EXPECT_FALSE(Scenario::prepare(fixture.registry, fixture.authority.issue(0),
        "script.version 1\nfixture.load unit\ncount -> first\nexpect $first == 8\nfinish"));
    EXPECT_FALSE(Scenario::prepare(fixture.registry, fixture.context, std::string(Scenario::max_file_bytes + 1, 'a')));
    EXPECT_FALSE(Scenario::prepare(fixture.registry, fixture.context,
        "script.version 1\nfixture.load unit\ncount -> first\nawait $first max_steps=257 -> final\nexpect $final == 8\nfinish"));
}
