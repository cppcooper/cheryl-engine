#include "../src/aftermath.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
    namespace S = OfflineStory;
    namespace D = CE::Dialogue;
    D::Selection token(const D::View& v, std::string_view id) {
        for (const auto& row : v.choices)
            if (row.id.value == id)
                return row.selection;
        throw std::logic_error("Missing authored test choice");
    }
    template <class T> void choose(T& owner, std::string_view id) {
        owner.choose(token(owner.view().value(), id)).value();
    }
    struct Prior {
        S::PayoffHandle content;
        S::PayoffCheckpoint pair;
    };
    Prior prior(std::string_view route = "promise", std::string_view finish = "deliver", bool present = true) {
        const auto ferry_content = S::project(S::accept(S::manual_sets()[0], S::ferry_catalogue(), {}).value(), 0).value();
        auto ferry = S::Encounter::start(ferry_content, {"after-ferry"}).value();
        choose(ferry, "pay");
        choose(ferry, "continue");
        const auto watch_content = S::compose_watch(S::manual_library(), ferry_content, ferry.capture().value(), 0).value();
        auto watch = S::WatchEncounter::start(watch_content, {"after-watch"}).value();
        choose(watch, "fare-record");
        choose(watch, "ask-again");
        const auto selected =
            S::select_bargain(S::selection_library(), S::selection_catalogue(), {watch_content, watch.capture().value()}, 1, 1).value();
        auto bargain = S::BargainEncounter::start(selected->content(), {"after-bargain"}).value();
        choose(bargain, route);
        choose(bargain, "close");
        const bool promise = route == "promise";
        auto content =
            S::compose_payoff(
                selected->content(), bargain.capture().value(), {S::ReliefLocation::Quay, "nera", promise}, S::manual_payoff_library()
            )
                .value();
        auto payoff = S::PayoffEncounter::start(content, {"after-payoff"}).value();
        if (present)
            payoff.handoff(true).value();
        choose(payoff, finish);
        return {std::move(content), payoff.capture().value()};
    }
    auto content(const Prior& p) {
        return S::compose_aftermath(p.content, p.pair).value();
    }
    auto scene(const Prior& p) {
        return S::AftermathEncounter::start(content(p), {"after-scene"}).value();
    }
    void isolated(const S::AftermathState& now, const S::PayoffState& past) {
        EXPECT_EQ(now.world.knowledge, past.world.knowledge);
        EXPECT_EQ(now.world.attitude, past.world.attitude);
        EXPECT_EQ(now.world.believes_unlicensed, past.world.believes_unlicensed);
        EXPECT_EQ(now.world.knowledge[2], S::Knowledge::None);
        EXPECT_EQ(now.world.knowledge[1], S::Knowledge::VerifiedFare);
    }
}

TEST(story_aftermath, kept_report_and_passage) {
    const auto p = prior();
    auto s = scene(p);
    EXPECT_EQ(s.content().previous(), p.content);
    EXPECT_EQ(s.content().source(), p.pair);
    EXPECT_EQ(s.content().fact(), S::Reliability::Kept);
    EXPECT_EQ(s.effects(), 0);
    EXPECT_EQ(s.state().observers[0].known, S::Reliability::Kept);
    EXPECT_EQ(s.state().observers[0].evidence, S::ReliabilityEvidence::Direct);
    EXPECT_EQ(s.state().observers[0].trust, 4);
    EXPECT_EQ(s.state().observers[1].known, S::Reliability::Unknown);
    choose(s, "report");
    EXPECT_EQ(s.fare(), 1);
    EXPECT_EQ(s.state().observers[1].known, S::Reliability::Kept);
    EXPECT_EQ(s.state().observers[1].evidence, S::ReliabilityEvidence::Report);
    EXPECT_EQ(s.state().observers[1].belief, S::ReliabilityBelief::Reliable);
    EXPECT_EQ(s.state().observers[1].trust, 2);
    choose(s, "book-passage");
    EXPECT_EQ(s.state().world.facts.coins, 4);
    EXPECT_EQ(s.state().world.facts.planks, 1);
    EXPECT_EQ(s.state().fare_paid, 1);
    EXPECT_TRUE(s.state().return_passage);
    EXPECT_EQ(s.effects(), 2);
    EXPECT_EQ(s.content().source(), p.pair);
    EXPECT_EQ(s.content().source().state.delivered, 2);
    EXPECT_TRUE(s.content().source().state.favor);
    isolated(s.state(), p.pair.state);
}

