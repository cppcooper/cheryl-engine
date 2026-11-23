#include <tools/console/fixture.h>
#include <tests/host/access.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <stdexcept>

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
    std::string source(const Console::Practice& practice, unsigned quantity) {
        std::string text(practice.plan->source());
        const auto offset = text.find("12 hp");
        if (offset == std::string::npos) throw std::runtime_error("S1 spelling changed");
        text.replace(offset, 5, std::to_string(quantity) + " hp");
        std::replace(text.begin(), text.end(), '\n', ' ');
        return text;
    }
    Core::BoundRequest bind_request(Console::Practice& practice, Console::Client& client, std::string_view text) {
        return Core::bind_text(practice.console->registry(), client.context(), text).value();
    }
    Core::Result query(Console::Practice& practice, Console::Client& client, std::uint64_t id, std::string_view text) {
        client.session().submit(id, bind_request(practice, client, text)).value();
        client.session().pump().value();
        return *client.session().lookup(id).value();
    }
    Core::Result edit(Console::Practice& practice, Console::Client& client, std::uint64_t id, const std::string& text) {
        return query(practice, client, id, "magic.edit \"" + text + "\"");
    }
    const auto& image(const Console::Practice& practice) { return Living::HostTestAccess::image(*practice.host); }
}

TEST(ConsoleHost, AuthoringTypedTextMeaningQuoteAndNormalCast) {
    auto practice = Console::practice(); auto client = practice.console->open(Console::Mode::Gameplay);
    const auto five = source(practice, 5);
    const auto compiled = ::Magic::compile(five, practice.catalog); ASSERT_TRUE(compiled);
    ASSERT_TRUE(::Magic::Contact::supports(*compiled));
    auto typed = practice.console->registry().bind("magic.edit", {Core::Value::text(five)}, client->context());
    auto text = Core::bind_text(practice.console->registry(), client->context(), "magic.edit \"" + five + "\"");
    ASSERT_TRUE(typed); ASSERT_TRUE(text); EXPECT_TRUE(typed->same_body(*text));
    ASSERT_TRUE(client->session().submit(1, *typed)); ASSERT_TRUE(client->session().pump());
    const auto draft = query(practice, *client, 2, "magic.draft");
    ASSERT_EQ(draft.stage, Core::Stage::Succeeded);
    EXPECT_EQ(std::get<Core::Enum>(field(draft, 1).data).index, 1u);
    EXPECT_EQ(std::get<std::string>(field(draft, 2).data), five);
    EXPECT_EQ(std::get<std::string>(field(draft, 3).data), (*compiled)->inspection());
    const auto& origins = std::get<Core::Value::Children>(field(draft, 4).data); ASSERT_EQ(origins.size(), 1u);
    const auto& origin = std::get<Core::Value::Children>(origins[0].data);
    EXPECT_EQ(std::get<std::string>(origin[0].data), (*compiled)->provenance()[0].grammar);
    EXPECT_EQ(std::get<std::uint64_t>(origin[2].data), (*compiled)->provenance()[0].clause.offset);
    const auto quote = query(practice, *client, 3, "magic.quote marin");
    ASSERT_EQ(quote.stage, Core::Stage::Succeeded); EXPECT_EQ(number(quote, 4), 3u);
    EXPECT_EQ(Living::HostTestAccess::issued(*practice.host), 0u); EXPECT_EQ(image(practice).record_count, 0u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, 10u);
    const auto target = client->context().resolve("marin", "actor"); ASSERT_TRUE(target);
    auto typed_cast = practice.console->registry().bind("magic.draft.cast", {Core::Value::handle(*target)}, client->context());
    auto text_cast = Core::bind_text(practice.console->registry(), client->context(), "magic.draft.cast marin");
    ASSERT_TRUE(typed_cast); ASSERT_TRUE(text_cast); EXPECT_TRUE(typed_cast->same_body(*text_cast));
    ASSERT_TRUE(client->session().submit(4, *typed_cast)); ASSERT_TRUE(client->session().pump());
    ASSERT_EQ((*client->session().lookup(4))->stage, Core::Stage::Pending);
    ASSERT_TRUE(client->session().submit(4, *text_cast)); ASSERT_TRUE(client->session().pump());
    const auto result = *client->session().lookup(4).value();
    ASSERT_EQ(result.stage, Core::Stage::Succeeded); EXPECT_EQ(number(result, 2), 5u);
    EXPECT_EQ(image(practice).records[0].action.body.plan.id, number(quote, 0));
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(2))->health, 17u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, 7u);
    EXPECT_EQ(edit(practice, *client, 5, source(practice, 12)).stage, Core::Stage::Succeeded);
    ASSERT_TRUE(client->session().submit(4, *text_cast)); ASSERT_TRUE(client->session().pump());
    EXPECT_EQ(query(practice, *client, 6, "operation.read op1").value, result.value);
    EXPECT_EQ(image(practice).record_count, 1u); EXPECT_EQ(Living::HostTestAccess::issued(*practice.host), 1u);
}

