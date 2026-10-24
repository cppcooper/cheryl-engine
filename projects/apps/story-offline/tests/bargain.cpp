#include "../src/bargain.h"
#include <cheryl/core/game-framework/dialogue-inspection.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
    namespace S = OfflineStory;
    namespace D = CE::Dialogue;
    D::Selection select(const D::View& view, std::string_view id) {
        for (const auto& c : view.choices)
            if (c.id.value == id)
                return c.selection;
        throw std::logic_error("Missing literal bargain choice");
    }
    struct History {
        S::ProjectionHandle ferry_graph;
        S::Encounter ferry;
        S::ContinuationHandle watch_graph;
        S::WatchEncounter watch;
        S::BargainContext context() { return {watch_graph, watch.capture().value()}; }
    };
    History history(std::string_view prior = "pay") {
        const auto ferry_graph = S::project(S::accept(S::manual_sets()[0], S::ferry_catalogue(), {}).value(), 0).value();
        auto ferry = S::Encounter::start(ferry_graph, {"ferry"}).value();
        ferry.choose(select(ferry.view().value(), prior)).value();
        ferry.choose(select(ferry.view().value(), "continue")).value();
        const auto watch_graph = S::compose_watch(S::manual_library(), ferry_graph, ferry.capture().value(), 0).value();
        auto watch = S::WatchEncounter::start(watch_graph, {"watch"}).value();
        watch.choose(select(watch.view().value(), prior == "decline" ? "withhold" : watch_graph->recipe().action)).value();
        if (prior != "decline")
            watch.choose(select(watch.view().value(), "ask-again")).value();
        return {ferry_graph, std::move(ferry), watch_graph, std::move(watch)};
    }
    S::BargainHandle content(std::string_view request = "ramp-quay", std::size_t flavour = 0, std::string_view prior = "pay") {
        auto h = history(prior);
        return S::compose_bargain(S::manual_bargain_sets(), S::relief_catalogue(), h.context(), {std::string(request), flavour}).value();
    }
    S::BargainEncounter scene(S::BargainHandle c = content()) {
        return S::BargainEncounter::start(std::move(c), {"relief"}).value();
    }
    void choose(S::BargainEncounter& s, std::string_view id) {
        s.choose(select(s.view().value(), id)).value();
    }
}

TEST(story_bargain, caller) {
    auto h = history();
    const auto context = h.context();
    const auto accepted = S::compose_bargain(S::manual_bargain_sets(), S::relief_catalogue(), context, {"ramp-quay", 0}).value();
    auto s = scene(accepted);
    const auto offered = s.view().value();
    ASSERT_EQ(offered.choices.size(), 6);
    EXPECT_EQ(offered.text, "On the harbor quay. The relief ramp needs a sound brace before nightfall.");
    const char* ids[] = {"ask-terms", "help-now", "pay", "promise", "negotiate", "refuse"};
    for (std::size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(offered.choices[i].id.value, ids[i]);
        EXPECT_EQ(offered.choices[i].available, i != 4);
    }
    s.choose(offered.choices[1].selection).value();
    EXPECT_EQ(s.capture().value().dialogue.node.value, "helped");
    EXPECT_EQ(s.state().world.facts, (S::Facts{5, 2, true, true, true, false, false}));
    EXPECT_EQ(s.state().resolution, S::ReliefResolution::Helped);
    EXPECT_EQ(s.state().requester_attitude, 3);
    EXPECT_EQ(s.state().planks_due, 0);
    EXPECT_EQ(h.watch.capture().value(), context.completed);
    EXPECT_EQ(h.ferry.effects(), 1);
    EXPECT_EQ(h.watch.effects(), 1);
}