TEST(story_aftermath, broken_and_declined_are_distinct) {
    for (bool broken : {false, true}) {
        const auto p = broken ? prior("promise", "break") : prior("refuse", "acknowledge");
        auto s = scene(p);
        const auto fact = broken ? S::Reliability::Broken : S::Reliability::Declined;
        EXPECT_EQ(s.content().fact(), fact);
        EXPECT_EQ(s.state().observers[0].trust, broken ? -3 : -1);
        choose(s, "report");
        EXPECT_EQ(s.state().observers[1].known, fact);
        EXPECT_EQ(s.state().observers[1].trust, broken ? -2 : 0);
        EXPECT_EQ(s.fare(), broken ? 5 : 3);
        choose(s, "book-passage");
        EXPECT_EQ(s.state().world.facts.coins, broken ? 0 : 2);
        EXPECT_EQ(s.state().world.facts.planks, 3);
        EXPECT_EQ(s.state().fare_paid, broken ? 5 : 3);
        EXPECT_TRUE(s.state().return_passage);
        EXPECT_EQ(s.content().source().state.obligation, broken ? S::Obligation::Broken : S::Obligation::None);
        EXPECT_EQ(s.content().source().state.delivered, 0);
        EXPECT_FALSE(s.content().source().state.favor);
        isolated(s.state(), p.pair.state);
    }
}

TEST(story_aftermath, uninformed_mara_does_not_infer) {
    std::optional<D::View> reference;
    for (const auto& p : {prior(), prior("promise", "break"), prior("refuse", "acknowledge")}) {
        auto s = scene(p);
        choose(s, "visit-ferry");
        const auto offered = s.view().value();
        if (!reference)
            reference = offered;
        EXPECT_EQ(offered.text, reference->text);
        ASSERT_EQ(offered.choices.size(), reference->choices.size());
        for (std::size_t i = 0; i < offered.choices.size(); ++i) {
            EXPECT_EQ(offered.choices[i].id, reference->choices[i].id);
            EXPECT_EQ(offered.choices[i].text, reference->choices[i].text);
            EXPECT_EQ(offered.choices[i].available, reference->choices[i].available);
        }
        EXPECT_EQ(s.fare(), 3);
        EXPECT_EQ(s.state().observers[1], S::ReliabilityObserver{});
        choose(s, "book-passage");
        EXPECT_EQ(s.state().world.facts.coins, 2);
        EXPECT_FALSE(s.state().reported);
        EXPECT_EQ(s.state().observers[1], S::ReliabilityObserver{});
        isolated(s.state(), p.pair.state);
    }
}

TEST(story_aftermath, restitution_preserves_breach_history) {
    const auto p = prior("promise", "break");
    auto s = scene(p);
    choose(s, "report");
    const auto held = token(s.view().value(), "reconcile");
    ASSERT_TRUE(s.resources(1));
    const auto poor = s.capture().value();
    const auto refused = s.choose(held);
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error(), D::SessionError::Unavailable);
    EXPECT_EQ(s.capture().value(), poor);
    ASSERT_TRUE(s.resources(5));
    ASSERT_TRUE(s.choose(held));
    EXPECT_EQ(s.state().world.facts.coins, 3);
    EXPECT_EQ(s.state().restitution_paid, 2);
    EXPECT_EQ(s.content().fact(), S::Reliability::Broken);
    EXPECT_EQ(s.content().source().state.obligation, S::Obligation::Broken);
    for (const auto& observer : s.state().observers) {
        EXPECT_EQ(observer.known, S::Reliability::Broken);
        EXPECT_EQ(observer.trust, 0);
        EXPECT_EQ(observer.belief, S::ReliabilityBelief::Repairing);
    }
    EXPECT_EQ(s.fare(), 3);
    choose(s, "book-passage");
    EXPECT_EQ(s.state().world.facts.coins, 0);
    EXPECT_EQ(s.state().fare_paid, 3);
    EXPECT_EQ(s.effects(), 3);
    EXPECT_EQ(s.content().source(), p.pair);
    for (const auto& good : {prior(), prior("refuse", "acknowledge")}) {
        auto other = scene(good);
        choose(other, "report");
        const auto pair = other.capture().value();
        auto rejected = other.choose(token(other.view().value(), "reconcile"));
        ASSERT_FALSE(rejected);
        EXPECT_EQ(rejected.error(), D::SessionError::Unavailable);
        EXPECT_EQ(other.capture().value(), pair);
    }
}

