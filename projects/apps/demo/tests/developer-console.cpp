#include "developer-console-fixture.h"

#include <limits>

namespace {
    using namespace DemoConsoleTests;
}

TEST(DemoConsole, ScopePriceAndDescriptorRange) {
    auto practice = Contact::practice({23, 41});
    DemoConsole::Model model(practice);
    EXPECT_NE(model.scope().find("Contact practice world | world 23 / branch 41"), std::string_view::npos);
    model.help("set");
    EXPECT_NE(model.text().find("4294967295"), std::string::npos);
    ASSERT_TRUE(model.submit("get practice.heal_price"));
    EXPECT_EQ(model.entries()[0].result.stage, Core::Stage::Queued);
    model.pump();
    ASSERT_EQ(model.entries()[0].result.stage, Core::Stage::Succeeded);
    EXPECT_EQ(number(model.entries()[0].result, 0), 3u);
    EXPECT_EQ(number(model.entries()[0].result, 1), practice.host->price());
    EXPECT_TRUE(std::get<Core::Value::Children>(field(model.entries()[0].result, 2).data).empty());
    ASSERT_TRUE(model.submit("set practice.heal_price 4294967295"));
    model.pump();
    EXPECT_EQ(practice.host->price(), std::numeric_limits<std::uint32_t>::max());
    EXPECT_FALSE(model.submit("set practice.heal_price 4294967296"));
    ASSERT_TRUE(model.failure());
    EXPECT_EQ(model.failure()->code, Core::Code::Range);
    EXPECT_GT(model.failure()->length, 0u);
    EXPECT_EQ(practice.host->price(), std::numeric_limits<std::uint32_t>::max());
}

TEST(DemoConsole, LiteralErrorsAndUnicodeStayData) {
    auto practice = Contact::practice();
    DemoConsole::Model model(practice);
    EXPECT_FALSE(model.submit("missing.command"));
    ASSERT_TRUE(model.failure());
    EXPECT_EQ(model.failure()->code, Core::Code::UnknownCommand);
    EXPECT_FALSE(model.submit("magic.edit \"unfinished"));
    EXPECT_EQ(model.failure()->code, Core::Code::Syntax);
    EXPECT_FALSE(model.submit("get practice.heal_price\nquit"));
    EXPECT_EQ(model.failure()->code, Core::Code::Encoding);
    EXPECT_FALSE(model.submit(std::string{static_cast<char>(0xff)}));
    EXPECT_EQ(model.failure()->code, Core::Code::Encoding);
    const std::string unsafe_source = "<img src='x'>\xe6\xb0\xb4\xf0\x9f\x8c\xb1";
    ASSERT_TRUE(model.submit("magic.edit \"" + unsafe_source + "\""));
    model.pump();
    EXPECT_EQ(std::get<std::string>(field(model.entries().back().result, 2).data), unsafe_source);
    EXPECT_NE(DemoConsole::escaped(model.text()).find("&lt;img"), std::string::npos);
    ASSERT_TRUE(model.submit("magic.draft.cast marin"));
    model.pump();
    EXPECT_EQ(model.entries().back().result.stage, Core::Stage::Failed);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(2))->health, 12u);
    EXPECT_EQ(DemoConsole::escaped("<&\"'>"), "&lt;&amp;&quot;&#39;&gt;");
    EXPECT_EQ(DemoConsole::display_text("x\x1b[0my"), "x[control][0my");
}

TEST(DemoConsole, PendingOwnsExactActionAndAcceptedDraft) {
    auto practice = Contact::practice();
    DemoConsole::Model model(practice);
    ASSERT_TRUE(model.submit("magic.edit \"" + source(practice, 5) + "\""));
    model.pump();
    ASSERT_TRUE(model.submit("magic.quote marin"));
    model.pump();
    EXPECT_EQ(number(model.entries()[1].result, 4), 3u);
    ASSERT_TRUE(model.submit("magic.draft.cast marin"));
    model.pump();
    ASSERT_EQ(model.entries()[2].result.stage, Core::Stage::Pending);
    const auto pending = model.entries()[2];
    ASSERT_TRUE(model.submit("magic.edit \"" + source(practice, 12) + "\""));
    model.pump();
    EXPECT_EQ(pending.result.stage, Core::Stage::Pending);
    ASSERT_EQ(model.entries()[2].result.stage, Core::Stage::Succeeded);
    EXPECT_EQ(number(model.entries()[2].result, 2), 5u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(2))->health, 17u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, 7u);
    ASSERT_TRUE(model.submit("operation.read op1"));
    model.pump();
    EXPECT_EQ(model.entries().back().result.value, model.entries()[2].result.value);
    EXPECT_EQ(model.entries()[2].id, 3u);
    model.close();
    model.pump();
    EXPECT_EQ(number(model.entries()[2].result, 2), 5u);
}

