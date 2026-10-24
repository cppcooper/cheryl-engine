#include "../src/selection.h"
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
        for (const auto& row : v.choices)
            if (row.id.value == id)
                return row.selection;
        throw std::logic_error("Missing independent selected choice");
    }
    void choose(S::BargainEncounter& s, std::string_view id) {
        s.choose(token(s.view().value(), id)).value();
    }
    S::BargainContext context(std::string_view prior = "pay") {
        auto graph = S::project(S::accept(S::manual_sets()[0], S::ferry_catalogue(), {}).value(), 0).value();
        auto ferry = S::Encounter::start(graph, {"ferry"}).value();
        ferry.choose(token(ferry.view().value(), prior)).value();
        ferry.choose(token(ferry.view().value(), "continue")).value();
        auto content = S::compose_watch(S::manual_library(), graph, ferry.capture().value(), 0).value();
        auto watch = S::WatchEncounter::start(content, {"watch"}).value();
        watch.choose(token(watch.view().value(), prior == "decline" ? "withhold" : content->recipe().action)).value();
        if (prior != "decline")
            watch.choose(token(watch.view().value(), "ask-again")).value();
        return {content, watch.capture().value()};
    }
    auto selected(std::uint64_t seed = 0, const S::SelectionConstraints& c = {}) {
        return S::select_bargain(S::selection_library(), S::selection_catalogue(), context(), seed, 1, c).value();
    }
}

TEST(story_selection, caller) {
    const auto chosen = selected();
    const auto& d = chosen->descriptor();
    EXPECT_EQ(d.algorithm, 1);
    EXPECT_EQ(d.seed, 0);
    EXPECT_EQ(d.examined, 32);
    EXPECT_EQ(d.compatible, 12);
    EXPECT_EQ(d.request, "shelter-quay");
    EXPECT_EQ(d.proposition, "shelter-request");
    EXPECT_EQ(d.plan, "ramp");
    EXPECT_EQ(d.actor, "nera");
    EXPECT_EQ(d.flavour, 1);
    EXPECT_EQ(d.bundle, S::ReliefBundle::ExplainFirst);
    EXPECT_EQ(chosen->content()->request().rule, "ramp"); // A REAL cross-request recombination, not the prewritten shelter plan.
    auto scene = S::BargainEncounter::start(chosen->content(), {"relief-seeded-01"}).value();
    const auto view = scene.view().value();
    ASSERT_EQ(view.choices.size(), 2);
    EXPECT_EQ(view.choices[0].id.value, "ask-terms");
    EXPECT_EQ(view.choices[1].id.value, "refuse");
    EXPECT_TRUE(view.choices[0].available && view.choices[1].available);
    EXPECT_EQ(scene.effects(), 0);
    EXPECT_EQ(view.text, "On the harbor quay. Rain is coming through the shelter roof.");
}

TEST(story_selection, literal_seeds) {
    // Independently computed SplitMix64 first outputs: e220a8397b1dcdaf, 910a2dec89025cc1, 975835de1c9756ce, 1d0b14e4db018fed.
    struct Expected {
        const char* request;
        const char* plan;
        S::ReliefBundle bundle;
        std::size_t flavour;
    };
    const Expected rows[] = {
        {"shelter-quay", "ramp", S::ReliefBundle::ExplainFirst, 1},
        {"shelter-quay", "ramp", S::ReliefBundle::OpenOffer, 1},
        {"shelter-quay", "shelter", S::ReliefBundle::ExplainFirst, 0},
        {"shelter-quay", "shelter", S::ReliefBundle::OpenOffer, 1}
    };
    for (std::uint64_t seed = 0; seed < 4; ++seed) {
        const auto a = selected(seed), b = selected(seed);
        const auto& d = a->descriptor();
        EXPECT_EQ(d, b->descriptor());
        EXPECT_EQ(d.request, rows[seed].request);
        EXPECT_EQ(d.plan, rows[seed].plan);
        EXPECT_EQ(d.bundle, rows[seed].bundle);
        EXPECT_EQ(d.flavour, rows[seed].flavour);
        EXPECT_EQ(S::compare_bargain(*a->content()).presentation, S::compare_bargain(*b->content()).presentation);
    }
}

