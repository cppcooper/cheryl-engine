#include "../src/payoff.h"
#include <cheryl/core/game-framework/dialogue-inspection.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
    namespace S = OfflineStory;
    namespace D = CE::Dialogue;
    D::Selection token(const D::View& v, std::string_view id) {
        for (const auto& c : v.choices)
            if (c.id.value == id)
                return c.selection;
        throw std::logic_error("Missing literal payoff choice");
    }
    struct Past {
        S::Encounter ferry;
        S::WatchEncounter watch;
        S::SelectedBargainHandle selected;
        S::BargainEncounter bargain;
    };
    Past past(std::uint64_t seed = 1, std::string_view route = "promise", bool bank = false) {
        const auto graph = S::project(S::accept(S::manual_sets()[0], S::ferry_catalogue(), {}).value(), 0).value();
        auto ferry = S::Encounter::start(graph, {"ferry"}).value();
        ferry.choose(token(ferry.view().value(), bank ? "decline" : "pay")).value();
        ferry.choose(token(ferry.view().value(), "continue")).value();
        const auto content = S::compose_watch(S::manual_library(), graph, ferry.capture().value(), 0).value();
        auto watch = S::WatchEncounter::start(content, {"watch"}).value();
        watch.choose(token(watch.view().value(), bank ? "withhold" : "fare-record")).value();
        if (!bank)
            watch.choose(token(watch.view().value(), "ask-again")).value();
        const auto selected =
            S::select_bargain(S::selection_library(), S::selection_catalogue(), {content, watch.capture().value()}, seed, 1).value();
        auto bargain = S::BargainEncounter::start(selected->content(), {"bargain"}).value();
        if (selected->descriptor().bundle == S::ReliefBundle::ExplainFirst || route == "negotiate")
            bargain.choose(token(bargain.view().value(), "ask-terms")).value();
        if (route == "negotiate")
            bargain.choose(token(bargain.view().value(), "negotiate")).value();
        bargain.choose(token(bargain.view().value(), route == "immediate" ? "help-now" : route == "negotiate" ? "commit" : route)).value();
        bargain.choose(token(bargain.view().value(), "close")).value();
        return {std::move(ferry), std::move(watch), selected, std::move(bargain)};
    }
    S::HandoffEvent event(const Past& p, bool suspect = true) {
        return {p.selected->content()->request().location, p.selected->content()->request().actor, suspect};
    }
    S::PayoffHandle content(std::uint64_t seed = 1, std::string_view route = "promise", bool bank = false) {
        auto p = past(seed, route, bank);
        return S::compose_payoff(
                   p.selected->content(), p.bargain.capture().value(), event(p, route == "promise" || route == "negotiate"),
                   S::manual_payoff_library()
        )
            .value();
    }
    S::PayoffEncounter scene(S::PayoffHandle c = content()) {
        return S::PayoffEncounter::start(std::move(c), {"payoff"}).value();
    }
    void choose(S::PayoffEncounter& s, std::string_view id) {
        s.choose(token(s.view().value(), id)).value();
    }
}

