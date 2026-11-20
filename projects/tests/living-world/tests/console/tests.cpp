#include <tools/console/fixture.h>
#include <tests/host/access.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <limits>

namespace {
    namespace Living = CE::LivingWorld;
    namespace Console = Living::Console;
    namespace Core = CE::Tools::Console;
    const Core::Value& field(const Core::Result& result, std::size_t index) {
        return std::get<Core::Value::Children>(result.value.data).at(index);
    }
    std::uint64_t number(const Core::Result& result, std::size_t index) {
        return std::get<std::uint64_t>(field(result, index).data);
    }
    Core::Result query(Console::Practice& practice, Console::Client& client, std::uint64_t id, std::string_view line) {
        auto request = Core::bind_text(practice.console->registry(), client.context(), line).value();
        client.session().submit(id, request).value();
        client.session().pump().value();
        return *client.session().lookup(id).value();
    }
    auto image(const Console::Practice& practice) { return Living::HostTestAccess::image(*practice.host); }
}

TEST(ConsoleHost, TypedAndTextShareActualContactAndDeliverWrenEvidence) {
    auto practice = Console::practice();
    auto client = practice.console->open(Console::Mode::PracticeObserver);
    auto target = client->context().resolve("marin", "actor"); ASSERT_TRUE(target);
    auto typed = practice.console->registry().bind("magic.cast", {Core::Value::enumeration(0), Core::Value::handle(*target)}, client->context());
    auto text = Core::bind_text(practice.console->registry(), client->context(), "magic.cast spell=S1 target=\"marin\"");
    ASSERT_TRUE(typed); ASSERT_TRUE(text); EXPECT_TRUE(typed->same_body(*text));
    ASSERT_TRUE(client->session().submit(1, *typed));
    EXPECT_EQ((*client->session().lookup(1))->stage, Core::Stage::Queued);
    EXPECT_EQ(image(practice).record_count, 0u);
    ASSERT_TRUE(client->session().pump());
    EXPECT_EQ((*client->session().lookup(1))->stage, Core::Stage::Pending);
    ASSERT_EQ(image(practice).record_count, 1u);
    const auto action = image(practice).records[0].action;
    ASSERT_TRUE(client->session().submit(1, *text));
    ASSERT_TRUE(client->session().pump());
    const auto result = *client->session().lookup(1).value();
    EXPECT_EQ(result.stage, Core::Stage::Succeeded); EXPECT_EQ(number(result, 2), 8u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(2))->health, 20u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, 7u);
    EXPECT_EQ(practice.host->lookup(practice.access, action)->state, Living::Disposition::Applied);
    EXPECT_EQ(number(result, 4), 1u);
    auto wren = practice.host->read(practice.host->grant(practice.host->entity(3))); ASSERT_TRUE(wren);
    ASSERT_EQ(wren->size, 1u); EXPECT_EQ(wren->wound_closures[0].subject, practice.host->entity(2));
    EXPECT_EQ(practice.host->deliver().adopted, 0u);
    const auto operation = query(practice, *client, 2, "operation.read op1");
    EXPECT_EQ(operation.value, result.value);
}