TEST(story_selection, mechanical_diversity) {
    std::vector<S::BargainMechanics> meanings;
    for (std::uint64_t seed = 0; seed < 8; ++seed) {
        auto m = S::compare_bargain(*selected(seed)->content()).mechanics;
        if (std::find(meanings.begin(), meanings.end(), m) == meanings.end())
            meanings.push_back(std::move(m));
    }
    EXPECT_EQ(meanings.size(), 4); // Two real cost/obligation plans x two decision structures, not eight different seeds.
    EXPECT_EQ(selected(2)->descriptor().plan, selected(4)->descriptor().plan);
    EXPECT_EQ(S::compare_bargain(*selected(2)->content()).mechanics, S::compare_bargain(*selected(4)->content()).mechanics);
}

TEST(story_selection, constraints_change_play) {
    S::SelectionConstraints direct;
    direct.required_initial = 1u << static_cast<unsigned>(S::ReliefIntent::Help);
    auto expensive = selected(1, direct);
    direct.max_material = 1;
    direct.max_coins = 2;
    direct.max_pending = 2;
    auto small = selected(1, direct);
    EXPECT_EQ(expensive->descriptor().plan, "shelter");
    EXPECT_EQ(small->descriptor().plan, "ramp");
    auto a = S::BargainEncounter::start(expensive->content(), {"a"}).value(),
         b = S::BargainEncounter::start(small->content(), {"b"}).value();
    ASSERT_EQ(a.view()->choices.size(), 6);
    ASSERT_EQ(b.view()->choices.size(), 6);
    choose(a, "help-now");
    choose(b, "help-now");
    EXPECT_EQ(a.state().world.facts.planks, 1);
    EXPECT_EQ(b.state().world.facts.planks, 2);
    EXPECT_EQ(a.state().requester_attitude, 3);
    EXPECT_EQ(b.state().requester_attitude, 3);
    const auto lib = S::selection_library();
    auto host = S::selection_catalogue();
    const auto ctx = context();
    auto single = lib;
    single.sets.requests.resize(1);
    single.bundles = {S::ReliefBundle::OpenOffer};
    single.flavours = {0};
    host.plans = {{"ramp", 1}};
    const auto r1 = S::select_bargain(single, host, ctx, 0, 1).value();
    host.plans = {{"shelter", 1}}; // The host explicitly approves this larger plan for the access goal.
    const auto r2 = S::select_bargain(single, host, ctx, 0, 1).value();
    EXPECT_NE(r1->content()->definition()->id(), r2->content()->definition()->id());
    auto first = S::BargainEncounter::start(r1->content(), {"same"}).value(),
         second = S::BargainEncounter::start(r2->content(), {"same"}).value();
    const auto before = second.capture().value(), wrong = first.capture().value();
    const auto authority = token(second.view().value(), "refuse");
    EXPECT_FALSE(second.restore(wrong));
    EXPECT_EQ(second.capture().value(), before);
    ASSERT_TRUE(second.choose(authority));
    S::SelectionConstraints consult;
    consult.edges = 14;
    auto c = S::BargainEncounter::start(selected(1, consult)->content(), {"c"}).value();
    EXPECT_EQ(c.view()->choices.size(), 2);
    choose(c, "ask-terms");
    const auto after = c.view().value();
    EXPECT_EQ(after.choices.size(), 5);
    EXPECT_TRUE(after.choices[0].available && after.choices[2].available);
    EXPECT_EQ(after.choices[2].id.value, "promise");
    choose(c, "promise");
    EXPECT_EQ(c.state().planks_due, 3);
    EXPECT_EQ(c.state().world.facts.planks, 3); // Pending, never fulfilled.
}

TEST(story_selection, representative_routes) {
    for (const auto row : {std::pair{5u, 2}, std::pair{2u, 1}}) {
        auto artifact = selected(row.first);
        auto s = S::BargainEncounter::start(artifact->content(), {"play"}).value();
        choose(s, "ask-terms");
        choose(s, "help-now");
        choose(s, "close");
        EXPECT_EQ(s.state().world.facts.coins, 5);
        EXPECT_EQ(s.state().world.facts.planks, row.second);
        EXPECT_EQ(s.state().resolution, S::ReliefResolution::Helped);
        EXPECT_EQ(s.state().planks_due, 0);
        EXPECT_EQ(s.capture()->dialogue.node.value, "closing");
        EXPECT_EQ(s.effects(), 2);
        EXPECT_EQ(s.state().world.knowledge[1], S::Knowledge::VerifiedFare);
        EXPECT_EQ(s.state().world.knowledge[2], S::Knowledge::None);
    }
    auto s = S::BargainEncounter::start(selected(0)->content(), {"promise"}).value();
    choose(s, "ask-terms");
    choose(s, "negotiate");
    choose(s, "commit");
    EXPECT_EQ(s.state().planks_due, 1);
    EXPECT_EQ(s.state().world.facts.planks, 3);
    EXPECT_EQ(s.state().world.facts.coins, 5);
    EXPECT_EQ(s.state().resolution, S::ReliefResolution::Negotiated);
    auto refused = S::BargainEncounter::start(selected(0)->content(), {"no"}).value();
    choose(refused, "refuse");
    EXPECT_EQ(refused.state().world.facts.planks, 3);
    EXPECT_EQ(refused.state().planks_due, 0);
    EXPECT_EQ(refused.state().requester_attitude, -1);
}