TEST(story_payoff, caller_and_pure_preparation) {
    auto p = past();
    const auto completed = p.bargain.capture().value();
    const auto c = S::compose_payoff(p.selected->content(), completed, event(p), S::manual_payoff_library()).value();
    EXPECT_EQ(p.bargain.capture().value(), completed);
    EXPECT_EQ(p.ferry.effects(), 1);
    EXPECT_EQ(p.watch.effects(), 1);
    EXPECT_EQ(p.bargain.effects(), 1);
    auto s = scene(c);
    const auto offered = s.view().value();
    ASSERT_EQ(offered.choices.size(), 3);
    EXPECT_EQ(offered.choices[0].id.value, "deliver");
    EXPECT_EQ(offered.choices[1].id.value, "break");
    EXPECT_EQ(offered.choices[2].id.value, "leave");
    EXPECT_FALSE(offered.choices[0].available);
    EXPECT_FALSE(offered.choices[1].available);
    EXPECT_TRUE(offered.choices[2].available);
    EXPECT_EQ(s.state().due, 2);
    EXPECT_EQ(s.state().world.facts.planks, 3);
    EXPECT_FALSE(s.state().favor);
    EXPECT_EQ(s.effects(), 0);
    const auto before = s.capture().value();
    auto unavailable = s.choose(offered.choices[0].selection);
    ASSERT_FALSE(unavailable);
    EXPECT_EQ(unavailable.error(), D::SessionError::Unavailable);
    EXPECT_EQ(s.capture().value(), before);
    const auto r = D::inspect(*c->definition()).value();
    EXPECT_EQ(r.nodes, 4);
    EXPECT_EQ(r.choices, 3);
    EXPECT_EQ(r.terminals, 3);
    EXPECT_EQ(r.reachable.size(), 4);
    EXPECT_EQ(s.effects(), 0);
    EXPECT_EQ(c->source(), completed);
}

TEST(story_payoff, fulfilled_literal_consequences) {
    for (const auto row : {std::pair{1u, 1}, std::pair{3u, 0}}) {
        auto s = scene(content(row.first));
        s.handoff(true).value();
        const auto offered = s.view().value();
        ASSERT_TRUE(offered.choices[0].available && offered.choices[1].available && offered.choices[2].available);
        EXPECT_EQ(s.state().obligation, S::Obligation::Pending);
        EXPECT_TRUE(s.state().believes_broken);
        EXPECT_FALSE(s.state().favor);
        choose(s, "deliver");
        const auto& state = s.state();
        EXPECT_EQ(state.world.facts.coins, 5);
        EXPECT_EQ(state.world.facts.planks, row.second);
        EXPECT_EQ(state.delivered, 3 - row.second);
        EXPECT_EQ(state.obligation, S::Obligation::Fulfilled);
        EXPECT_EQ(state.outcome, S::PayoffOutcome::Fulfilled);
        EXPECT_EQ(state.due, 0);
        EXPECT_TRUE(state.favor);
        EXPECT_EQ(state.recipient_knows, S::RecipientKnowledge::CheckedDelivery);
        EXPECT_EQ(state.recipient_attitude, 4);
        EXPECT_FALSE(state.believes_broken);
        EXPECT_EQ(s.capture()->dialogue.node.value, "fulfilled");
        EXPECT_EQ(s.view()->kind, D::NodeKind::Terminal);
        EXPECT_EQ(s.effects(), 1);
    }
}

TEST(story_payoff, breach_and_leave) {
    auto s = scene();
    s.handoff(true).value();
    choose(s, "break");
    EXPECT_EQ(s.state().world.facts.coins, 5);
    EXPECT_EQ(s.state().world.facts.planks, 3);
    EXPECT_EQ(s.state().due, 0);
    EXPECT_EQ(s.state().delivered, 0);
    EXPECT_FALSE(s.state().favor);
    EXPECT_EQ(s.state().obligation, S::Obligation::Broken);
    EXPECT_EQ(s.state().recipient_knows, S::RecipientKnowledge::HeardBreach);
    EXPECT_EQ(s.state().recipient_attitude, -3);
    EXPECT_TRUE(s.state().believes_broken);
    EXPECT_EQ(s.capture()->dialogue.node.value, "broken");
    auto left = scene();
    choose(left, "leave");
    EXPECT_EQ(left.state().world.facts.planks, 3);
    EXPECT_EQ(left.state().obligation, S::Obligation::Pending);
    EXPECT_EQ(left.state().due, 2);
    EXPECT_FALSE(left.state().favor);
    EXPECT_FALSE(left.state().encountered);
    EXPECT_EQ(left.state().recipient_attitude, 0);
    EXPECT_EQ(left.capture()->dialogue.node.value, "left");
    EXPECT_FALSE(left.handoff(true));
}