TEST(ConsoleHost, PureParseCompletionHelpAndInspectNeverIssueAttempt) {
    auto practice = Console::practice(); auto client = practice.console->open(Console::Mode::PracticeObserver);
    const auto before = image(practice);
    EXPECT_TRUE(Core::parse("magic.cast S1 target=\"marin\""));
    auto suggestions = Core::complete(practice.console->registry(), client->context(), "magic.cast S1 target=\"ma");
    ASSERT_TRUE(suggestions); EXPECT_NE(std::ranges::find(*suggestions, "marin"), suggestions->end());
    EXPECT_TRUE(Core::bind_text(practice.console->registry(), client->context(), "magic.cast S1 marin"));
    EXPECT_EQ(query(practice, *client, 1, "help magic.cast").stage, Core::Stage::Succeeded);
    EXPECT_EQ(query(practice, *client, 2, "commands").stage, Core::Stage::Succeeded);
    const auto inspected = query(practice, *client, 3, "magic.inspect S1");
    EXPECT_EQ(inspected.stage, Core::Stage::Succeeded);
    EXPECT_EQ(std::get<std::string>(field(inspected, 0).data), practice.plan->source());
    EXPECT_EQ(std::get<std::string>(field(inspected, 1).data), practice.plan->inspection());
    EXPECT_EQ(image(practice).revision, before.revision); EXPECT_EQ(image(practice).record_count, before.record_count);
    auto cast = Core::bind_text(practice.console->registry(), client->context(), "magic.cast S1 marin").value();
    ASSERT_TRUE(client->session().submit(4, cast)); ASSERT_TRUE(client->session().pump());
    ASSERT_EQ(image(practice).record_count, 1u); EXPECT_EQ(image(practice).records[0].action.key.sequence, 1u);
}

TEST(ConsoleHost, ActualSettingReadbackAndCheckedFullUint32Domain) {
    auto practice = Console::practice(); auto client = practice.console->open(Console::Mode::PracticeObserver);
    auto initial = query(practice, *client, 1, "get practice.heal_price");
    EXPECT_EQ(number(initial, 0), 3u); EXPECT_EQ(number(initial, 1), 3u);
    EXPECT_TRUE(std::get<Core::Value::Children>(field(initial, 2).data).empty());
    EXPECT_EQ(std::get<Core::Enum>(field(initial, 3).data).index, 0u);
    ASSERT_TRUE(practice.host->set_price(11));
    EXPECT_EQ(number(query(practice, *client, 2, "get practice.heal_price"), 1), 11u);
    EXPECT_EQ(number(query(practice, *client, 3, "set practice.heal_price 4294967295"), 1), 4294967295ULL);
    EXPECT_EQ(practice.host->price(), std::numeric_limits<std::uint32_t>::max());
    const auto revision = image(practice).revision;
    for (const auto* line : {"set practice.heal_price -1", "set practice.heal_price 4294967296",
            "set practice.heal_price 1.5", "set practice.heal_price NaN", "set practice.heal_price garbage",
            "set practice.heal_price 3 extra", "set setting=practice.heal_price setting=practice.heal_price"}) {
        EXPECT_FALSE(Core::bind_text(practice.console->registry(), client->context(), line)) << line;
    }
    EXPECT_EQ(image(practice).revision, revision);
    EXPECT_EQ(practice.host->price(), std::numeric_limits<std::uint32_t>::max());
    auto changed = query(practice, *client, 4, "set practice.heal_price 0");
    EXPECT_EQ(changed.stage, Core::Stage::Succeeded); EXPECT_EQ(practice.host->price(), 0u);
    changed = query(practice, *client, 5, "set practice.heal_price 5");
    EXPECT_EQ(changed.stage, Core::Stage::Succeeded);
    auto cast = Core::bind_text(practice.console->registry(), client->context(), "magic.cast S1 marin").value();
    ASSERT_TRUE(client->session().submit(6, cast)); ASSERT_TRUE(client->session().pump()); ASSERT_TRUE(client->session().pump());
    EXPECT_EQ((*client->session().lookup(6))->stage, Core::Stage::Succeeded);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, 5u);
}

TEST(ConsoleHost, ValidInsufficientFundsAndUnwillingSetupsRefuseUnchanged) {
    for (const bool insufficient : {false, true}) {
        auto practice = Console::practice(); auto client = practice.console->open(Console::Mode::PracticeObserver);
        auto actor = practice.host->actor(practice.access, practice.host->entity(insufficient ? 1 : 2)).value();
        if (insufficient) actor.reserve = 2; else actor.willing = false;
        ASSERT_TRUE(practice.host->edit_actor(actor));
        const auto before = image(practice);
        auto request = Core::bind_text(practice.console->registry(), client->context(), "magic.cast S1 marin").value();
        ASSERT_TRUE(client->session().submit(1, request)); ASSERT_TRUE(client->session().pump()); ASSERT_TRUE(client->session().pump());
        const auto result = *client->session().lookup(1).value();
        EXPECT_EQ(result.stage, Core::Stage::Refused); EXPECT_EQ(number(result, 2), 0u);
        EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, before.actors[0].reserve);
        EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(2))->health, 12u);
        EXPECT_EQ(image(practice).work_count, 0u);
    }
}