TEST(story_selection, flavour_and_cosmetic_identity) {
    auto lib = S::selection_library();
    auto cat = S::selection_catalogue();
    auto ctx = context();
    lib.sets.requests.resize(1);
    lib.bundles = {S::ReliefBundle::ExplainFirst};
    lib.flavours = {0};
    const auto a = S::select_bargain(lib, cat, ctx, 0, 1).value();
    lib.flavours = {1};
    const auto b = S::select_bargain(lib, cat, ctx, 0, 1).value();
    const auto ar = S::compare_bargain(*a->content()), br = S::compare_bargain(*b->content());
    EXPECT_EQ(ar.mechanics, br.mechanics);
    EXPECT_NE(ar.presentation, br.presentation);
    lib.sets.requests[0].id = "cosmetic-request";
    cat.host.rules[0].id = "cosmetic-plan";
    lib.sets.requests[0].rule = "cosmetic-plan";
    cat.plans[0].id = "cosmetic-plan";
    const auto c = S::select_bargain(lib, cat, ctx, 0, 1).value();
    EXPECT_NE(c->descriptor().request, b->descriptor().request);
    EXPECT_NE(c->descriptor().plan, b->descriptor().plan);
    EXPECT_EQ(S::compare_bargain(*c->content()).mechanics, br.mechanics);
    EXPECT_NE(S::compare_bargain(*c->content()).presentation, br.presentation);
}

TEST(story_selection, ownership_and_purity) {
    S::SelectedBargainHandle retained;
    S::BargainComparison owned;
    {
        auto lib = S::selection_library();
        auto cat = S::selection_catalogue();
        auto ctx = context();
        const auto old = ctx.completed;
        retained = S::select_bargain(lib, cat, ctx, 0, 1).value();
        EXPECT_EQ(ctx.completed, old);
        owned = S::compare_bargain(*retained->content());
        const auto r = D::inspect(*retained->content()->definition()).value();
        EXPECT_EQ(r.reachable.size(), 8);
        EXPECT_EQ(r.choices, 14);
        lib.sets.texts[1].flavours[1] = "mutated under same revision";
        lib.goals.clear();
        cat.host.rules[0].now_planks = 10;
        ctx.completed.state.world.facts.coins = 0;
        EXPECT_EQ(retained->content()->rule().now_planks, 1);
        EXPECT_EQ(retained->content()->context().completed, old);
    }
    EXPECT_EQ(S::compare_bargain(*retained->content()).presentation, owned.presentation);
    std::weak_ptr<const D::Definition> released = retained->content()->definition();
    {
        auto scene = S::BargainEncounter::start(retained->content(), {"owned"}).value();
        retained.reset();
        EXPECT_EQ(scene.effects(), 0);
        EXPECT_NE(scene.view()->text.find("Rain is coming"), std::string::npos);
        choose(scene, "ask-terms");
        choose(scene, "help-now");
        EXPECT_EQ(scene.state().world.facts.planks, 2);
    }
    EXPECT_TRUE(released.expired());
    EXPECT_NE(owned.presentation[2].find("Rain is coming"), std::string::npos);
    static_assert(!std::is_copy_constructible_v<S::SelectedBargain>);
}

TEST(story_selection, paired_resume_authority) {
    const auto c = selected(0);
    auto a = S::BargainEncounter::start(c->content(), {"pair"}).value();
    choose(a, "ask-terms");
    const auto saved = a.capture().value();
    const auto old = token(a.view().value(), "help-now");
    auto damaged = saved;
    damaged.state.planks_due = 1;
    EXPECT_FALSE(a.restore(damaged));
    EXPECT_EQ(a.capture().value(), saved);
    ASSERT_TRUE(a.choose(old));
    EXPECT_EQ(a.effects(), 2);
    a.restore(saved).value();
    const auto stale = a.choose(old);
    ASSERT_FALSE(stale);
    EXPECT_EQ(stale.error(), D::SessionError::WrongIncarnation);
    auto b = S::BargainEncounter::start(c->content(), {"fresh"}).value();
    b.restore(saved).value();
    EXPECT_EQ(b.effects(), 0);
    choose(b, "negotiate");
    choose(b, "commit");
    EXPECT_EQ(b.state().planks_due, 1);
    EXPECT_EQ(b.state().world.facts.coins, 5);
    EXPECT_EQ(b.effects(), 1);
    EXPECT_EQ(b.state().world.knowledge[2], S::Knowledge::None);
}