TEST(story_payoff, immediate_payment_and_refusal) {
    struct Row {
        const char* route;
        int coins, planks, attitude;
        S::RecipientKnowledge knows;
    };
    const Row rows[] = {
        {"immediate", 5, 2, 3, S::RecipientKnowledge::SawHelp},
        {"pay", 3, 3, 1, S::RecipientKnowledge::SawPayment},
        {"refuse", 5, 3, -1, S::RecipientKnowledge::HeardRefusal}
    };
    for (const auto& row : rows) {
        auto s = scene(content(1, row.route));
        s.handoff(true).value();
        const auto v = s.view().value();
        ASSERT_EQ(v.choices.size(), 2);
        EXPECT_TRUE(v.choices[0].available && v.choices[1].available);
        choose(s, "acknowledge");
        EXPECT_EQ(s.state().world.facts.coins, row.coins);
        EXPECT_EQ(s.state().world.facts.planks, row.planks);
        EXPECT_EQ(s.state().recipient_attitude, row.attitude);
        EXPECT_EQ(s.state().recipient_knows, row.knows);
        EXPECT_EQ(s.state().obligation, S::Obligation::None);
        EXPECT_EQ(s.state().due, 0);
        EXPECT_EQ(s.state().delivered, 0);
        EXPECT_FALSE(s.state().favor);
        EXPECT_EQ(s.capture()->dialogue.node.value, "acknowledged");
    }
    auto bank = scene(content(1, "refuse", true));
    bank.handoff(true).value();
    choose(bank, "acknowledge");
    EXPECT_FALSE(bank.state().world.facts.passage);
    EXPECT_TRUE(bank.state().world.facts.declined);
    EXPECT_EQ(bank.state().world.facts.coins, 8);
    EXPECT_EQ(bank.state().world.facts.planks, 3);
    EXPECT_NE(bank.view()->text.find("refusal"), std::string::npos);
}

TEST(story_payoff, accepted_quantity_and_negotiation) {
    const auto a = content(1), b = content(3);
    EXPECT_EQ(a->bargain()->rule().promise_planks, 2);
    EXPECT_EQ(b->bargain()->rule().promise_planks, 3);
    auto x = scene(a), y = scene(b);
    EXPECT_NE(x.view()->text, y.view()->text);
    EXPECT_NE(x.view()->text.find("Recorded due: 2 planks."), std::string::npos);
    EXPECT_NE(y.view()->text.find("Recorded due: 3 planks."), std::string::npos);
    auto n = scene(content(1, "negotiate"));
    EXPECT_EQ(n.state().due, 1);
    n.handoff(true).value();
    choose(n, "deliver");
    EXPECT_EQ(n.state().world.facts.planks, 2);
    EXPECT_EQ(n.state().delivered, 1);
    EXPECT_TRUE(n.state().favor);
}

TEST(story_payoff, knowledge_is_not_facts) {
    const auto c = content();
    auto s = scene(c);
    const auto world = s.state().world;
    EXPECT_FALSE(s.state().believes_broken);
    s.handoff(true).value();
    EXPECT_TRUE(s.state().believes_broken);
    EXPECT_EQ(s.state().obligation, S::Obligation::Pending);
    EXPECT_EQ(s.state().world, world);
    EXPECT_EQ(s.state().recipient_knows, S::RecipientKnowledge::HeardPromise);
    EXPECT_EQ(s.view()->text.find("Ivo"), std::string::npos);
    EXPECT_EQ(s.view()->text.find("Lena"), std::string::npos);
    choose(s, "deliver");
    EXPECT_EQ(s.state().world.knowledge, world.knowledge);
    EXPECT_EQ(s.state().world.attitude, world.attitude);
    EXPECT_EQ(s.state().world.believes_unlicensed, world.believes_unlicensed);
    EXPECT_EQ(s.state().world.knowledge[2], S::Knowledge::None);
    EXPECT_EQ(s.state().world.attitude[2], 0);
    EXPECT_FALSE(s.state().world.believes_unlicensed[2]);
    EXPECT_EQ(s.state().world.facts.passage, world.facts.passage);
    EXPECT_EQ(s.state().world.facts.licensed, world.facts.licensed);
    EXPECT_FALSE(s.state().believes_broken);
    EXPECT_EQ(s.state().recipient_knows, S::RecipientKnowledge::CheckedDelivery);
}