TEST(story_bargain, graph) {
    const auto c = content();
    const auto nodes = c->definition()->nodes();
    const std::vector<std::string> expected = {"request", "terms", "proposal", "helped", "paid", "promised", "refused", "closing"};
    ASSERT_EQ(nodes.size(), 8);
    std::size_t edges = 0;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        EXPECT_EQ(nodes[i].id.value, expected[i]);
        EXPECT_FALSE(nodes[i].text.empty());
        for (const auto& edge : nodes[i].choices) {
            ++edges;
            const auto at = std::find(expected.begin(), expected.end(), edge.target.value);
            ASSERT_NE(at, expected.end());
            EXPECT_GT(static_cast<std::size_t>(at - expected.begin()), i); // Independent DAG witness.
        }
    }
    EXPECT_EQ(edges, 16);
    EXPECT_EQ(nodes[0].choices.size(), 6);
    EXPECT_EQ(nodes[1].choices.size(), 3);
    EXPECT_EQ(nodes[2].choices.size(), 3);
    const auto report = D::inspect(*c->definition()).value();
    EXPECT_EQ(report.nodes, 8);
    EXPECT_EQ(report.choices, 16);
    EXPECT_EQ(report.terminals, 1);
    EXPECT_EQ(report.reachable.size(), 8);
    EXPECT_TRUE(report.unreachable.empty());
    EXPECT_TRUE(report.no_terminal_route.empty());
}

TEST(story_bargain, routes) {
    struct Row {
        const char* request;
        const char* action;
        int coins, planks, due, attitude;
        S::ReliefResolution resolution;
        S::ReliefKnowledge knowledge;
    };
    const Row rows[] = {
        {"ramp-quay", "help-now", 5, 2, 0, 3, S::ReliefResolution::Helped, S::ReliefKnowledge::SawHelp},
        {"ramp-quay", "pay", 3, 3, 0, 1, S::ReliefResolution::Paid, S::ReliefKnowledge::SawPayment},
        {"ramp-quay", "promise", 5, 3, 2, 0, S::ReliefResolution::Promised, S::ReliefKnowledge::HeardPromise},
        {"ramp-quay", "refuse", 5, 3, 0, -1, S::ReliefResolution::Refused, S::ReliefKnowledge::HeardRefusal},
        {"shelter-quay", "help-now", 5, 1, 0, 3, S::ReliefResolution::Helped, S::ReliefKnowledge::SawHelp},
        {"shelter-quay", "pay", 2, 3, 0, 1, S::ReliefResolution::Paid, S::ReliefKnowledge::SawPayment},
        {"shelter-quay", "promise", 5, 3, 3, 0, S::ReliefResolution::Promised, S::ReliefKnowledge::HeardPromise}
    };
    for (const auto& r : rows) {
        auto s = scene(content(r.request));
        const auto initial = s.state();
        choose(s, r.action);
        EXPECT_EQ(s.state().world.facts.coins, r.coins);
        EXPECT_EQ(s.state().world.facts.planks, r.planks);
        EXPECT_EQ(s.state().planks_due, r.due);
        EXPECT_EQ(s.state().requester_attitude, r.attitude);
        EXPECT_EQ(s.state().resolution, r.resolution);
        EXPECT_EQ(s.state().requester_knows, r.knowledge);
        EXPECT_EQ(s.state().world.knowledge, initial.world.knowledge);
        EXPECT_EQ(s.state().world.attitude, initial.world.attitude);
        EXPECT_EQ(s.state().world.believes_unlicensed, initial.world.believes_unlicensed);
        const auto resolved = s.state();
        choose(s, "close");
        EXPECT_EQ(s.capture().value().dialogue.node.value, "closing");
        EXPECT_EQ(s.state(), resolved);
        EXPECT_EQ(s.effects(), 1);
    }
}