TEST(story_selection, live_resource_and_foreign_tokens) {
    S::SelectionConstraints c;
    c.required_initial = 1u << static_cast<unsigned>(S::ReliefIntent::Help);
    const auto content = selected(1, c)->content();
    auto a = S::BargainEncounter::start(content, {"same"}).value(), b = S::BargainEncounter::start(content, {"same"}).value();
    const auto offered = a.view().value();
    const auto choice = token(offered, "help-now");
    auto foreign = b.choose(choice);
    ASSERT_FALSE(foreign);
    EXPECT_EQ(foreign.error(), D::SessionError::WrongIncarnation);
    ASSERT_TRUE(a.resources(0, 0));
    const auto pair = a.capture().value();
    const auto r = a.choose(choice);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error(), D::SessionError::Unavailable);
    EXPECT_EQ(a.capture().value(), pair);
    EXPECT_EQ(a.effects(), 0);
}

TEST(story_selection, empty_versus_incomplete) {
    auto lib = S::selection_library();
    auto cat = S::selection_catalogue();
    auto ctx = context();
    S::SelectionConstraints c;
    c.max_pending = 0;
    auto empty = S::select_bargain(lib, cat, ctx, 0, 1, c);
    ASSERT_FALSE(empty);
    EXPECT_EQ(empty.error().code, S::SelectionCode::Empty);
    S::SelectionLimits l;
    l.work = 0;
    auto incomplete = S::select_bargain(lib, cat, ctx, 0, 1, c, l);
    ASSERT_FALSE(incomplete);
    EXPECT_EQ(incomplete.error().code, S::SelectionCode::WorkLimit);
    l = {};
    l.draws = 0;
    auto draw = S::select_bargain(lib, cat, ctx, 0, 1, {}, l);
    ASSERT_FALSE(draw);
    EXPECT_EQ(draw.error().code, S::SelectionCode::DrawLimit);
    cat.roles[0].goals = 1;
    lib.sets.requests.erase(lib.sets.requests.begin(), lib.sets.requests.begin() + 2);
    auto roles = S::select_bargain(lib, cat, ctx, 0, 1);
    ASSERT_FALSE(roles);
    EXPECT_EQ(roles.error().code, S::SelectionCode::Empty);
}

TEST(story_selection, context_and_eager_refusal) {
    auto lib = S::selection_library();
    auto cat = S::selection_catalogue();
    auto ctx = context("decline");
    auto bank = S::select_bargain(lib, cat, ctx, 0, 1).value();
    EXPECT_EQ(bank->descriptor().actor, "oren");
    EXPECT_EQ(bank->content()->request().location, S::ReliefLocation::Bank);
    EXPECT_FALSE(bank->content()->context().completed.state.world.facts.passage);
    ctx.completed.state.world.facts.licensed = true;
    auto invalid = S::select_bargain(lib, cat, ctx, 0, 1);
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().code, S::SelectionCode::Composition);
    EXPECT_TRUE(invalid.error().detail.has_value());
    ctx = context();
    S::SelectionConstraints empty;
    empty.max_pending = 0;
    lib.sets.actions[0].operation = "invent-reward";
    auto eager = S::select_bargain(lib, cat, ctx, 0, 1, empty);
    ASSERT_FALSE(eager);
    EXPECT_EQ(eager.error().code, S::SelectionCode::Composition);
    EXPECT_EQ(eager.error().detail->code, S::Code::UnknownOperation);
}

TEST(story_selection, metadata_and_constraints) {
    const auto original = S::selection_library();
    const auto cat = S::selection_catalogue();
    const auto ctx = context();
    for (int k = 0; k < 7; ++k) {
        auto lib = original;
        auto host = cat;
        switch (k) {
            case 0:
                lib.goals[0].proposition = "unknown";
                break;
            case 1:
                lib.goals.push_back(lib.goals[0]);
                break;
            case 2:
                host.roles[0].id = "unknown-observer";
                break;
            case 3:
                host.plans[0].goals = 4;
                break;
            case 4:
                lib.bundles[1] = lib.bundles[0];
                break;
            case 5:
                lib.flavours[1] = 0;
                break;
            case 6:
                lib.goals.erase(lib.goals.begin());
                break;
        }
        const auto result = S::select_bargain(lib, host, ctx, 0, 1);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, S::SelectionCode::InvalidLibrary);
    }
    for (auto c : {S::SelectionConstraints{128}, S::SelectionConstraints{0, 1}, S::SelectionConstraints{0, 8}}) {
        auto result = S::select_bargain(original, cat, ctx, 0, 1, c);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, S::SelectionCode::InvalidConstraints);
    }
    auto badversion = S::select_bargain(original, cat, ctx, 0, 2);
    ASSERT_FALSE(badversion);
    EXPECT_EQ(badversion.error().code, S::SelectionCode::InvalidConstraints);
}