TEST(story_payoff, live_resources_and_presence) {
    auto s = scene();
    s.handoff(true).value();
    const auto presented = s.view().value();
    const auto deliver = token(presented, "deliver");
    ASSERT_TRUE(s.resources(1));
    const auto before = s.capture().value();
    auto r = s.choose(deliver);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error(), D::SessionError::Unavailable);
    EXPECT_EQ(s.capture().value(), before);
    EXPECT_EQ(s.effects(), 0);
    ASSERT_TRUE(s.resources(2));
    s.handoff(false).value();
    const auto absent = s.capture().value();
    r = s.choose(deliver);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error(), D::SessionError::Unavailable);
    EXPECT_EQ(s.capture().value(), absent);
    EXPECT_TRUE(s.state().believes_broken); // Leaving the site does not erase previously heard suspicion.
    s.handoff(true).value();
    ASSERT_TRUE(s.choose(deliver));
    EXPECT_EQ(s.state().world.facts.planks, 0);
    EXPECT_TRUE(s.state().favor);
    EXPECT_EQ(s.effects(), 1);
    EXPECT_FALSE(s.resources(3));
    EXPECT_FALSE(s.handoff(true));
}

TEST(story_payoff, authority_and_same_timeline) {
    const auto c = content();
    auto a = scene(c), b = scene(c);
    a.handoff(true).value();
    b.handoff(true).value();
    const auto choice = token(a.view().value(), "deliver");
    auto foreign = b.choose(choice);
    ASSERT_FALSE(foreign);
    EXPECT_EQ(foreign.error(), D::SessionError::WrongIncarnation);
    auto other = S::PayoffEncounter::start(c, {"other"}).value();
    other.handoff(true).value();
    auto wrong = other.choose(choice);
    ASSERT_FALSE(wrong);
    EXPECT_EQ(wrong.error(), D::SessionError::WrongSession);
    auto unknown = choice;
    unknown.choice = {"invent-reward"};
    auto rejected = a.choose(unknown);
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error(), D::SessionError::UnknownChoice);
    EXPECT_EQ(a.effects(), 0);
    ASSERT_TRUE(a.choose(choice));
    const auto done = a.capture().value();
    for (int i = 0; i < 3; ++i) {
        auto stale = a.choose(choice);
        ASSERT_FALSE(stale);
        EXPECT_EQ(stale.error(), D::SessionError::StaleSelection);
        EXPECT_EQ(a.capture().value(), done);
    }
    EXPECT_EQ(a.effects(), 1);
    EXPECT_EQ(a.state().world.facts.planks, 1);
    EXPECT_TRUE(a.view()->choices.empty());
}

