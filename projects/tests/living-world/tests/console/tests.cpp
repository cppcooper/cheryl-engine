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
    EXPECT_EQ(number(query(practice, *client, 3, "actor.read marin"), 0), 20u);
    EXPECT_EQ(number(query(practice, *client, 4, "actor.read mage"), 2), 7u);
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
    auto typed_setting = practice.console->registry().bind("set", {Core::Value::enumeration(0), Core::Value::unsigned_integer(4294967295ULL)}, client->context());
    auto text_setting = Core::bind_text(practice.console->registry(), client->context(), "set practice.heal_price 4294967295");
    ASSERT_TRUE(typed_setting); ASSERT_TRUE(text_setting); EXPECT_TRUE(typed_setting->same_body(*text_setting));
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
        EXPECT_EQ(std::get<Core::Enum>(field(result, 3).data).index,
            static_cast<unsigned>(insufficient ? Living::Reason::Resources : Living::Reason::Contact));
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

TEST(ConsoleHost, OrdinaryDiscoveryDoesNotChangeWithHiddenPracticeState) {
    auto first = Console::practice(), second = Console::practice({2, 1});
    auto left = first.console->open(Console::Mode::Gameplay), right = second.console->open(Console::Mode::Gameplay);
    ASSERT_TRUE(second.host->set_price(19));
    auto hidden = second.host->actor(second.access, second.host->entity(2)).value(); hidden.willing = false;
    ASSERT_TRUE(second.host->edit_actor(hidden));
    EXPECT_EQ(query(first, *left, 1, "commands").value, query(second, *right, 1, "commands").value);
    for (const auto* prefix : {"", "set", "actor", "magic.inspect", "magic.cast S1 "}) {
        auto left_result = Core::complete(first.console->registry(), left->context(), prefix);
        auto right_result = Core::complete(second.console->registry(), right->context(), prefix);
        ASSERT_TRUE(left_result); ASSERT_TRUE(right_result); EXPECT_EQ(*left_result, *right_result);
    }
    auto left_help = query(first, *left, 2, "help get"), right_help = query(second, *right, 2, "help get");
    EXPECT_EQ(left_help.stage, right_help.stage); EXPECT_EQ(left_help.failure, right_help.failure);
    EXPECT_EQ(left_help.stage, Core::Stage::Failed);
    EXPECT_EQ(image(first).record_count, 0u); EXPECT_EQ(image(second).record_count, 0u);
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
    EXPECT_EQ(query(practice, *client, 2, "operation.read op1").stage, Core::Stage::Pending);
    ASSERT_TRUE(client->session().submit(1, request));
    wren.access = true; ASSERT_TRUE(practice.host->edit_actor(wren));
    ASSERT_TRUE(client->session().pump()); EXPECT_EQ((*client->session().lookup(1))->stage, Core::Stage::Succeeded);
    EXPECT_EQ((*client->session().lookup(2))->stage, Core::Stage::Succeeded);
    EXPECT_EQ(number(**client->session().lookup(1), 4), 1u); EXPECT_EQ(image(practice).record_count, 1u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, 7u);
}

TEST(ConsoleHost, ClosedClientAndForeignOperationRefuseBeforeHostWork) {
    auto practice = Console::practice();
    auto first = practice.console->open(Console::Mode::Gameplay), second = practice.console->open(Console::Mode::Gameplay);
    auto request = Core::bind_text(practice.console->registry(), first->context(), "magic.cast S1 marin").value();
    ASSERT_TRUE(first->session().submit(1, request)); ASSERT_TRUE(first->session().pump());
    const auto operation = first->context().resolve("op1", "operation").value();
    const auto before = image(practice);
    EXPECT_FALSE(practice.console->registry().bind("operation.read", {Core::Value::handle(operation)}, second->context()));
    auto waiting = Core::bind_text(practice.console->registry(), second->context(), "magic.cast S1 marin").value();
    ASSERT_TRUE(second->session().submit(1, waiting)); second->close();
    EXPECT_EQ((*second->session().lookup(1))->stage, Core::Stage::Cancelled);
    EXPECT_FALSE(Core::bind_text(practice.console->registry(), second->context(), "magic.cast S1 marin"));
    EXPECT_EQ(image(practice).revision, before.revision); EXPECT_EQ(image(practice).record_count, 1u);
}