TEST(DemoConsole, OwnedResultsSurviveControllerAndAdapterTeardown) {
    auto practice = Contact::practice();
    Core::Result retained;
    {
        DemoConsole::Model model(practice);
        ASSERT_TRUE(model.submit("get practice.heal_price"));
        model.pump();
        retained = model.entries()[0].result;
    }
    practice.console->close();
    EXPECT_EQ(retained.stage, Core::Stage::Succeeded);
    EXPECT_EQ(number(retained, 1), 3u);
}

TEST(DemoConsole, HistoryCompletionAndVisibleCapacity) {
    auto practice = Contact::practice();
    DemoConsole::Model model(practice);
    for (unsigned index = 0; index < 40; ++index)
        EXPECT_FALSE(model.submit("missing" + std::to_string(index)));
    EXPECT_EQ(model.history().size(), DemoConsole::Model::history_count);
    EXPECT_NE(model.text().find("history truncated"), std::string::npos);
    EXPECT_EQ(model.previous("unsubmitted draft"), "missing39");
    EXPECT_EQ(model.previous("ignored"), "missing38");
    EXPECT_EQ(model.next(), "missing39");
    EXPECT_EQ(model.next(), "unsubmitted draft");
    const auto candidates = model.suggestions("magic.draft.cast target=\"mar");
    EXPECT_NE(std::ranges::find(candidates, "marin"), candidates.end());
    EXPECT_EQ(model.complete("magic.draft.cast target=\"mar"), "magic.draft.cast target=marin ");
    EXPECT_LE(model.suggestions("").size(), DemoConsole::Model::suggestion_count);
    EXPECT_TRUE(model.entries().empty());
    for (std::size_t index = 0; index < Core::max_requests; ++index) {
        ASSERT_TRUE(model.submit("get practice.heal_price"));
        model.pump();
    }
    EXPECT_FALSE(model.submit("get practice.heal_price"));
    EXPECT_EQ(model.failure()->code, Core::Code::Capacity);
    EXPECT_NE(model.text().find("retained IDs are never evicted"), std::string::npos);
    EXPECT_EQ(model.entries().size(), Core::max_requests);
}

TEST(DemoConsole, SaturatedRenderingKeepsLatestHostResultAndCurrentNotice) {
    auto practice = Contact::practice();
    DemoConsole::Model model(practice);
    const std::string query = "get practice.heal_price";
    const auto padded = std::string(DemoConsole::Model::command_bytes - query.size(), ' ') + query;
    ASSERT_EQ(padded.size(), DemoConsole::Model::command_bytes);
    for (std::size_t index = 0; index < 16; ++index) {
        ASSERT_TRUE(model.submit(padded));
        model.pump();
        ASSERT_EQ(model.entries().back().result.stage, Core::Stage::Succeeded);
    }
    ASSERT_TRUE(model.submit("set practice.heal_price 123456789"));
    const auto queued = model.text();
    EXPECT_NE(queued.find("#17 > set practice.heal_price 123456789\nTransport: queued"), std::string::npos);
    model.pump();
    ASSERT_EQ(practice.host->price(), 123456789u);
    ASSERT_EQ(model.entries().back().result.stage, Core::Stage::Succeeded);
    EXPECT_EQ(number(model.entries().back().result, 1), 123456789u);
    const auto expect_current = [&] {
        const auto output = model.text();
        EXPECT_LE(output.size(), DemoConsole::Model::output_bytes);
        EXPECT_TRUE(Core::valid_utf8(output));
        EXPECT_NE(output.find("output truncated: showing recent entries"), std::string::npos);
        const auto latest = output.find("#17 > set practice.heal_price 123456789\nTransport: succeeded");
        EXPECT_NE(latest, std::string::npos);
        EXPECT_NE(output.find("effective: 123456789", latest), std::string::npos);
        EXPECT_LT(output.find("#16 >"), latest);
        return output;
    };
    expect_current();
    model.help("set");
    EXPECT_NE(expect_current().find("4294967295"), std::string::npos);
    const std::string unicode = "\xe6\xb0\xb4\xf0\x9f\x8c\xb1";
    EXPECT_FALSE(model.submit("missing.command \"<" + unicode + ">\""));
    const auto refused = expect_current();
    EXPECT_NE(refused.find("Refused: unknown command"), std::string::npos);
    EXPECT_NE(refused.find(unicode), std::string::npos);
    EXPECT_NE(DemoConsole::escaped(refused).find("&lt;" + unicode + "&gt;"), std::string::npos);
    ASSERT_EQ(model.entries().size(), 17u);
    for (std::size_t index = 0; index < 16; ++index) {
        EXPECT_EQ(model.entries()[index].id, index + 1);
        EXPECT_EQ(model.entries()[index].line, padded);
        EXPECT_EQ(number(model.entries()[index].result, 1), 3u);
    }
    EXPECT_EQ(model.entries().back().id, 17u);
}