TEST(story_payoff, paired_restore_boundaries) {
    const auto c = content();
    auto a = scene(c);
    const auto before = a.capture().value();
    a.handoff(true).value();
    const auto pending = a.capture().value();
    const auto old = token(a.view().value(), "deliver");
    a.restore(pending).value();
    auto rejected = a.choose(old);
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error(), D::SessionError::WrongIncarnation);
    EXPECT_EQ(a.effects(), 0);
    choose(a, "deliver");
    const auto resolved = a.capture().value();
    EXPECT_EQ(a.effects(), 1);
    auto fresh = scene(c);
    fresh.restore(resolved).value();
    EXPECT_EQ(fresh.effects(), 0);
    EXPECT_EQ(fresh.capture().value(), resolved);
    EXPECT_TRUE(fresh.state().favor);
    EXPECT_EQ(fresh.state().world.facts.planks, 1);
    EXPECT_EQ(fresh.state().delivered, 2);
    a.restore(before).value();
    EXPECT_FALSE(a.state().at_handoff);
    EXPECT_EQ(a.state().due, 2);
    EXPECT_FALSE(a.state().favor);
    EXPECT_EQ(a.effects(), 1);
    fresh.restore(pending).value();
    EXPECT_EQ(fresh.state().world.facts.planks, 3);
    EXPECT_FALSE(fresh.state().favor);
    choose(fresh, "break");
    const auto broken = fresh.capture().value();
    a.restore(broken).value();
    EXPECT_EQ(a.state().obligation, S::Obligation::Broken);
    EXPECT_EQ(a.effects(), 1);
    fresh.restore(pending).value();
    choose(fresh, "deliver");
    EXPECT_EQ(fresh.effects(), 2); // Complete-world rewind starts a NEW timeline, not global exactly-once.
    EXPECT_EQ(fresh.state().world.facts.planks, 1);
    EXPECT_EQ(fresh.state().delivered, 2);
}

TEST(story_payoff, failed_restore_preserves_pair_and_token) {
    const auto accepted = content();
    auto a = scene(accepted);
    a.handoff(true).value();
    const auto original = a.capture().value();
    const auto authority = token(a.view().value(), "deliver");
    for (int i = 0; i < 11; ++i) {
        auto bad = original;
        switch (i) {
            case 0:
                bad.state.favor = true;
                break;
            case 1:
                bad.state.due = 3;
                break;
            case 2:
                bad.state.delivered = 1;
                break;
            case 3:
                bad.state.world.knowledge[2] = S::Knowledge::HeardPassage;
                break;
            case 4:
                bad.state.world.facts.coins = 4;
                break;
            case 5:
                bad.dialogue.node = {"fulfilled"};
                break;
            case 6:
                ++bad.dialogue.definition_revision;
                break;
            case 7:
                bad.dialogue.session = {""};
                break;
            case 8:
                bad.dialogue.schema = 0;
                break;
            case 9:
                bad.state.recipient_attitude = 4;
                break;
            case 10:
                bad.state.encountered = false;
                break;
        }
        EXPECT_FALSE(a.restore(bad));
        EXPECT_EQ(a.capture().value(), original);
        EXPECT_EQ(a.effects(), 0);
    }
    auto other_library = S::manual_payoff_library();
    other_library.id = "another-handoff";
    const auto alternate = S::compose_payoff(accepted->bargain(), accepted->source(), accepted->event(), other_library).value();
    EXPECT_NE(alternate->definition()->id(), accepted->definition()->id());
    auto other = scene(alternate);
    other.handoff(true).value();
    EXPECT_FALSE(a.restore(other.capture().value()));
    EXPECT_EQ(a.capture().value(), original);
    const auto& old_source = *accepted->bargain();
    auto revised_sets = old_source.sets();
    ++revised_sets.revision;
    const auto revised_bargain = S::compose_bargain(revised_sets, old_source.catalogue(), old_source.context(), old_source.pick()).value();
    auto revised_pair = accepted->source();
    revised_pair.dialogue.definition_revision = revised_bargain->definition()->revision();
    const auto revised = S::compose_payoff(revised_bargain, revised_pair, accepted->event(), accepted->library()).value();
    EXPECT_EQ(revised_bargain->definition()->id(), old_source.definition()->id());
    EXPECT_NE(revised->definition()->id(), accepted->definition()->id());
    auto revised_owner = scene(revised);
    revised_owner.handoff(true).value();
    EXPECT_FALSE(a.restore(revised_owner.capture().value()));
    EXPECT_EQ(a.capture().value(), original);
    ASSERT_TRUE(a.choose(authority));
    EXPECT_TRUE(a.state().favor);
    auto bad = a.capture().value();
    bad.state.settlement_planks = 0;
    EXPECT_FALSE(a.restore(bad));
    EXPECT_TRUE(a.state().favor);
}