TEST(ConsoleHost, PendingRetryKeepsExactActionAndStaleQuoteRefusesWithoutEffects) {
    auto practice = Console::practice(); auto client = practice.console->open(Console::Mode::PracticeObserver);
    auto request = Core::bind_text(practice.console->registry(), client->context(), "magic.cast S1 marin").value();
    ASSERT_TRUE(client->session().submit(1, request)); ASSERT_TRUE(client->session().pump());
    const auto exact = image(practice).records[0].action;
    ASSERT_TRUE(practice.host->set_price(9));
    ASSERT_TRUE(client->session().submit(1, request)); EXPECT_EQ((*client->session().lookup(1))->stage, Core::Stage::Pending);
    ASSERT_TRUE(client->session().pump());
    const auto result = *client->session().lookup(1).value(); EXPECT_EQ(result.stage, Core::Stage::Refused);
    EXPECT_EQ(std::get<Core::Enum>(field(result, 3).data).index, static_cast<unsigned>(Living::Reason::Stale));
    EXPECT_EQ(image(practice).record_count, 1u); EXPECT_EQ(image(practice).records[0].action, exact);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, 10u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(2))->health, 12u);
    EXPECT_EQ(practice.host->lookup(practice.access, exact)->reason, Living::Reason::Stale);
}

TEST(ConsoleHost, LostAppliedReplyNeverReplaysAfterSettingAndActorChanges) {
    auto practice = Console::practice(); auto client = practice.console->open(Console::Mode::PracticeObserver);
    auto request = Core::bind_text(practice.console->registry(), client->context(), "magic.cast S1 marin").value();
    ASSERT_TRUE(client->session().submit(1, request)); ASSERT_TRUE(client->session().pump()); ASSERT_TRUE(client->session().pump());
    ASSERT_TRUE(practice.host->set_price(9));
    auto actor = practice.host->actor(practice.access, practice.host->entity(2)).value(); actor.willing = false;
    ASSERT_TRUE(practice.host->edit_actor(actor)); const auto revision = image(practice).revision;
    ASSERT_TRUE(client->session().submit(1, request));
    EXPECT_EQ((*client->session().lookup(1))->stage, Core::Stage::Succeeded);
    EXPECT_EQ(number(**client->session().lookup(1), 2), 8u);
    auto changed = Core::bind_text(practice.console->registry(), client->context(), "magic.cast S1 garo").value();
    auto conflict = client->session().submit(1, changed); ASSERT_FALSE(conflict); EXPECT_EQ(conflict.error().code, Core::Code::Conflict);
    EXPECT_EQ(query(practice, *client, 2, "operation.read op1").stage, Core::Stage::Succeeded);
    EXPECT_EQ(image(practice).revision, revision); EXPECT_EQ(image(practice).record_count, 1u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, 7u);
}