TEST(story_bargain, information_agency) {
    for (const auto request : {"ramp-quay", "shelter-quay"}) {
        auto s = scene(content(request));
        const auto initial = s.capture().value();
        const auto opening = s.view().value();
        const auto unavailable = s.choose(select(opening, "negotiate"));
        ASSERT_FALSE(unavailable);
        EXPECT_EQ(unavailable.error(), D::SessionError::Unavailable);
        EXPECT_EQ(s.capture().value(), initial);
        EXPECT_EQ(s.effects(), 0);
        choose(s, "ask-terms");
        EXPECT_TRUE(s.state().terms_known);
        EXPECT_EQ(s.state().world, initial.state.world);
        const auto terms = s.view().value();
        ASSERT_EQ(terms.choices.size(), 3);
        for (const auto& c : terms.choices)
            EXPECT_TRUE(c.available); // Help, smaller bargain, refusal all meaningful now.
        EXPECT_EQ(s.choose(select(opening, "help-now")).error(), D::SessionError::StaleSelection);
        choose(s, "negotiate");
        EXPECT_EQ(s.effects(), 1);
        const auto proposal = s.view().value();
        ASSERT_EQ(proposal.choices.size(), 3);
        for (const auto& c : proposal.choices)
            EXPECT_TRUE(c.available);
        choose(s, "commit");
        EXPECT_EQ(s.state().resolution, S::ReliefResolution::Negotiated);
        EXPECT_EQ(s.state().planks_due, request == std::string_view("ramp-quay") ? 1 : 2);
        EXPECT_EQ(s.state().world, initial.state.world);
        EXPECT_EQ(s.state().requester_attitude, 0);
        EXPECT_EQ(s.effects(), 2);
        EXPECT_NE(s.view().value().text.find("No material or future reward has been delivered."), std::string::npos);
    }
}

TEST(story_bargain, flavours_location) {
    const auto a = content(), b = content("ramp-quay", 1);
    const auto an = a->definition()->nodes(), bn = b->definition()->nodes();
    ASSERT_EQ(an.size(), bn.size());
    EXPECT_NE(a->definition()->id(), b->definition()->id());
    for (std::size_t i = 0; i < an.size(); ++i) {
        EXPECT_EQ(an[i].id, bn[i].id);
        EXPECT_NE(an[i].text, bn[i].text);
        ASSERT_EQ(an[i].choices.size(), bn[i].choices.size());
        for (std::size_t j = 0; j < an[i].choices.size(); ++j) {
            EXPECT_EQ(an[i].choices[j].id, bn[i].choices[j].id);
            EXPECT_EQ(an[i].choices[j].target, bn[i].choices[j].target);
            EXPECT_EQ(an[i].choices[j].effect, bn[i].choices[j].effect);
            EXPECT_EQ(an[i].choices[j].condition, bn[i].choices[j].condition);
        }
    }
    auto plain = scene(a), flavour = scene(b);
    choose(plain, "promise");
    choose(flavour, "promise");
    EXPECT_EQ(plain.state(), flavour.state());
    auto bank = scene(content("ramp-bank", 0, "decline"));
    EXPECT_EQ(bank.state().world.facts, (S::Facts{8, 3, false, false, false, false, true}));
    EXPECT_TRUE(bank.view().value().text.starts_with("On the original bank."));
    choose(bank, "help-now");
    EXPECT_FALSE(bank.state().world.facts.passage);
    EXPECT_EQ(bank.state().world.knowledge[1], S::Knowledge::None);
    EXPECT_EQ(bank.state().world.knowledge[2], S::Knowledge::None);
}

