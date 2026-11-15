#include "../src/arc.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace {
    namespace A = OfflineStory::Arc;
    namespace S = OfflineStory;
    namespace D = CE::Dialogue;
    A::Context context() {
        A::Context c;
        c.recipient = "steward";
        c.location = "quay";
        c.material = "planks";
        c.actors = {{"steward", "quay", A::ActorRole::Recipient, 0},
                    {"witness", "quay", A::ActorRole::Witness, A::evidence_known}};
        c.society = {"harbor", "mixed-register", 0};
        return c;
    }
    std::vector<A::Component> pieces() {
        // Literal authored fixture; do not call production fixture or reconstruct expectations with its rewrite.
        const A::Scope scope{};
        return {
            {"need", "steward", "steward", "quay", "planks", "harbor", "A crossing needs interim access.",
             A::Role::Need, A::Action::NoticeNeed, 3, A::need_exists, 0, 0, 0, scope},
            {"promise", "steward", "steward", "quay", "planks", "harbor", "Promise two planks or decline.",
             A::Role::Commitment, A::Action::PromiseOrRefuse, 3, 0, 0, 2, 1, scope},
            {"handoff", "steward", "steward", "quay", "planks", "harbor", "Only a checked host handoff settles the promise.",
             A::Role::Settlement, A::Action::CheckHandoff, 3, 0, 0, 0, 0, scope},
            {"response", "steward", "steward", "quay", "planks", "harbor", "Respond only to the observed outcome.",
             A::Role::Response, A::Action::Respond, 3, 0, A::outcome_known, 0, 0, scope}
        };
    }
    A::Component evidence() {
        return {"record", "witness", "steward", "quay", "planks", "harbor", "Present the checked material record or withhold it.",
                A::Role::Evidence, A::Action::ShareEvidence, 3, A::evidence_available, 0, 0, 0, {}};
    }
    A::Component renegotiation() {
        return {"revision", "steward", "steward", "quay", "planks", "harbor", "The informed offer can request one plank.",
                A::Role::Renegotiation, A::Action::ReviseTerms, 3, 0, A::evidence_known, 0, 0, {}};
    }
    A::Handle direct() { return A::compose_direct(pieces(), context()).value(); }
    A::Handle informed() { return A::introduce_evidence(*direct(), evidence(), renegotiation()).value(); }
    A::Draft literal_direct() {
        return {pieces(), {{0, 1, A::Link::Continue}, {1, 2, A::Link::Promise}, {1, 3, A::Link::Refuse},
                           {2, 3, A::Link::Deliver}, {2, 3, A::Link::Break}, {2, 3, A::Link::Leave}}};
    }
    A::Shape direct_shape() {
        return {{{A::Role::Need, A::Action::NoticeNeed, 1, 0},
                 {A::Role::Commitment, A::Action::PromiseOrRefuse, 0, 0},
                 {A::Role::Settlement, A::Action::CheckHandoff, 0, 0},
                 {A::Role::Response, A::Action::Respond, 0, 4}},
                {{A::Role::Need, A::Role::Commitment, A::Link::Continue},
                 {A::Role::Commitment, A::Role::Settlement, A::Link::Promise},
                 {A::Role::Commitment, A::Role::Response, A::Link::Refuse},
                 {A::Role::Settlement, A::Role::Response, A::Link::Deliver},
                 {A::Role::Settlement, A::Role::Response, A::Link::Break},
                 {A::Role::Settlement, A::Role::Response, A::Link::Leave}}};
    }
    D::Selection token(const D::View& v, std::string_view id) {
        for (const auto& row : v.choices)
            if (row.id.value == id)
                return row.selection;
        throw std::logic_error("Missing independent live witness choice");
    }
}

TEST(story_arc, direct) {
    const auto c = context();
    const auto p = pieces();
    const auto accepted = A::compose_direct(p, c);
    ASSERT_TRUE(accepted);
    EXPECT_EQ((*accepted)->shape(), direct_shape());
    EXPECT_EQ((*accepted)->draft(), literal_direct());
    EXPECT_EQ(c, context());
    EXPECT_EQ(p, pieces());
    const auto& traces = (*accepted)->traces();
    ASSERT_EQ(traces.size(), 4);
    const std::vector<A::Trace> expected{
        {{A::Role::Need, A::Role::Commitment, A::Role::Settlement, A::Role::Response},
         {A::Link::Continue, A::Link::Promise, A::Link::Deliver}, S::Obligation::Fulfilled, 2, 0, 4, true},
        {{A::Role::Need, A::Role::Commitment, A::Role::Settlement, A::Role::Response},
         {A::Link::Continue, A::Link::Promise, A::Link::Break}, S::Obligation::Broken, 2, 0, 4, false},
        {{A::Role::Need, A::Role::Commitment, A::Role::Settlement, A::Role::Response},
         {A::Link::Continue, A::Link::Promise, A::Link::Leave}, S::Obligation::Pending, 2, 2, 4, false},
        {{A::Role::Need, A::Role::Commitment, A::Role::Response},
         {A::Link::Continue, A::Link::Refuse}, S::Obligation::None, 0, 0, 4, false}
    };
    EXPECT_EQ(traces, expected); // Conditional previews, not certified world states.
}