TEST(ConsoleHost, AuthoringRejectsInvalidAndActuallyUnsupportedWithoutFallback) {
    auto practice = Console::practice(); auto client = practice.console->open(Console::Mode::Gameplay);
    const auto good = source(practice, 5);
    ASSERT_TRUE(::Magic::compile(good, practice.catalog));
    ASSERT_EQ(edit(practice, *client, 1, good).stage, Core::Stage::Succeeded);
    std::string ray = good;
    const auto delivery = ray.find("via contact"); ASSERT_NE(delivery, std::string::npos);
    ray.replace(delivery, 11, "via ray");
    const auto compiled = ::Magic::compile(ray, practice.catalog); ASSERT_TRUE(compiled);
    EXPECT_FALSE(::Magic::Contact::supports(*compiled));
    const auto unsupported = edit(practice, *client, 2, ray);
    ASSERT_EQ(unsupported.stage, Core::Stage::Succeeded);
    EXPECT_EQ(std::get<Core::Enum>(field(unsupported, 1).data).index, 3u);
    EXPECT_EQ(query(practice, *client, 3, "magic.draft.cast marin").failure.code, Core::Code::Owner);
    EXPECT_EQ(query(practice, *client, 4, "magic.quote marin").failure.code, Core::Code::Owner);
    const auto invalid = edit(practice, *client, 5, "cercle de feu");
    ASSERT_EQ(invalid.stage, Core::Stage::Succeeded);
    EXPECT_EQ(std::get<Core::Enum>(field(invalid, 1).data).index, 2u);
    const auto& diagnostic = std::get<Core::Value::Children>(field(invalid, 5).data);
    EXPECT_TRUE(std::get<bool>(diagnostic[0].data));
    EXPECT_EQ(query(practice, *client, 6, "magic.draft.cast marin").failure.code, Core::Code::Owner);
    EXPECT_EQ(Living::HostTestAccess::issued(*practice.host), 0u); EXPECT_EQ(image(practice).record_count, 0u);
    EXPECT_EQ(image(practice).plan_count, 2u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(2))->health, 12u);
    EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, 10u);
    ASSERT_EQ(query(practice, *client, 7, "magic.cast S1 marin").stage, Core::Stage::Pending);
    ASSERT_TRUE(client->session().pump());
    EXPECT_EQ(number(*client->session().lookup(7).value(), 2), 8u);
}