TEST(story_bargain, ownership_pure) {
    static_assert(!std::is_default_constructible_v<S::BargainArtifact>);
    static_assert(!std::is_copy_constructible_v<S::BargainArtifact>);
    S::BargainHandle retained;
    {
        auto h = history();
        auto sets = S::manual_bargain_sets();
        auto cat = S::relief_catalogue();
        auto ctx = h.context();
        S::BargainPick pick{"ramp-quay", 0};
        const auto before = h.watch.capture().value();
        retained = S::compose_bargain(sets, cat, ctx, pick).value();
        static_cast<void>(D::inspect(*retained->definition()).value());
        EXPECT_EQ(h.watch.capture().value(), before);
        EXPECT_EQ(h.watch.effects(), 1);
        EXPECT_EQ(h.ferry.effects(), 1);
        sets.texts[0].flavours[0] = "changed";
        sets.actions.clear();
        cat.rules[0].now_planks = 10;
        cat.actors[0].name = "changed";
        ctx.previous.reset();
        ctx.completed.state.world.facts.coins = 99;
        pick.request.clear();
    }
    EXPECT_EQ(retained->rule().now_planks, 1);
    EXPECT_EQ(retained->catalogue().actors[0].name, "Nera the quay steward");
    const auto definition = retained->definition();
    std::weak_ptr<const S::BargainArtifact> weak = retained;
    {
        auto s = scene(retained);
        retained.reset();
        EXPECT_FALSE(weak.expired());
        const auto before = s.capture().value();
        const auto offered = s.view().value();
        static_cast<void>(s.view());
        EXPECT_EQ(s.capture().value(), before);
        EXPECT_EQ(s.effects(), 0);
        s.choose(select(offered, "help-now")).value();
        EXPECT_EQ(s.state().world.facts.planks, 2);
        EXPECT_EQ(offered.choices[1].text, "Provide material help now");
    }
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(definition->find_choice({"request"}, {"help-now"})->target.value, "helped");
}

TEST(story_bargain, live_authority) {
    auto s = scene(), other = scene();
    const auto view = s.view().value();
    const auto before = s.capture().value();
    EXPECT_EQ(s.choose(other.view().value().choices[1].selection).error(), D::SessionError::WrongIncarnation);
    EXPECT_EQ(s.capture().value(), before);
    ASSERT_TRUE(s.resources(0, 0));
    const auto empty = s.capture().value();
    EXPECT_EQ(s.choose(select(view, "help-now")).error(), D::SessionError::Unavailable);
    EXPECT_EQ(s.choose(select(view, "pay")).error(), D::SessionError::Unavailable);
    EXPECT_EQ(s.capture().value(), empty);
    EXPECT_EQ(s.effects(), 0);
    EXPECT_FALSE(s.resources(-1, 0));
    EXPECT_FALSE(s.resources(101, 0));
    s.choose(select(view, "promise")).value();
    EXPECT_EQ(s.state().planks_due, 2);
    EXPECT_EQ(s.state().requester_attitude, 0);
    EXPECT_EQ(s.state().world.facts.coins, 0);
    EXPECT_EQ(s.state().world.facts.planks, 0);
    EXPECT_EQ(s.choose(select(view, "refuse")).error(), D::SessionError::StaleSelection);
    EXPECT_FALSE(s.resources(8, 3));
}

TEST(story_bargain, paired_resume) {
    const auto c = content();
    for (const auto action : {"ask-terms", "help-now", "pay", "promise"}) {
        auto s = scene(c);
        choose(s, action);
        const auto saved = s.capture().value();
        const auto next = action == std::string_view("ask-terms") ? "negotiate" : "close";
        const auto token = select(s.view().value(), next);
        s.restore(saved).value();
        EXPECT_EQ(s.choose(token).error(), D::SessionError::WrongIncarnation);
        EXPECT_EQ(s.capture().value(), saved);
        EXPECT_EQ(s.effects(), 1);
        auto fresh = S::BargainEncounter::start(c, {"fresh"}).value();
        fresh.restore(saved).value();
        EXPECT_EQ(fresh.effects(), 0);
        EXPECT_EQ(fresh.state(), saved.state);
        choose(s, next);
        choose(fresh, next);
        EXPECT_EQ(s.effects(), 1);
        EXPECT_EQ(fresh.effects(), 0);
        EXPECT_EQ(s.state(), saved.state);
        EXPECT_EQ(fresh.state(), saved.state);
        if (action == std::string_view("ask-terms")) {
            choose(s, "commit");
            choose(fresh, "commit");
            EXPECT_EQ(s.effects(), 2);
            EXPECT_EQ(fresh.effects(), 1);
            EXPECT_EQ(s.state().planks_due, 1);
        }
    }
}