TEST(story_selection, exact_budgets_and_overflow) {
    const auto lib = S::selection_library();
    const auto cat = S::selection_catalogue();
    const auto ctx = context();
    const auto previous = S::select_bargain(lib, cat, ctx, 0, 1).value();
    const auto descriptor = previous->descriptor();
    const auto u = previous->usage();
    S::SelectionLimits l;
    l.candidates = 32;
    l.work = u.work;
    l.storage_bytes = u.storage_bytes;
    l.text_bytes = u.text_bytes;
    ASSERT_TRUE(S::select_bargain(lib, cat, ctx, 0, 1, {}, l));
    for (int k = 0; k < 4; ++k) {
        auto short_limit = l;
        if (k == 0)
            --short_limit.candidates;
        if (k == 1)
            --short_limit.work;
        if (k == 2)
            --short_limit.storage_bytes;
        if (k == 3)
            --short_limit.text_bytes;
        auto r = S::select_bargain(lib, cat, ctx, 0, 1, {}, short_limit);
        ASSERT_FALSE(r);
        const S::SelectionCode codes[] = {
            S::SelectionCode::CandidateLimit, S::SelectionCode::WorkLimit, S::SelectionCode::StorageLimit, S::SelectionCode::TextLimit
        };
        EXPECT_EQ(r.error().code, codes[k]);
        EXPECT_EQ(previous->descriptor(), descriptor);
    }
    for (auto f :
         {&S::SelectionLimits::candidates, &S::SelectionLimits::draws, &S::SelectionLimits::work, &S::SelectionLimits::text_bytes,
          &S::SelectionLimits::storage_bytes}) {
        auto bad = S::SelectionLimits{};
        bad.*f = std::numeric_limits<std::size_t>::max();
        auto r = S::select_bargain(lib, cat, ctx, 0, 1, {}, bad);
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code, S::SelectionCode::InvalidLimits);
    }
    l = {};
    l.composition.work = std::numeric_limits<std::size_t>::max();
    auto r = S::select_bargain(lib, cat, ctx, 0, 1, {}, l);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code, S::SelectionCode::InvalidLimits);
    auto boundary = lib;
    boundary.goals[0].proposition = std::string(64, 'a');
    boundary.sets.texts[0].id = boundary.goals[0].proposition;
    boundary.sets.requests[0].proposition = boundary.goals[0].proposition;
    boundary.sets.requests[1].proposition = boundary.goals[0].proposition;
    EXPECT_TRUE(S::select_bargain(boundary, cat, ctx, 0, 1));
    auto longlib = lib;
    longlib.goals[0].proposition = std::string(65, 'a');
    auto too_long = S::select_bargain(longlib, cat, ctx, 0, 1);
    ASSERT_FALSE(too_long);
    EXPECT_EQ(too_long.error().code, S::SelectionCode::InvalidLibrary);
}

TEST(story_selection, ordering_and_content_changes) {
    auto lib = S::selection_library();
    auto cat = S::selection_catalogue();
    auto ctx = context();
    const auto old = S::select_bargain(lib, cat, ctx, 0, 1).value();
    std::reverse(lib.bundles.begin(), lib.bundles.end());
    const auto reordered = S::select_bargain(lib, cat, ctx, 0, 1).value();
    EXPECT_EQ(reordered->descriptor().bundle, S::ReliefBundle::OpenOffer);
    EXPECT_NE(reordered->descriptor(), old->descriptor());
    lib.sets.texts[1].flavours[1] = "A new shelter wording under the same nominal revision.";
    const auto changed = S::select_bargain(lib, cat, ctx, 0, 1).value();
    EXPECT_EQ(changed->descriptor(), reordered->descriptor()); // Revision/seed descriptor is NOT a content digest.
    EXPECT_NE(S::compare_bargain(*changed->content()).presentation, S::compare_bargain(*reordered->content()).presentation);
    EXPECT_EQ(old->content()->sets().texts[1].flavours[1], "Rain is coming through the shelter roof.");
}