TEST(ConsoleHost, AuthoringAdmissionPinsPlanBeforeLaterEditAndCloseRetainsOutcome) {
    auto practice = Console::practice(); auto client = practice.console->open(Console::Mode::Gameplay);
    const auto five = source(practice, 5); ASSERT_TRUE(::Magic::compile(five, practice.catalog));
    ASSERT_EQ(edit(practice, *client, 1, five).stage, Core::Stage::Succeeded);
    const auto cast = bind_request(practice, *client, "magic.draft.cast marin");
    const auto later = bind_request(practice, *client, "magic.edit \"" + source(practice, 12) + "\"");
    ASSERT_TRUE(client->session().submit(2, cast)); ASSERT_TRUE(client->session().submit(3, later));
    ASSERT_TRUE(client->session().pump());
    ASSERT_EQ((*client->session().lookup(2))->stage, Core::Stage::Pending);
    ASSERT_EQ((*client->session().lookup(3))->stage, Core::Stage::Succeeded);
    client->close();
    const auto result = client->session().lookup(2); ASSERT_TRUE(result);
    ASSERT_EQ((*result)->stage, Core::Stage::Succeeded); EXPECT_EQ(number(**result, 2), 5u);
    EXPECT_EQ(image(practice).record_count, 1u); EXPECT_EQ(Living::HostTestAccess::issued(*practice.host), 1u);
}

TEST(ConsoleHost, AuthoringSelectionsAndEqualNumericHandlesStayClientScoped) {
    auto practice = Console::practice(); auto first = practice.console->open(Console::Mode::Gameplay);
    auto second = practice.console->open(Console::Mode::Gameplay);
    auto diagnostic = practice.console->open(Console::Mode::PracticeObserver);
    const auto five = source(practice, 5); ASSERT_TRUE(::Magic::compile(five, practice.catalog));
    ASSERT_EQ(edit(practice, *first, 1, five).stage, Core::Stage::Succeeded);
    EXPECT_EQ(std::get<Core::Enum>(field(query(practice, *second, 1, "magic.draft"), 1).data).index, 0u);
    EXPECT_EQ(query(practice, *second, 2, "magic.draft.cast marin").stage, Core::Stage::Failed);
    const auto foreign = diagnostic->context().resolve("marin", "actor"); ASSERT_TRUE(foreign);
    const auto local = first->context().resolve("marin", "actor"); ASSERT_TRUE(local);
    EXPECT_EQ(foreign->key(), local->key());
    EXPECT_FALSE(practice.console->registry().bind("magic.draft.cast", {Core::Value::handle(*foreign)}, first->context()));
    EXPECT_EQ(image(practice).record_count, 0u); EXPECT_EQ(Living::HostTestAccess::issued(*practice.host), 0u);
}

TEST(ConsoleHost, AuthoringQueuedCastRevalidatesPriceAndPermissionWithoutReplay) {
    for (const bool price : {false, true}) {
        auto practice = Console::practice(); auto client = practice.console->open(Console::Mode::Gameplay);
        const auto five = source(practice, 5); ASSERT_TRUE(::Magic::compile(five, practice.catalog));
        ASSERT_EQ(edit(practice, *client, 1, five).stage, Core::Stage::Succeeded);
        const auto cast = bind_request(practice, *client, "magic.draft.cast marin");
        ASSERT_TRUE(client->session().submit(2, cast)); ASSERT_TRUE(client->session().pump());
        ASSERT_EQ((*client->session().lookup(2))->stage, Core::Stage::Pending);
        if (price) { ASSERT_TRUE(practice.host->set_price(9)); }
        else {
            auto payer = practice.host->actor(practice.access, practice.host->entity(1)); ASSERT_TRUE(payer);
            payer->can_cast = false; ASSERT_TRUE(practice.host->edit_actor(*payer));
        }
        ASSERT_TRUE(client->session().pump());
        const auto result = *client->session().lookup(2).value();
        EXPECT_EQ(result.stage, Core::Stage::Refused);
        EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(2))->health, 12u);
        EXPECT_EQ(practice.host->actor(practice.access, practice.host->entity(1))->reserve, 10u);
        ASSERT_TRUE(client->session().submit(2, cast)); ASSERT_TRUE(client->session().pump());
        EXPECT_EQ((*client->session().lookup(2))->value, result.value);
        EXPECT_EQ(Living::HostTestAccess::issued(*practice.host), 1u); EXPECT_EQ(image(practice).record_count, 1u);
    }
}