TEST(ConsoleHost, OperationQuotaRefusesWithoutEvictionOrAnotherAttempt) {
    auto practice = Console::practice(); auto client = practice.console->open(Console::Mode::Gameplay);
    auto request = Core::bind_text(practice.console->registry(), client->context(), "magic.cast S1 marin").value();
    for (std::uint64_t id = 1; id <= 16; ++id) {
        ASSERT_TRUE(client->session().submit(id, request)); ASSERT_TRUE(client->session().pump()); ASSERT_TRUE(client->session().pump());
        EXPECT_TRUE(Core::terminal((*client->session().lookup(id))->stage));
    }
    const auto before = image(practice);
    ASSERT_TRUE(client->session().submit(17, request)); ASSERT_TRUE(client->session().pump());
    EXPECT_EQ((*client->session().lookup(17))->stage, Core::Stage::Failed);
    EXPECT_EQ((*client->session().lookup(17))->failure.code, Core::Code::Capacity);
    EXPECT_EQ(image(practice).revision, before.revision); EXPECT_EQ(image(practice).record_count, 16u);
    ASSERT_TRUE(client->session().submit(1, request));
    EXPECT_EQ((*client->session().lookup(1))->stage, Core::Stage::Succeeded);
}

TEST(ConsoleHost, TwoClientsRetainTheirOwnDeliveredObservation) {
    auto practice = Console::practice();
    auto first = practice.console->open(Console::Mode::Gameplay);
    auto second = practice.console->open(Console::Mode::PracticeObserver);
    auto wren = practice.host->actor(practice.access, practice.host->entity(3)).value();
    wren.access = false;
    ASSERT_TRUE(practice.host->edit_actor(wren));
    auto marin = Core::bind_text(practice.console->registry(), first->context(), "magic.cast S1 marin").value();
    ASSERT_TRUE(first->session().submit(1, marin));
    ASSERT_TRUE(first->session().pump()); ASSERT_TRUE(first->session().pump());
    const auto first_action = image(practice).records[0].action;
    EXPECT_EQ((*first->session().lookup(1))->stage, Core::Stage::Pending);
    EXPECT_EQ(image(practice).work_count, 1u); EXPECT_EQ(image(practice).acquired_count, 0u);
    wren.access = true;
    ASSERT_TRUE(practice.host->edit_actor(wren));
    auto garo = Core::bind_text(practice.console->registry(), second->context(), "magic.cast S1 garo").value();
    ASSERT_TRUE(second->session().submit(1, garo));
    ASSERT_TRUE(second->session().pump()); ASSERT_TRUE(second->session().pump());
    const auto second_action = image(practice).records[1].action;
    const auto second_result = *second->session().lookup(1).value();
    EXPECT_EQ(second_result.stage, Core::Stage::Succeeded); EXPECT_EQ(number(second_result, 4), 1u);
    EXPECT_FALSE(std::get<bool>(field(second_result, 5).data));
    EXPECT_EQ(image(practice).record_count, 2u); EXPECT_EQ(image(practice).work_count, 2u);
    ASSERT_EQ(image(practice).acquired_count, 2u);
    EXPECT_EQ(image(practice).acquired[0].cause, first_action.key);
    EXPECT_EQ(image(practice).acquired[1].cause, second_action.key);
    EXPECT_EQ(practice.host->delivery(practice.access, first_action)->acquired, 1u);
    ASSERT_TRUE(first->session().submit(1, marin));
    const auto first_view = query(practice, *first, 2, "operation.read op1");
    EXPECT_EQ(first_view.stage, Core::Stage::Succeeded); EXPECT_EQ(number(first_view, 4), 1u);
    EXPECT_EQ(first_view.value, (*first->session().lookup(1))->value);
    EXPECT_EQ(query(practice, *second, 2, "operation.read op1").value, second_result.value);
    ASSERT_TRUE(first->session().submit(1, marin)); ASSERT_TRUE(second->session().submit(1, garo));
    ASSERT_TRUE(first->session().pump()); ASSERT_TRUE(second->session().pump());
    EXPECT_EQ(number(**first->session().lookup(1), 4), 1u);
    EXPECT_EQ(number(**second->session().lookup(1), 4), 1u);
    EXPECT_EQ(image(practice).record_count, 2u); EXPECT_EQ(image(practice).acquired_count, 2u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(2))->health, 20u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(4))->health, 20u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, 4u);
    first->close(); second->close();
    EXPECT_EQ((*first->session().lookup(1))->stage, Core::Stage::Succeeded);
    EXPECT_EQ((*second->session().lookup(1))->stage, Core::Stage::Succeeded);
}