TEST(story_aftermath, pending_fact_is_not_suspicion) {
    const auto p = prior("promise", "leave");
    ASSERT_EQ(p.pair.state.obligation, S::Obligation::Pending);
    ASSERT_TRUE(p.pair.state.believes_broken);
    auto s = scene(p);
    EXPECT_EQ(s.content().fact(), S::Reliability::Pending);
    EXPECT_EQ(s.state().observers[0].belief, S::ReliabilityBelief::Unreliable);
    choose(s, "report");
    EXPECT_EQ(s.state().observers[1].known, S::Reliability::Pending);
    EXPECT_EQ(s.state().observers[1].belief, S::ReliabilityBelief::Uncertain);
    EXPECT_EQ(s.state().observers[1].trust, 0);
    EXPECT_EQ(s.fare(), 3);
    choose(s, "leave");
    EXPECT_EQ(s.state().world.facts.coins, 5);
    EXPECT_FALSE(s.state().return_passage);
    EXPECT_EQ(s.content().source().state.due, 2);
    EXPECT_EQ(s.content().source(), p.pair);
    const auto unseen = prior("promise", "leave", false);
    auto absent = scene(unseen);
    EXPECT_EQ(absent.state().observers[0].belief, S::ReliabilityBelief::Uncertain);
    choose(absent, "report");
    EXPECT_EQ(absent.state().observers[1].known, S::Reliability::Pending);
}

TEST(story_aftermath, immediate_help_and_payment) {
    for (bool pay : {false, true}) {
        const auto p = prior(pay ? "pay" : "help-now", "acknowledge");
        auto s = scene(p);
        EXPECT_EQ(s.content().fact(), pay ? S::Reliability::Paid : S::Reliability::Helped);
        choose(s, "report");
        EXPECT_EQ(s.fare(), 2);
        choose(s, "book-passage");
        EXPECT_EQ(s.state().world.facts.coins, pay ? 1 : 3);
        EXPECT_EQ(s.state().world.facts.planks, pay ? 3 : 2);
        EXPECT_EQ(s.content().source().state.obligation, S::Obligation::None);
        isolated(s.state(), p.pair.state);
    }
}

TEST(story_aftermath, exact_source_and_limits_refuse) {
    const auto p = prior();
    ASSERT_TRUE(S::compose_aftermath(p.content, p.pair));
    EXPECT_FALSE(S::compose_aftermath({}, p.pair));
    auto open = S::PayoffEncounter::start(p.content, {"still-open"}).value();
    const auto incomplete = open.capture().value();
    const auto refused_open = S::compose_aftermath(p.content, incomplete);
    ASSERT_FALSE(refused_open);
    EXPECT_EQ(refused_open.error().field, "aftermath.completed");
    for (int defect = 0; defect < 4; ++defect) {
        auto corrupt = p.pair;
        if (defect == 0)
            ++corrupt.state.world.facts.coins;
        if (defect == 1)
            corrupt.dialogue.node.value = "broken";
        if (defect == 2)
            ++corrupt.dialogue.definition_revision;
        if (defect == 3)
            corrupt.dialogue.definition.value = "foreign";
        EXPECT_FALSE(S::compose_aftermath(p.content, corrupt)) << defect;
    }
    for (auto field :
         {&S::AftermathLimits::nodes, &S::AftermathLimits::choices, &S::AftermathLimits::text_length, &S::AftermathLimits::text_bytes,
          &S::AftermathLimits::storage_bytes, &S::AftermathLimits::retained_bytes, &S::AftermathLimits::work}) {
        S::AftermathLimits l;
        l.*field = 0;
        EXPECT_FALSE(S::compose_aftermath(p.content, p.pair, l));
        l = {};
        ++(l.*field);
        const auto too_large = S::compose_aftermath(p.content, p.pair, l);
        ASSERT_FALSE(too_large);
        EXPECT_EQ(too_large.error().code, S::Code::InvalidLimits);
    }
    auto c = content(p);
    std::cout << "Aftermath logical usage: text=" << c.usage().text_bytes << " storage=" << c.usage().storage_bytes
              << " work=" << c.usage().work << " retained=" << c.retained_bytes() << " scalar_state=" << sizeof(S::AftermathState) << '\n';
}