TEST(DemoConsole, ByteBoundsAndQueuedClose) {
    auto practice = Contact::practice();
    DemoConsole::Model model(practice);
    EXPECT_FALSE(model.submit(std::string(DemoConsole::Model::command_bytes + 1, 'x')));
    EXPECT_EQ(model.failure()->code, Core::Code::Limit);
    EXPECT_TRUE(model.history().empty());
    for (std::size_t index = 0; index < Core::max_requests; ++index) {
        ASSERT_TRUE(model.submit("magic.edit \"" + std::string(Core::max_text, '<') + "\""));
        model.pump();
    }
    std::size_t history_size = 0;
    for (const auto& line : model.history())
        history_size += line.size();
    EXPECT_LE(history_size, DemoConsole::Model::history_bytes);
    EXPECT_LE(model.text().size(), DemoConsole::Model::output_bytes);
    EXPECT_TRUE(Core::valid_utf8(model.text()));
    model.close();
    EXPECT_FALSE(model.accepting());
    auto second_practice = Contact::practice();
    DemoConsole::Model queued(second_practice);
    ASSERT_TRUE(queued.submit("magic.cast S1 marin"));
    queued.close();
    queued.pump();
    EXPECT_EQ(queued.entries()[0].result.stage, Core::Stage::Cancelled);
    EXPECT_EQ(second_practice.host->actor(second_practice.access, second_practice.host->entity(2))->health, 12u);
}

#if defined(__unix__) || defined(__APPLE__)
TEST(DemoConsole, RealScenarioDispositionIsNotTransportSuccess) {
    Files files;
    auto practice = Contact::practice();
    DemoConsole::Model model(practice, files.config());
    ASSERT_TRUE(model.submit("test.run wrong-assertion.console failure"));
    model.pump();
    EXPECT_EQ(model.entries().back().result.stage, Core::Stage::Pending);
    settle(model);
    ASSERT_EQ(model.entries().back().result.stage, Core::Stage::Succeeded);
    EXPECT_EQ(std::get<std::string>(field(model.entries().back().result, 0).data), "failed");
    EXPECT_TRUE(std::get<bool>(field(model.entries().back().result, 2).data));
    EXPECT_NE(model.text().find("disposition: \"failed\""), std::string::npos);
    ASSERT_TRUE(model.submit("test.run authoring.console authoring"));
    settle(model);
    ASSERT_EQ(model.entries().back().result.stage, Core::Stage::Succeeded);
    EXPECT_EQ(std::get<std::string>(field(model.entries().back().result, 0).data), "passed");
    EXPECT_TRUE(std::filesystem::is_regular_file(files.root / "authoring" / "report.json"));
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(2))->health, 12u);
}

TEST(DemoConsole, InteractiveCloseRetainsExternalService) {
    Files files;
    auto practice = Contact::practice();
    {
        DemoConsole::Model model(practice, files.config());
        ASSERT_TRUE(model.submit("test.run authoring.console close"));
        model.pump();
        model.close();
        model.pump();
        EXPECT_TRUE(Core::terminal(model.entries().back().result.stage));
    }
    files.io->shutdown();
    files.io->settle();
    EXPECT_TRUE(files.io->settled());
}

TEST(DemoConsole, ApplicationRetainsControllerUntilExternalSettlement) {
    DemoConsole::Scenarios::Config config;
    DemoConsole::Application application(config);
    ASSERT_TRUE(application.model().submit("magic.cast S1 marin"));
    application.model().pump();
    EXPECT_EQ(application.model().entries()[0].result.stage, Core::Stage::Pending);
    application.stop();
    application.finish();
    EXPECT_TRUE(Core::terminal(application.model().entries()[0].result.stage));
    EXPECT_EQ(application.model().entries()[0].id, 1u);
}
#endif