TEST(ConsoleHost, UnrelatedWithholdingDoesNotBlockCompletionOrEnteredClose) {
    for (const bool refused : {false, true}) for (const bool close_entered : {false, true}) {
        auto practice = Console::practice();
        auto first = practice.console->open(Console::Mode::Gameplay);
        auto second = practice.console->open(Console::Mode::Gameplay);
        auto wren = practice.host->actor(practice.access, practice.host->entity(3)).value();
        wren.access = false;
        ASSERT_TRUE(practice.host->edit_actor(wren));
        EXPECT_EQ(query(practice, *first, 1, "magic.cast S1 marin").stage, Core::Stage::Pending);
        ASSERT_TRUE(first->session().pump());
        const auto first_action = image(practice).records[0].action;
        wren.observes = false;
        ASSERT_TRUE(practice.host->edit_actor(wren));
        if (refused) {
            auto garo = practice.host->actor(practice.access, practice.host->entity(4)).value();
            garo.willing = false;
            ASSERT_TRUE(practice.host->edit_actor(garo));
        }
        auto request = Core::bind_text(practice.console->registry(), second->context(), "magic.cast S1 garo").value();
        ASSERT_TRUE(second->session().submit(1, request)); ASSERT_TRUE(second->session().pump());
        const auto second_action = image(practice).records[1].action;
        if (close_entered) second->close();
        else {
            ASSERT_TRUE(second->session().pump());
            const auto view = query(practice, *second, 2, "operation.read op1");
            EXPECT_EQ(view.value, (*second->session().lookup(1))->value);
            second->close();
        }
        const auto result = *second->session().lookup(1).value();
        EXPECT_EQ(result.stage, refused ? Core::Stage::Refused : Core::Stage::Succeeded);
        EXPECT_EQ(number(result, 2), refused ? 0u : 8u); EXPECT_EQ(number(result, 4), 0u);
        EXPECT_EQ(std::get<Core::Enum>(field(result, 3).data).index,
            static_cast<unsigned>(refused ? Living::Reason::Contact : Living::Reason::None));
        EXPECT_FALSE(std::get<bool>(field(result, 5).data));
        EXPECT_EQ(practice.host->delivery(practice.access, second_action)->outstanding, 0u);
        ASSERT_TRUE(second->session().submit(1, request));
        EXPECT_EQ((*second->session().lookup(1))->value, result.value);
        EXPECT_EQ(image(practice).record_count, 2u); EXPECT_EQ(image(practice).work_count, 1u);
        EXPECT_FALSE(image(practice).work[0].consumed); EXPECT_EQ(image(practice).acquired_count, 0u);
        EXPECT_EQ(practice.host->delivery(practice.access, first_action)->outstanding, 1u);
        EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(2))->health, 20u);
        EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(4))->health, refused ? 12u : 20u);
        EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, refused ? 7u : 4u);
        first->close();
        EXPECT_EQ((*first->session().lookup(1))->stage, Core::Stage::Failed);
        EXPECT_EQ((*first->session().lookup(1))->failure.code, Core::Code::Owner);
        EXPECT_EQ(practice.host->delivery(practice.access, first_action)->outstanding, 1u);
        wren.access = true;
        ASSERT_TRUE(practice.host->edit_actor(wren));
        EXPECT_EQ(practice.host->deliver().adopted, 1u);
        EXPECT_EQ(image(practice).acquired_count, 1u);
        EXPECT_EQ(image(practice).acquired[0].cause, first_action.key);
    }
}