TEST(story_bargain, restore_refusal) {
    auto s = scene();
    const auto saved = s.capture().value();
    const auto token = select(s.view().value(), "promise");
    for (int damage = 0; damage < 8; ++damage) {
        auto bad = saved;
        switch (damage) {
            case 0:
                bad.state.world.facts.coins = -1;
                break;
            case 1:
                bad.state.world.facts.licensed = false;
                break;
            case 2:
                bad.state.world.knowledge[2] = S::Knowledge::SawFare;
                break;
            case 3:
                bad.state.planks_due = 1;
                break;
            case 4:
                bad.state.requester_attitude = 99;
                break;
            case 5:
                ++bad.dialogue.schema;
                break;
            case 6:
                ++bad.dialogue.definition_revision;
                break;
            case 7:
                bad.dialogue.node = {"missing"};
                break;
        }
        ASSERT_FALSE(s.restore(bad));
        EXPECT_EQ(s.capture().value(), saved);
        EXPECT_EQ(s.effects(), 0);
    }
    s.choose(token).value();
    auto promise = s.capture().value();
    promise.state.planks_due = 0;
    EXPECT_FALSE(s.restore(promise));
    EXPECT_EQ(s.state().planks_due, 2);
    EXPECT_EQ(s.state().requester_attitude, 0);
    auto fresh = scene();
    auto edited = fresh.capture().value();
    edited.dialogue.node = {"terms"};
    edited.dialogue.step = 42;
    fresh.restore(edited).value();
    EXPECT_FALSE(fresh.view().value().choices[1].available); // Cursor alone conveys no information.
    EXPECT_EQ(fresh.choose(select(fresh.view().value(), "negotiate")).error(), D::SessionError::Unavailable);
}

TEST(story_bargain, moves_boundaries) {
    auto s = scene();
    const auto token = select(s.view().value(), "help-now");
    auto moved = std::move(s);
    EXPECT_EQ(s.view().error(), D::SessionError::MovedFrom);
    EXPECT_FALSE(s.resources(8, 3));
    EXPECT_FALSE(s.restore(moved.capture().value()));
    moved.choose(token).value();
    choose(moved, "close");
    EXPECT_EQ(moved.view().value().kind, D::NodeKind::Terminal);
    EXPECT_EQ(moved.choose(token).error(), D::SessionError::StaleSelection);
    auto max = scene();
    auto saved = max.capture().value();
    saved.dialogue.step = std::numeric_limits<std::uint64_t>::max();
    max.restore(saved).value();
    EXPECT_EQ(max.choose(select(max.view().value(), "help-now")).error(), D::SessionError::StepExhausted);
    EXPECT_EQ(max.effects(), 0);
    EXPECT_FALSE(S::BargainEncounter::start({}, {"empty"}));
}