TEST(story_arc, evidence) {
    const auto original = direct();
    const auto before = original->draft();
    const auto accepted = A::introduce_evidence(*original, evidence(), renegotiation());
    ASSERT_TRUE(accepted);
    EXPECT_EQ(original->draft(), before);
    const A::Shape expected{
        {{A::Role::Need, A::Action::NoticeNeed, 1, 0},
         {A::Role::Evidence, A::Action::ShareEvidence, 2, 0},
         {A::Role::Renegotiation, A::Action::ReviseTerms, 0, 1},
         {A::Role::Commitment, A::Action::PromiseOrRefuse, 0, 0},
         {A::Role::Settlement, A::Action::CheckHandoff, 0, 0},
         {A::Role::Response, A::Action::Respond, 0, 4}},
        {{A::Role::Need, A::Role::Evidence, A::Link::Continue},
         {A::Role::Evidence, A::Role::Renegotiation, A::Link::Present},
         {A::Role::Evidence, A::Role::Commitment, A::Link::Withhold},
         {A::Role::Renegotiation, A::Role::Commitment, A::Link::Revised},
         {A::Role::Commitment, A::Role::Settlement, A::Link::Promise},
         {A::Role::Commitment, A::Role::Response, A::Link::Refuse},
         {A::Role::Settlement, A::Role::Response, A::Link::Deliver},
         {A::Role::Settlement, A::Role::Response, A::Link::Break},
         {A::Role::Settlement, A::Role::Response, A::Link::Leave}}
    };
    EXPECT_EQ((*accepted)->shape(), expected);
    EXPECT_NE((*accepted)->shape(), original->shape());
    const auto& t = (*accepted)->traces();
    ASSERT_EQ(t.size(), 8);
    const std::vector<A::Trace> expected_traces{
        {{A::Role::Need, A::Role::Evidence, A::Role::Renegotiation, A::Role::Commitment,
          A::Role::Settlement, A::Role::Response},
         {A::Link::Continue, A::Link::Present, A::Link::Revised, A::Link::Promise, A::Link::Deliver},
         S::Obligation::Fulfilled, 1, 0, 7, true},
        {{A::Role::Need, A::Role::Evidence, A::Role::Renegotiation, A::Role::Commitment,
          A::Role::Settlement, A::Role::Response},
         {A::Link::Continue, A::Link::Present, A::Link::Revised, A::Link::Promise, A::Link::Break},
         S::Obligation::Broken, 1, 0, 7, false},
        {{A::Role::Need, A::Role::Evidence, A::Role::Renegotiation, A::Role::Commitment,
          A::Role::Settlement, A::Role::Response},
         {A::Link::Continue, A::Link::Present, A::Link::Revised, A::Link::Promise, A::Link::Leave},
         S::Obligation::Pending, 1, 1, 7, false},
        {{A::Role::Need, A::Role::Evidence, A::Role::Renegotiation, A::Role::Commitment, A::Role::Response},
         {A::Link::Continue, A::Link::Present, A::Link::Revised, A::Link::Refuse},
         S::Obligation::None, 0, 0, 7, false},
        {{A::Role::Need, A::Role::Evidence, A::Role::Commitment, A::Role::Settlement, A::Role::Response},
         {A::Link::Continue, A::Link::Withhold, A::Link::Promise, A::Link::Deliver},
         S::Obligation::Fulfilled, 2, 0, 4, true},
        {{A::Role::Need, A::Role::Evidence, A::Role::Commitment, A::Role::Settlement, A::Role::Response},
         {A::Link::Continue, A::Link::Withhold, A::Link::Promise, A::Link::Break},
         S::Obligation::Broken, 2, 0, 4, false},
        {{A::Role::Need, A::Role::Evidence, A::Role::Commitment, A::Role::Settlement, A::Role::Response},
         {A::Link::Continue, A::Link::Withhold, A::Link::Promise, A::Link::Leave},
         S::Obligation::Pending, 2, 2, 4, false},
        {{A::Role::Need, A::Role::Evidence, A::Role::Commitment, A::Role::Response},
         {A::Link::Continue, A::Link::Withhold, A::Link::Refuse},
         S::Obligation::None, 0, 0, 4, false}
    };
    EXPECT_EQ(t, expected_traces);
    EXPECT_EQ(A::introduce_evidence(*original, evidence(), renegotiation()).value()->traces(), t);
    auto twice = A::introduce_evidence(**accepted, evidence(), renegotiation());
    ASSERT_FALSE(twice);
    EXPECT_EQ(twice.error().code, A::Code::Role);
}