TEST(ConsoleHost, DirectOwnerDeliverySurvivesRetryReadAndClose) {
    for (const bool close_entered : {false, true}) {
        auto practice = Console::practice(); auto client = practice.console->open(Console::Mode::Gameplay);
        auto wren = practice.host->actor(practice.access, practice.host->entity(3)).value();
        wren.access = false;
        ASSERT_TRUE(practice.host->edit_actor(wren));
        auto request = Core::bind_text(practice.console->registry(), client->context(), "magic.cast S1 marin").value();
        ASSERT_TRUE(client->session().submit(1, request));
        ASSERT_TRUE(client->session().pump()); ASSERT_TRUE(client->session().pump());
        const auto action = image(practice).records[0].action;
        EXPECT_EQ((*client->session().lookup(1))->stage, Core::Stage::Pending);
        EXPECT_EQ(number(**client->session().lookup(1), 4), 0u);
        wren.access = true; wren.observes = false;
        ASSERT_TRUE(practice.host->edit_actor(wren));
        EXPECT_EQ(practice.host->deliver().adopted, 1u);
        EXPECT_EQ(practice.host->deliver().adopted, 0u);
        ASSERT_TRUE(client->session().submit(1, request));
        if (close_entered) client->close();
        else {
            const auto view = query(practice, *client, 2, "operation.read op1");
            EXPECT_EQ(view.stage, Core::Stage::Succeeded); EXPECT_EQ(number(view, 4), 1u);
            EXPECT_EQ(view.value, (*client->session().lookup(1))->value);
            ASSERT_TRUE(client->session().pump());
            client->close();
        }
        const auto result = *client->session().lookup(1).value();
        EXPECT_EQ(result.stage, Core::Stage::Succeeded); EXPECT_EQ(number(result, 4), 1u);
        EXPECT_FALSE(std::get<bool>(field(result, 5).data));
        EXPECT_EQ(image(practice).record_count, 1u); EXPECT_EQ(image(practice).work_count, 1u);
        EXPECT_TRUE(image(practice).work[0].consumed); EXPECT_EQ(image(practice).acquired_count, 1u);
        EXPECT_EQ(practice.host->delivery(practice.access, action)->acquired, 1u);
        EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(2))->health, 20u);
        EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, 7u);
    }
}

TEST(ConsoleHost, PartialDeliveryAssignsCumulativeCount) {
    auto practice = Console::practice(); auto client = practice.console->open(Console::Mode::Gameplay);
    auto wren = practice.host->actor(practice.access, practice.host->entity(3)).value();
    auto garo = practice.host->actor(practice.access, practice.host->entity(4)).value();
    wren.access = false; garo.observes = true;
    ASSERT_TRUE(practice.host->edit_actor(wren)); ASSERT_TRUE(practice.host->edit_actor(garo));
    auto request = Core::bind_text(practice.console->registry(), client->context(), "magic.cast S1 marin").value();
    ASSERT_TRUE(client->session().submit(1, request));
    ASSERT_TRUE(client->session().pump()); ASSERT_TRUE(client->session().pump());
    const auto action = image(practice).records[0].action;
    const auto partial = *client->session().lookup(1).value();
    EXPECT_EQ(partial.stage, Core::Stage::Pending); EXPECT_EQ(number(partial, 4), 1u);
    EXPECT_TRUE(std::get<bool>(field(partial, 5).data));
    EXPECT_EQ(query(practice, *client, 2, "operation.read op1").value, partial.value);
    ASSERT_TRUE(client->session().submit(1, request)); ASSERT_TRUE(client->session().pump());
    EXPECT_EQ(number(**client->session().lookup(1), 4), 1u);
    wren.access = true; wren.observes = false;
    ASSERT_TRUE(practice.host->edit_actor(wren));
    EXPECT_EQ(practice.host->deliver().adopted, 1u);
    ASSERT_TRUE(client->session().pump());
    const auto complete = *client->session().lookup(1).value();
    EXPECT_EQ(complete.stage, Core::Stage::Succeeded); EXPECT_EQ(number(complete, 4), 2u);
    EXPECT_FALSE(std::get<bool>(field(complete, 5).data));
    EXPECT_EQ((*client->session().lookup(2))->value, complete.value);
    EXPECT_EQ(practice.host->delivery(practice.access, action)->acquired, 2u);
    EXPECT_EQ(image(practice).record_count, 1u); EXPECT_EQ(image(practice).work_count, 2u);
    EXPECT_EQ(image(practice).acquired_count, 2u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(2))->health, 20u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, 7u);
    client->close();
    EXPECT_EQ(number(**client->session().lookup(1), 4), 2u);
}