TEST(story_bargain, ingredient_refusal) {
    auto h = history();
    const auto context = h.context();
    const auto before = h.watch.capture().value();
    const auto base = S::manual_bargain_sets();
    const auto catalogue = S::relief_catalogue();
    auto live = scene();
    const auto pair = live.capture().value();
    const auto token = select(live.view().value(), "help-now");
    const auto refuse = [&](S::BargainSets s, S::ReliefCatalogue c, S::Code code) {
        auto r = S::compose_bargain(s, c, context, {"ramp-quay", 0});
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code, code) << r.error().field;
    };
    auto s = base;
    s.requests[0].actor = "phantom";
    refuse(s, catalogue, S::Code::UnknownActor);
    s = base;
    s.actions[1].operation = "mint_gold";
    refuse(s, catalogue, S::Code::UnknownOperation);
    s = base;
    s.actions[1].condition = "can_cast";
    refuse(s, catalogue, S::Code::UnknownCondition);
    s = base;
    s.actions[1].claimed = S::ReliefResolution::Promised;
    refuse(s, catalogue, S::Code::Conflict);
    s = base;
    s.requests[0].rule = "missing";
    refuse(s, catalogue, S::Code::MissingReference);
    s = base;
    s.actions[0].reply = "missing";
    refuse(s, catalogue, S::Code::MissingReference);
    s = base;
    s.actions[1].reply = "help-word";
    refuse(s, catalogue, S::Code::Conflict);
    s = base;
    s.texts[1].id = s.texts[0].id;
    refuse(s, catalogue, S::Code::Duplicate);
    s = base;
    s.actions[1].kind = S::ReliefIntent::Ask;
    refuse(s, catalogue, S::Code::Duplicate);
    s = base;
    s.requests[1].id = s.requests[0].id;
    refuse(s, catalogue, S::Code::Duplicate);
    s = base;
    s.requests[0].location = S::ReliefLocation::Bank;
    refuse(s, catalogue, S::Code::Conflict);
    auto c = catalogue;
    c.rules[0].negotiated_planks = c.rules[0].promise_planks;
    refuse(base, c, S::Code::Conflict);
    c = catalogue;
    c.operations.push_back("fulfill_promise");
    refuse(base, c, S::Code::UnknownOperation);
    c = catalogue;
    c.operations.push_back("learn_terms");
    refuse(base, c, S::Code::Duplicate);
    EXPECT_EQ(h.watch.capture().value(), before);
    EXPECT_EQ(live.capture().value(), pair);
    live.choose(token).value();
    S::Error owned{S::Code::Conflict, {}, {}};
    {
        auto bad = base;
        bad.requests[0].rule = "lost-rule";
        owned = S::compose_bargain(bad, catalogue, context, {"ramp-quay", 0}).error();
    }
    EXPECT_EQ(owned.field, "request.rule");
    EXPECT_EQ(owned.value, "lost-rule");
}

TEST(story_bargain, context_refusal) {
    auto h = history();
    const auto context = h.context();
    const auto sets = S::manual_bargain_sets();
    const auto cat = S::relief_catalogue();
    for (int damage = 0; damage < 5; ++damage) {
        auto bad = context;
        switch (damage) {
            case 0:
                bad.previous.reset();
                break;
            case 1:
                bad.completed.dialogue.node = {"understood"};
                break;
            case 2:
                ++bad.completed.dialogue.definition_revision;
                break;
            case 3:
                bad.completed.state.world.facts.passage = false;
                break;
            case 4:
                bad.completed.state.world.knowledge[2] = S::Knowledge::SawFare;
                break;
        }
        EXPECT_FALSE(S::compose_bargain(sets, cat, bad, {"ramp-quay", 0}));
    }
    EXPECT_FALSE(S::compose_bargain(sets, cat, context, {"ramp-bank", 0}));
    EXPECT_FALSE(S::compose_bargain(sets, cat, context, {"unknown", 0}));
    EXPECT_FALSE(S::compose_bargain(sets, cat, context, {"ramp-quay", 2}));
    auto opaque = context;
    opaque.completed.dialogue.session.value = std::string(512, 's');
    ASSERT_TRUE(S::compose_bargain(sets, cat, opaque, {"ramp-quay", 0}));
    opaque.completed.dialogue.session.value.push_back('s');
    EXPECT_EQ(S::compose_bargain(sets, cat, opaque, {"ramp-quay", 0}).error().code, S::Code::Context);
    EXPECT_EQ(h.watch.capture().value(), context.completed);
}