TEST(story_arc, equivalent) {
    const auto base = informed();
    auto copy = base->draft();
    auto c = base->context();
    c.recipient = "keeper";
    c.location = "courtyard";
    c.material = "stone";
    c.society.id = "guild";
    c.society.speaker_register = "exclusive-register";
    c.actors = {{"keeper", "courtyard", A::ActorRole::Recipient, 0},
                {"scribe", "courtyard", A::ActorRole::Witness, 1}};
    // Reverse node order AND remap every endpoint. Cosmetic permutation is not a new graph.
    std::reverse(copy.nodes.begin(), copy.nodes.end());
    for (auto& edge : copy.edges) {
        edge.from = 5 - edge.from;
        edge.to = 5 - edge.to;
    }
    std::reverse(copy.edges.begin(), copy.edges.end());
    for (auto& n : copy.nodes) {
        n.id = "copy-" + n.id;
        n.actor = n.role == A::Role::Evidence ? "scribe" : "keeper";
        n.recipient = "keeper";
        n.location = "courtyard";
        n.material = "stone";
        n.society = "guild";
        n.text = "A paraphrase of the same approved meaning.";
    }
    const auto accepted = A::accept(copy, c);
    ASSERT_TRUE(accepted);
    EXPECT_EQ((*accepted)->shape(), base->shape());
    EXPECT_EQ((*accepted)->mechanics(), base->mechanics());
    EXPECT_EQ((*accepted)->traces(), base->traces());
    EXPECT_NE((*accepted)->draft(), base->draft());
}

TEST(story_arc, mechanics) {
    const auto original = direct();
    auto p = pieces();
    p[1].ordinary_due = 3;
    const auto more = A::compose_direct(p, context());
    ASSERT_TRUE(more);
    EXPECT_EQ((*more)->shape(), original->shape());
    EXPECT_NE((*more)->mechanics(), original->mechanics());
    EXPECT_EQ((*more)->traces()[2].remaining_due, 3);
    auto c = context();
    c.goal = A::Goal::Shelter; // Same-role swap is approved by the host's goal mask, not by its text.
    p[0].id = "shelter-need";
    p[0].text = "Interim access supports shelter relief; no completed shelter is claimed.";
    const auto swapped = A::compose_direct(p, c);
    ASSERT_TRUE(swapped);
    EXPECT_EQ((*swapped)->shape(), original->shape());
    p[0].goals = 1;
    const auto incompatible = A::compose_direct(p, c);
    ASSERT_FALSE(incompatible);
    EXPECT_EQ(incompatible.error().code, A::Code::Compatibility);
}

TEST(story_arc, information) {
    auto c = context();
    c.actors[1].information = 0;
    const auto a = A::compose_direct(pieces(), c);
    ASSERT_TRUE(a); // A valid direct setup isolates the later witness-information rejection.
    const auto unknown = A::introduce_evidence(**a, evidence(), renegotiation());
    ASSERT_FALSE(unknown);
    EXPECT_EQ(unknown.error().code, A::Code::Information);
    auto draft = informed()->draft();
    draft.nodes[5].required_information = 0;
    const auto leaked = A::accept(draft, context());
    ASSERT_FALSE(leaked);
    EXPECT_EQ(leaked.error().code, A::Code::Information);
    draft = informed()->draft();
    draft.edges[6].link = A::Link::Withhold; // Remove Present: no revised offer on unshared evidence.
    const auto wrong_link = A::accept(draft, context());
    ASSERT_FALSE(wrong_link);
    EXPECT_EQ(wrong_link.error().code, A::Code::Topology);
}