TEST(story_payoff, source_context_refusals) {
    auto p = past();
    const auto complete = p.bargain.capture().value();
    auto handoff = event(p);
    auto missing = S::compose_payoff({}, complete, handoff, S::manual_payoff_library());
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, S::Code::EmptyHandle);
    for (int i = 0; i < 6; ++i) {
        auto bad = complete;
        auto e = handoff;
        switch (i) {
            case 0:
                bad.dialogue.node = {"promised"};
                break;
            case 1:
                bad.state.planks_due = 3;
                break;
            case 2:
                ++bad.dialogue.definition_revision;
                break;
            case 3:
                e.recipient = "lena";
                break;
            case 4:
                e.location = S::ReliefLocation::Bank;
                break;
            case 5:
                bad.state.resolution = S::ReliefResolution::Open;
                break;
        }
        const auto r = S::compose_payoff(p.selected->content(), bad, e, S::manual_payoff_library());
        EXPECT_FALSE(r);
        EXPECT_EQ(p.bargain.capture().value(), complete);
    }
    auto q = past(1, "immediate");
    auto impossible = S::compose_payoff(q.selected->content(), q.bargain.capture().value(), event(q, true), S::manual_payoff_library());
    ASSERT_FALSE(impossible);
    EXPECT_EQ(impossible.error().field, "payoff.handoff");
}

TEST(story_payoff, ingredient_refusals_and_ownership) {
    S::PayoffHandle accepted;
    std::weak_ptr<const S::BargainArtifact> retained;
    {
        auto p = past();
        retained = p.selected->content();
        auto lib = S::manual_payoff_library();
        auto event_value = event(p);
        auto completed = p.bargain.capture().value();
        accepted = S::compose_payoff(p.selected->content(), completed, event_value, lib).value();
        const auto prior = accepted;
        for (int i = 0; i < 6; ++i) {
            auto broken = lib;
            switch (i) {
                case 0:
                    broken.fragments[0].id = "grant-arbitrary-operation";
                    break;
                case 1:
                    broken.fragments[0].id = broken.fragments[1].id;
                    break;
                case 2:
                    broken.fragments.pop_back();
                    break;
                case 3:
                    broken.revision = 0;
                    break;
                case 4:
                    broken.fragments[0].text[0] = "";
                    break;
                case 5:
                    broken.fragments[0].text[1] = std::string("bad\0text", 8);
                    break;
            }
            EXPECT_FALSE(S::compose_payoff(p.selected->content(), completed, event_value, broken));
            EXPECT_EQ(accepted, prior);
        }
        lib.fragments[4].text[1] = "mutated fulfillment";
        event_value.recipient = "lena";
        completed.state.planks_due = 10;
        EXPECT_NE(accepted->library().fragments[4].text[1], "mutated fulfillment");
        EXPECT_EQ(accepted->event().recipient, "nera");
        EXPECT_EQ(accepted->source().state.planks_due, 2);
    }
    auto claimed = accepted->library();
    claimed.fragments[4].text[1] = "A hundred coins and magical rewards are supposedly yours.";
    const auto claim_content = S::compose_payoff(accepted->bargain(), accepted->source(), accepted->event(), claimed).value();
    auto claim = scene(claim_content);
    claim.handoff(true).value();
    choose(claim, "deliver");
    EXPECT_EQ(claim.state().world.facts.coins, 5);
    EXPECT_EQ(claim.state().world.facts.planks, 1);
    EXPECT_TRUE(claim.state().favor);
    EXPECT_FALSE(retained.expired());
    auto s = scene(accepted);
    accepted.reset();
    s.handoff(true).value();
    choose(s, "deliver");
    EXPECT_EQ(s.state().delivered, 2);
    static_assert(!std::is_copy_constructible_v<S::PayoffContent>);
}