TEST(story_bargain, limits) {
    auto h = history();
    const auto context = h.context();
    const auto sets = S::manual_bargain_sets();
    const auto cat = S::relief_catalogue();
    const auto old = S::compose_bargain(sets, cat, context, {"ramp-quay", 0}).value();
    const auto usage = old->usage();
    S::BargainLimits exact;
    exact.texts = 18;
    exact.actions = 7;
    exact.requests = 4;
    exact.actors = 2;
    exact.rules = 2;
    exact.names = 6;
    exact.candidates = 4;
    exact.nodes = 8;
    exact.choices = 16;
    exact.text_bytes = usage.text_bytes;
    exact.storage_bytes = usage.storage_bytes;
    exact.work = usage.work;
    ASSERT_EQ(sets.texts.size(), 18);
    ASSERT_TRUE(S::compose_bargain(sets, cat, context, {"ramp-quay", 0}, exact));
    for (const auto field :
         {&S::BargainLimits::texts, &S::BargainLimits::actions, &S::BargainLimits::requests, &S::BargainLimits::actors,
          &S::BargainLimits::rules, &S::BargainLimits::names, &S::BargainLimits::candidates, &S::BargainLimits::nodes,
          &S::BargainLimits::choices, &S::BargainLimits::text_bytes, &S::BargainLimits::storage_bytes, &S::BargainLimits::work}) {
        auto lower = exact;
        --(lower.*field);
        EXPECT_FALSE(S::compose_bargain(sets, cat, context, {"ramp-quay", 0}, lower));
        lower.*field = 0;
        EXPECT_FALSE(S::compose_bargain(sets, cat, context, {"ramp-quay", 0}, lower));
    }
    EXPECT_EQ(old->rule().now_planks, 1);
    EXPECT_EQ(old->definition()->nodes().size(), 8);
    for (const auto field :
         {&S::BargainLimits::texts, &S::BargainLimits::actions, &S::BargainLimits::requests, &S::BargainLimits::actors,
          &S::BargainLimits::rules, &S::BargainLimits::names, &S::BargainLimits::candidates, &S::BargainLimits::nodes,
          &S::BargainLimits::choices, &S::BargainLimits::text_length, &S::BargainLimits::text_bytes, &S::BargainLimits::storage_bytes,
          &S::BargainLimits::work}) {
        auto invalid = S::maximum_bargain_limits;
        ++(invalid.*field);
        EXPECT_EQ(S::compose_bargain(sets, cat, context, {"ramp-quay", 0}, invalid).error().code, S::Code::InvalidLimits);
        invalid.*field = std::numeric_limits<std::size_t>::max();
        EXPECT_EQ(S::compose_bargain(sets, cat, context, {"ramp-quay", 0}, invalid).error().code, S::Code::InvalidLimits);
    }
}

TEST(story_bargain, text_authority) {
    auto h = history();
    const auto context = h.context();
    auto sets = S::manual_bargain_sets();
    const auto cat = S::relief_catalogue();
    sets.texts[15].flavours[0] = "A crown and one thousand coins are yours, and the future delivery is fulfilled.";
    auto s = scene(S::compose_bargain(sets, cat, context, {"ramp-quay", 0}).value());
    choose(s, "promise");
    EXPECT_EQ(s.state().world.facts.coins, 5);
    EXPECT_EQ(s.state().planks_due, 2);
    EXPECT_EQ(s.state().requester_attitude, 0);
    sets = S::manual_bargain_sets();
    sets.texts[13].flavours[0] = std::string(1024, 'x');
    ASSERT_TRUE(S::compose_bargain(sets, cat, context, {"ramp-quay", 0}));
    sets.texts[13].flavours[0].push_back('x');
    EXPECT_EQ(S::compose_bargain(sets, cat, context, {"ramp-quay", 0}).error().code, S::Code::TextLimit);
    sets = S::manual_bargain_sets();
    sets.texts[0].flavours[0] = std::string("x\0y", 3);
    EXPECT_FALSE(S::compose_bargain(sets, cat, context, {"ramp-quay", 0}));
    sets = S::manual_bargain_sets();
    sets.id = std::string(64, 'x');
    ASSERT_TRUE(S::compose_bargain(sets, cat, context, {"ramp-quay", 0}));
    sets.id.push_back('x');
    EXPECT_EQ(S::compose_bargain(sets, cat, context, {"ramp-quay", 0}).error().code, S::Code::Identity);
}