TEST(story_arc, scope) {
    const auto original = direct();
    auto c = context();
    c.presentation = {"fr", 100, 96, true};
    const auto help = A::compose_direct(pieces(), c);
    ASSERT_TRUE(help);
    EXPECT_EQ((*help)->shape(), original->shape());
    EXPECT_EQ((*help)->mechanics(), original->mechanics());
    EXPECT_EQ((*help)->traces(), original->traces());
    EXPECT_EQ((*help)->context().society, original->context().society);
    auto p = pieces();
    p[2].scope = {A::mature, 1, true, 1, 3}; // Metadata only; no adult prose and no deletion of settlement.
    const auto disallowed = A::compose_direct(p, c);
    ASSERT_FALSE(disallowed);
    EXPECT_EQ(disallowed.error().code, A::Code::Scope);
    c.allowed_themes = 3;
    c.maximum_explicitness = 1;
    c.allow_adult_language = true;
    EXPECT_FALSE(A::compose_direct(p, c)); // Help/proficiency does not advance society membership.
    c.society.echelon = 1;
    const auto admitted = A::compose_direct(p, c);
    ASSERT_TRUE(admitted);
    EXPECT_EQ((*admitted)->shape(), original->shape());
    EXPECT_EQ((*admitted)->traces()[2].proposed_obligation, S::Obligation::Pending);
    p.erase(p.begin() + 2);
    EXPECT_FALSE(A::compose_direct(p, c)); // Cannot repair disallowed scope by dropping the obligation owner.
}

TEST(story_arc, invalid) {
    const auto valid = literal_direct();
    ASSERT_TRUE(A::accept(valid, context()));
    for (int k = 0; k < 9; ++k) {
        auto d = valid;
        auto c = context();
        A::Code code = A::Code::Reference;
        switch (k) {
            case 0: d.edges[0].to = 99; break;
            case 1: d.edges[0] = {0, 0, A::Link::Continue}; code = A::Code::Cycle; break;
            case 2: d.nodes[2].recipient = "unknown"; break;
            case 3: d.nodes[2].material = "gold"; break;
            case 4: d.nodes[1].action = A::Action::CheckHandoff; code = A::Code::Action; break;
            case 5: d.nodes[2].ordinary_due = 1; code = A::Code::Compatibility; break;
            case 6: d.nodes[1].revised_due = 11; code = A::Code::Compatibility; break;
            case 7: d.nodes[3].id = d.nodes[0].id; code = A::Code::Identity; break;
            case 8: c.facts = 0; code = A::Code::Compatibility; break;
        }
        const auto before = d;
        const auto result = A::accept(d, c);
        ASSERT_FALSE(result) << k;
        EXPECT_EQ(result.error().code, code) << k;
        EXPECT_EQ(d, before);
    }
}

TEST(story_arc, ownership) {
    A::Handle retained;
    {
        auto p = pieces();
        auto c = context();
        retained = A::compose_direct(p, c).value();
        p[1].ordinary_due = 10;
        p[0].text = "changed";
        c.actors.clear();
        EXPECT_EQ(retained->draft(), literal_direct());
    }
    EXPECT_EQ(retained->context(), context());
    EXPECT_EQ(retained->traces()[2].remaining_due, 2);
    std::weak_ptr<const A::Artifact> weak = retained;
    retained.reset();
    EXPECT_TRUE(weak.expired());
    static_assert(!std::is_copy_constructible_v<A::Artifact>);
}