TEST(ConsoleHost, OrdinaryAndForeignAuthoritiesCannotDiscoverOrInvokeDeveloperCommands) {
    auto practice = Console::practice(), other = Console::practice({2, 1});
    auto ordinary = practice.console->open(Console::Mode::Gameplay);
    auto privileged = practice.console->open(Console::Mode::PracticeObserver);
    auto foreign = other.console->open(Console::Mode::PracticeObserver);
    Core::Authority invented; auto forged = invented.issue(std::numeric_limits<Core::Rights>::max());
    const auto before = image(practice);
    for (const auto* context : {&ordinary->context(), &foreign->context(), &forged}) {
        EXPECT_FALSE(practice.console->registry().describe("set", *context));
        EXPECT_FALSE(Core::bind_text(practice.console->registry(), *context, "set practice.heal_price 0"));
        auto suggestions = Core::complete(practice.console->registry(), *context, "set");
        ASSERT_TRUE(suggestions); EXPECT_TRUE(suggestions->empty());
    }
    const auto commands = query(practice, *ordinary, 1, "commands");
    EXPECT_EQ(std::get<std::string>(commands.value.data), "commands help magic.cast operation.read");
    EXPECT_EQ(query(practice, *ordinary, 2, "help actor.read").stage, Core::Stage::Failed);
    auto foreign_actor = foreign->context().resolve("marin", "actor").value();
    EXPECT_FALSE(practice.console->registry().bind("magic.cast", {Core::Value::enumeration(0), Core::Value::handle(foreign_actor)}, privileged->context()));
    auto sibling_actor = ordinary->context().resolve("marin", "actor").value();
    EXPECT_FALSE(practice.console->registry().bind("actor.read", {Core::Value::handle(sibling_actor)}, privileged->context()));
    EXPECT_EQ(image(practice).revision, before.revision); EXPECT_EQ(image(practice).record_count, 0u);
}

TEST(ConsoleHost, QuotaUnenteredCancellationAndEnteredShutdownRetention) {
    auto practice = Console::practice(); auto queued = practice.console->open(Console::Mode::Gameplay, 1);
    auto entered = practice.console->open(Console::Mode::Gameplay);
    auto waiting = Core::bind_text(practice.console->registry(), queued->context(), "magic.cast S1 marin").value();
    auto active = Core::bind_text(practice.console->registry(), entered->context(), "magic.cast S1 marin").value();
    ASSERT_TRUE(queued->session().submit(1, waiting));
    auto full = queued->session().submit(2, waiting); ASSERT_FALSE(full); EXPECT_EQ(full.error().code, Core::Code::Capacity);
    ASSERT_TRUE(entered->session().submit(1, active)); ASSERT_TRUE(entered->session().pump());
    practice.console->unregister_commands();
    ASSERT_TRUE(queued->session().pump()); EXPECT_EQ((*queued->session().lookup(1))->stage, Core::Stage::Cancelled);
    practice.console->close();
    EXPECT_EQ((*entered->session().lookup(1))->stage, Core::Stage::Succeeded);
    ASSERT_TRUE(entered->session().submit(1, active));
    EXPECT_FALSE(entered->session().submit(2, active));
    EXPECT_FALSE(Core::bind_text(practice.console->registry(), entered->context(), "magic.cast S1 marin"));
    EXPECT_EQ(image(practice).record_count, 1u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, 7u);
}

TEST(ConsoleHost, RequiredObservationWithholdingRetainsAppliedProgressWithoutReplay) {
    auto practice = Console::practice(); auto client = practice.console->open(Console::Mode::PracticeObserver);
    auto wren = practice.host->actor(practice.access, practice.host->entity(3)).value(); wren.access = false;
    ASSERT_TRUE(practice.host->edit_actor(wren));
    auto request = Core::bind_text(practice.console->registry(), client->context(), "magic.cast S1 marin").value();
    ASSERT_TRUE(client->session().submit(1, request)); ASSERT_TRUE(client->session().pump()); ASSERT_TRUE(client->session().pump());
    const auto progress = *client->session().lookup(1).value();
    EXPECT_EQ(progress.stage, Core::Stage::Pending); EXPECT_EQ(number(progress, 2), 8u);
    EXPECT_TRUE(std::get<bool>(field(progress, 5).data));
    ASSERT_TRUE(client->session().submit(1, request));
    wren.access = true; ASSERT_TRUE(practice.host->edit_actor(wren));
    ASSERT_TRUE(client->session().pump()); EXPECT_EQ((*client->session().lookup(1))->stage, Core::Stage::Succeeded);
    EXPECT_EQ(number(**client->session().lookup(1), 4), 1u); EXPECT_EQ(image(practice).record_count, 1u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, 7u);
}