TEST(story_payoff, exact_limits) {
    auto p = past();
    const auto lib = S::manual_payoff_library();
    const auto complete = p.bargain.capture().value();
    const auto e = event(p);
    const auto prior = S::compose_payoff(p.selected->content(), complete, e, lib).value();
    const auto u = prior->usage();
    S::PayoffLimits exact;
    exact.work = u.owned.work;
    exact.storage_bytes = u.owned.storage_bytes;
    exact.retained_bytes = u.retained_bytes;
    exact.text_bytes = u.owned.text_bytes;
    ASSERT_TRUE(S::compose_payoff(p.selected->content(), complete, e, lib, exact));
    std::size_t S::PayoffLimits::* fields[] = {&S::PayoffLimits::work,           &S::PayoffLimits::storage_bytes,
                                               &S::PayoffLimits::retained_bytes, &S::PayoffLimits::text_bytes,
                                               &S::PayoffLimits::ingredients,    &S::PayoffLimits::candidates,
                                               &S::PayoffLimits::nodes,          &S::PayoffLimits::choices};
    for (auto f : fields) {
        auto short_limit = exact;
        --(short_limit.*f);
        auto r = S::compose_payoff(p.selected->content(), complete, e, lib, short_limit);
        ASSERT_FALSE(r);
        EXPECT_EQ(prior->source(), complete);
    }
    for (auto f : fields) {
        auto invalid = exact;
        invalid.*f = std::numeric_limits<std::size_t>::max();
        auto r = S::compose_payoff(p.selected->content(), complete, e, lib, invalid);
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code, S::Code::InvalidLimits);
    }
    auto longest = std::max(
        {prior->definition()->id().value.size(), lib.id.size(), e.recipient.size(), complete.dialogue.definition.value.size(),
         complete.dialogue.session.value.size(), complete.dialogue.node.value.size()}
    );
    for (const auto& f : lib.fragments) {
        longest = std::max(longest, f.id.size());
        for (const auto& text : f.text)
            longest = std::max(longest, text.size());
    }
    for (const auto& n : prior->definition()->nodes())
        longest = std::max(longest, n.text.size());
    auto exact_string = exact;
    exact_string.text_length = longest;
    ASSERT_TRUE(S::compose_payoff(p.selected->content(), complete, e, lib, exact_string));
    --exact_string.text_length;
    EXPECT_FALSE(S::compose_payoff(p.selected->content(), complete, e, lib, exact_string));
    auto identity = lib;
    identity.id = std::string(64, 'a');
    EXPECT_TRUE(S::compose_payoff(p.selected->content(), complete, e, identity));
    identity.id.push_back('a');
    EXPECT_FALSE(S::compose_payoff(p.selected->content(), complete, e, identity));
    auto short_string = exact;
    short_string.text_length = 1;
    auto r = S::compose_payoff(p.selected->content(), complete, e, lib, short_string);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code, S::Code::TextLimit);
}

TEST(story_payoff, moved_and_step_boundaries) {
    const auto c = content();
    auto a = scene(c);
    a.handoff(true).value();
    const auto issued = token(a.view().value(), "deliver");
    auto moved = std::move(a);
    EXPECT_FALSE(a.view());
    EXPECT_FALSE(a.capture());
    EXPECT_FALSE(a.handoff(true));
    EXPECT_FALSE(a.resources(2));
    ASSERT_TRUE(moved.choose(issued));
    auto full = scene(c);
    full.handoff(true).value();
    auto exhausted = full.capture().value();
    exhausted.dialogue.step = std::numeric_limits<std::uint64_t>::max();
    full.restore(exhausted).value();
    const auto before = full.capture().value();
    auto r = full.choose(token(full.view().value(), "deliver"));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error(), D::SessionError::StepExhausted);
    EXPECT_EQ(full.capture().value(), before);
    EXPECT_EQ(full.effects(), 0);
}