TEST(story_arc, limits) {
    const auto original = direct();
    const auto snapshot = original->draft();
    const auto usage = original->usage();
    A::Limits exact;
    exact.nodes = 4;
    exact.edges = 6;
    exact.work = usage.work;
    exact.text_bytes = usage.text_bytes;
    exact.storage_bytes = usage.storage_bytes;
    ASSERT_TRUE(A::accept(snapshot, context(), exact));
    for (int k = 0; k < 7; ++k) {
        auto l = exact;
        A::Code code = A::Code::CountLimit;
        if (k == 0) { --l.nodes; }
        if (k == 1) { --l.edges; }
        if (k == 2) { --l.work; code = A::Code::WorkLimit; }
        if (k == 3) { --l.text_bytes; code = A::Code::TextLimit; }
        if (k == 4) { --l.storage_bytes; code = A::Code::StorageLimit; }
        if (k == 5) { l.traces = 3; }
        if (k == 6) { l.steps = 14; }
        const auto r = A::accept(snapshot, context(), l);
        ASSERT_FALSE(r) << k;
        EXPECT_EQ(r.error().code, code) << k;
        EXPECT_EQ(original->draft(), snapshot);
    }
    for (auto f : {&A::Limits::nodes, &A::Limits::edges, &A::Limits::actors, &A::Limits::traces, &A::Limits::steps,
                   &A::Limits::text_length, &A::Limits::text_bytes, &A::Limits::storage_bytes, &A::Limits::work}) {
        auto l = A::Limits{};
        l.*f = std::numeric_limits<std::size_t>::max();
        auto r = A::accept(snapshot, context(), l);
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code, A::Code::InvalidLimits);
    }
    auto l = A::Limits{};
    l.nodes = 5;
    const auto rewrite = A::introduce_evidence(*original, evidence(), renegotiation(), l);
    ASSERT_FALSE(rewrite);
    EXPECT_EQ(rewrite.error().code, A::Code::CountLimit);
    auto large = pieces();
    large[0].text.assign(1025, 'x');
    const auto text = A::compose_direct(large, context());
    ASSERT_FALSE(text);
    EXPECT_EQ(text.error().code, A::Code::TextLimit);
}

TEST(story_arc, handoff) {
    const auto graph = S::project(S::accept(S::manual_sets()[0], S::ferry_catalogue(), {}).value(), 0).value();
    auto ferry = S::Encounter::start(graph, {"arc-test-ferry"}).value();
    ferry.choose(token(ferry.view().value(), "pay")).value();
    ferry.choose(token(ferry.view().value(), "continue")).value();
    const auto content = S::compose_watch(S::manual_library(), graph, ferry.capture().value(), 0).value();
    auto watch = S::WatchEncounter::start(content, {"arc-test-watch"}).value();
    watch.choose(token(watch.view().value(), "fare-record")).value();
    watch.choose(token(watch.view().value(), "ask-again")).value();
    const auto selected = S::select_bargain(S::selection_library(), S::selection_catalogue(), {content, watch.capture().value()}, 0, 1).value();
    auto bargain = S::BargainEncounter::start(selected->content(), {"arc-test-bargain"}).value();
    bargain.choose(token(bargain.view().value(), "ask-terms")).value();
    bargain.choose(token(bargain.view().value(), "promise")).value();
    bargain.choose(token(bargain.view().value(), "close")).value();
    const auto payoff = S::compose_payoff(selected->content(), bargain.capture().value(),
                                          {S::ReliefLocation::Quay, selected->content()->request().actor, false},
                                          S::manual_payoff_library()).value();
    auto scene = S::PayoffEncounter::start(payoff, {"arc-test-payoff"}).value();
    EXPECT_EQ(scene.state().obligation, S::Obligation::Pending);
    EXPECT_EQ(scene.state().due, 2);
    EXPECT_FALSE(scene.state().favor);
    auto before = scene.capture().value();
    const auto absent = scene.choose(token(scene.view().value(), "deliver"));
    ASSERT_FALSE(absent);
    EXPECT_EQ(absent.error(), D::SessionError::Unavailable);
    EXPECT_EQ(scene.capture().value(), before);
    scene.handoff(true).value();
    ASSERT_TRUE(scene.resources(1));
    before = scene.capture().value();
    const auto shortfall = scene.choose(token(scene.view().value(), "deliver"));
    ASSERT_FALSE(shortfall);
    EXPECT_EQ(shortfall.error(), D::SessionError::Unavailable);
    EXPECT_EQ(scene.capture().value(), before);
    EXPECT_EQ(scene.effects(), 0);
    ASSERT_TRUE(scene.resources(3));
    scene.choose(token(scene.view().value(), "deliver")).value();
    EXPECT_EQ(scene.state().obligation, S::Obligation::Fulfilled);
    EXPECT_EQ(scene.state().delivered, 2);
    EXPECT_EQ(scene.state().world.facts.planks, 1);
    EXPECT_TRUE(scene.state().favor);
    EXPECT_EQ(scene.effects(), 1);
}

TEST(story_arc, caller) {
    std::ostringstream out;
    EXPECT_EQ(A::run(out), 0);
    EXPECT_NE(out.str().find("Accepted realizations=4 structural-shapes=2 mechanical-variants=3"), std::string::npos);
    EXPECT_NE(out.str().find("Conditional previews: direct=4 evidence-first=8"), std::string::npos);
    EXPECT_NE(out.str().find("absent/insufficient handoffs refused; obligation=fulfilled delivered=2 remaining-planks=1"),
              std::string::npos);
}