TEST(story_aftermath, retention_move_and_original_authority) {
    auto p = prior();
    const auto expected = p.pair;
    auto c = content(p);
    p.content.reset();
    p.pair.dialogue.definition.value.assign(3000, 'x');
    p.pair.state.world.facts.coins = 99;
    EXPECT_EQ(c.source(), expected);
    auto a = S::AftermathEncounter::start(c, {"same"}).value();
    auto b = S::AftermathEncounter::start(c, {"same"}).value();
    const auto original = token(a.view().value(), "report");
    const auto foreign = b.choose(original);
    ASSERT_FALSE(foreign);
    EXPECT_EQ(foreign.error(), D::SessionError::WrongIncarnation);
    auto moved = std::move(a);
    ASSERT_TRUE(moved.choose(original));
    const auto stale = moved.choose(original);
    ASSERT_FALSE(stale);
    EXPECT_EQ(stale.error(), D::SessionError::StaleSelection);
    auto priced = S::AftermathEncounter::start(c, {"fare-offer"}).value();
    choose(priced, "visit-ferry");
    const auto old_price = token(priced.view().value(), "book-passage");
    choose(priced, "report");
    const auto reported = priced.capture().value();
    const auto old_fare = priced.choose(old_price);
    ASSERT_FALSE(old_fare);
    EXPECT_EQ(old_fare.error(), D::SessionError::StaleSelection);
    EXPECT_EQ(priced.capture().value(), reported);
    choose(priced, "book-passage");
    EXPECT_EQ(priced.state().world.facts.coins, 4);
    const auto view = [&] {
        auto temporary = S::AftermathEncounter::start(std::move(c), {"temporary"}).value();
        return temporary.view().value();
    }();
    EXPECT_NE(view.text.find("checked your delivery"), std::string::npos);
    EXPECT_EQ(view.choices[0].id.value, "report");
    EXPECT_EQ(moved.content().source(), expected);
}

TEST(story_aftermath, paired_restore_and_live_refusal) {
    auto s = scene(prior("promise", "break"));
    choose(s, "report");
    const auto held = token(s.view().value(), "book-passage");
    const auto pair = s.capture().value();
    auto bad = pair;
    bad.state.observers[1].known = S::Reliability::Kept;
    EXPECT_FALSE(s.restore(bad));
    EXPECT_EQ(s.capture().value(), pair);
    auto cursor = pair;
    cursor.dialogue.node.value = "booked";
    EXPECT_FALSE(s.restore(cursor));
    EXPECT_EQ(s.capture().value(), pair);
    ASSERT_TRUE(s.resources(4));
    const auto short_pair = s.capture().value();
    auto poor = s.choose(held);
    ASSERT_FALSE(poor);
    EXPECT_EQ(poor.error(), D::SessionError::Unavailable);
    EXPECT_EQ(s.capture().value(), short_pair);
    ASSERT_TRUE(s.resources(5));
    ASSERT_TRUE(s.restore(pair));
    const auto resumed = s.choose(held);
    ASSERT_FALSE(resumed);
    EXPECT_EQ(resumed.error(), D::SessionError::WrongIncarnation);
    choose(s, "book-passage");
    EXPECT_EQ(s.state().world.facts.coins, 0);
    EXPECT_EQ(s.effects(), 2);
    const auto terminal = s.capture().value();
    EXPECT_TRUE(s.restore(terminal));
    EXPECT_EQ(s.state().world.facts.coins, 0);
    EXPECT_EQ(s.effects(), 2);
    EXPECT_FALSE(s.resources(99));
}
