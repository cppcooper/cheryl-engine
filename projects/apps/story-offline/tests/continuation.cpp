#include "../src/continuation.h"
#include <cheryl/core/game-framework/dialogue-inspection.h>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
    namespace S = OfflineStory;
    namespace D = CE::Dialogue;
    D::Selection select(const D::View& view, std::string_view id) {
        for (const auto& row : view.choices)
            if (row.id.value == id)
                return row.selection;
        throw std::logic_error("Missing literal continuation action");
    }
    struct Source {
        S::ProjectionHandle graph;
        S::Encounter owner;
    };
    Source source(std::string_view action = "pay") {
        const auto graph = S::project(S::accept(S::manual_sets()[0], S::ferry_catalogue(), {}).value(), 0).value();
        auto owner = S::Encounter::start(graph, {"ferry-source"}).value();
        owner.choose(select(owner.view().value(), action)).value();
        owner.choose(select(owner.view().value(), "continue")).value();
        return {graph, std::move(owner)};
    }
    S::ContinuationHandle composed(std::string_view action = "pay", std::size_t flavour = 0) {
        auto src = source(action);
        return S::compose_watch(S::manual_library(), src.graph, src.owner.capture().value(), flavour).value();
    }
    S::WatchEncounter encounter(std::string_view action = "pay", std::size_t flavour = 0) {
        return S::WatchEncounter::start(composed(action, flavour), {"watch"}).value();
    }
}

TEST(story_continuation, caller) {
    auto ferry = source();
    const auto pair = ferry.owner.capture().value();
    const auto content = S::compose_watch(S::manual_library(), ferry.graph, pair, 0).value();
    auto watch = S::WatchEncounter::start(content, {"watch-01"}).value();
    const auto view = watch.view().value();
    ASSERT_EQ(view.choices.size(), 2);
    EXPECT_EQ(view.text, "You step onto the harbor quay.\nIvo: I heard you crossed without a license. Explain?");
    EXPECT_EQ(view.choices[0].id.value, "fare-record");
    EXPECT_EQ(view.choices[0].text, "Show the fare record");
    EXPECT_EQ(view.choices[1].id.value, "withhold");
    EXPECT_TRUE(view.choices[0].available);
    EXPECT_TRUE(view.choices[1].available);
    ASSERT_TRUE(watch.choose(view.choices[0].selection));
    EXPECT_EQ(watch.state().world.facts, (S::Facts{5, 3, true, true, true, false, false}));
    EXPECT_EQ(watch.state().world.knowledge[1], S::Knowledge::VerifiedFare);
    EXPECT_EQ(watch.state().world.attitude, (std::array<int, 3>{1, 1, 0}));
    EXPECT_FALSE(watch.state().world.believes_unlicensed[1]);
    EXPECT_EQ(ferry.owner.capture().value(), pair);
    EXPECT_EQ(ferry.owner.effects(), 1);
    EXPECT_EQ(watch.effects(), 1);
}

TEST(story_continuation, combinations) {
    struct Expected {
        const char* prior;
        const char* action;
        const char* reply;
        S::Knowledge learned;
        S::Facts facts;
    };
    const Expected expected[] = {
        {"pay",
         "fare-record",
         "Ivo checks the fare record. I was wrong: your passage was licensed.",
         S::Knowledge::VerifiedFare,
         {5, 3, true, true, true, false, false}},
        {"repair",
         "mara-testimony",
         "Mara confirms your repair earned licensed passage. Ivo withdraws the suspicion.",
         S::Knowledge::HeardTestimony,
         {8, 1, true, true, false, true, false}},
        {"decline",
         "explain-stay",
         "Ivo understands: you declined, and made no crossing.",
         S::Knowledge::SawDecline,
         {8, 3, false, false, false, false, true}}
    };
    for (const auto& row : expected) {
        const auto content = composed(row.prior);
        auto watch = S::WatchEncounter::start(content, {"watch"}).value();
        const auto initial = watch.state();
        ASSERT_TRUE(watch.choose(select(watch.view().value(), row.action)));
        EXPECT_EQ(watch.view().value().text, row.reply);
        EXPECT_EQ(watch.state().world.facts, row.facts);
        EXPECT_EQ(watch.state().world.knowledge[1], row.learned);
        EXPECT_EQ(watch.state().world.knowledge[2], S::Knowledge::None);
        EXPECT_EQ(watch.state().world.attitude[0], initial.world.attitude[0]);
        EXPECT_EQ(watch.state().world.attitude[2], 0);
        ASSERT_TRUE(watch.choose(select(watch.view().value(), "ask-again")));
        EXPECT_EQ(watch.view().value().text, "Ivo: I remember the explanation. No second proof or payment is needed.");
        EXPECT_EQ(watch.effects(), 1);
        auto withheld = S::WatchEncounter::start(content, {"withheld"}).value();
        ASSERT_TRUE(withheld.choose(select(withheld.view().value(), "withhold")));
        EXPECT_EQ(withheld.state(), initial);
        EXPECT_EQ(withheld.effects(), 0);
        EXPECT_EQ(withheld.view().value().kind, D::NodeKind::Terminal);
    }
}

TEST(story_continuation, shared_flavours) {
    for (const auto prior : {"pay", "repair", "decline"}) {
        const auto a = composed(prior, 0), b = composed(prior, 1);
        EXPECT_NE(a->definition()->id(), b->definition()->id());
        const auto an = a->definition()->nodes(), bn = b->definition()->nodes();
        ASSERT_EQ(an.size(), 5);
        ASSERT_EQ(bn.size(), 5);
        for (std::size_t i = 0; i < an.size(); ++i) {
            EXPECT_EQ(an[i].id, bn[i].id);
            EXPECT_NE(an[i].text, bn[i].text);
            ASSERT_EQ(an[i].choices.size(), bn[i].choices.size());
            for (std::size_t j = 0; j < an[i].choices.size(); ++j) {
                EXPECT_EQ(an[i].choices[j].id, bn[i].choices[j].id);
                EXPECT_EQ(an[i].choices[j].target, bn[i].choices[j].target);
                EXPECT_EQ(an[i].choices[j].condition, bn[i].choices[j].condition);
                EXPECT_EQ(an[i].choices[j].effect, bn[i].choices[j].effect);
            }
        }
        auto plain = S::WatchEncounter::start(a, {"a"}).value(), flavour = S::WatchEncounter::start(b, {"b"}).value();
        plain.choose(plain.view().value().choices[0].selection).value();
        flavour.choose(flavour.view().value().choices[0].selection).value();
        EXPECT_EQ(plain.state(), flavour.state());
        EXPECT_EQ(a->recipe().location, prior == std::string_view("decline") ? "bank" : "arrival");
        EXPECT_EQ(a->library().withhold_reply, "withhold-reply");
    }
}

TEST(story_continuation, remembered_context) {
    for (const auto prior : {"pay", "repair", "decline"}) {
        auto ferry = source(prior);
        auto context = ferry.owner.capture().value();
        const auto old = S::compose_watch(S::manual_library(), ferry.graph, context, 0).value();
        auto watch = S::WatchEncounter::start(old, {"watch"}).value();
        watch.choose(watch.view().value().choices[0].selection).value();
        watch.choose(select(watch.view().value(), "ask-again")).value();
        context.state = watch.state().world;
        const auto remembered = S::compose_watch(S::manual_library(), ferry.graph, context, 0).value();
        EXPECT_NE(old->definition()->id(), remembered->definition()->id());
        EXPECT_EQ(
            remembered->definition()->entry().text,
            prior == std::string_view("decline")
                ? "You remain on the original bank; the ferry has left.\nIvo: I remember you stayed ashore. No crossing to question."
                : "You step onto the harbor quay.\nIvo: I remember your licensed passage. No accusation today."
        );
        EXPECT_NE(old->definition()->entry().text, remembered->definition()->entry().text);
        EXPECT_EQ(watch.effects(), 1);
        EXPECT_EQ(watch.state().world.facts, ferry.owner.state().facts);
    }
}

TEST(story_continuation, ownership) {
    static_assert(!std::is_default_constructible_v<S::Continuation>);
    static_assert(!std::is_copy_constructible_v<S::Continuation>);
    S::ContinuationHandle retained;
    {
        auto ferry = source("repair");
        auto library = S::manual_library();
        auto context = ferry.owner.capture().value();
        retained = S::compose_watch(library, ferry.graph, context, 0).value();
        library.fragments[0].variants[0] = "changed";
        library.outcomes.clear();
        library.recipes.clear();
        context.state.facts.coins = 99;
        context.dialogue.session.value.clear();
    }
    EXPECT_EQ(retained->context().state.facts.coins, 8);
    EXPECT_EQ(retained->library().fragments[0].variants[0], "You step onto the harbor quay.");
    auto watch = S::WatchEncounter::start(retained, {"retained"}).value();
    const auto frozen = retained->definition();
    std::weak_ptr<const S::Continuation> weak = retained;
    retained.reset();
    EXPECT_FALSE(weak.expired());
    auto offered = watch.view().value();
    watch.choose(offered.choices[0].selection).value();
    watch.choose(select(watch.view().value(), "ask-again")).value();
    EXPECT_EQ(offered.choices[0].text, "Ask Mara to vouch for the repair");
    EXPECT_EQ(frozen->find_choice({"question"}, {"mara-testimony"})->target.value, "understood");
    EXPECT_EQ(watch.state().world.facts.planks, 1);
}

TEST(story_continuation, pure_preparation) {
    auto ferry = source();
    const auto before = ferry.owner.capture().value();
    const auto content = S::compose_watch(S::manual_library(), ferry.graph, before, 0).value();
    const auto report = D::inspect(*content->definition()).value();
    EXPECT_EQ(report.nodes, 5);
    EXPECT_EQ(report.choices, 4);
    EXPECT_EQ(report.terminals, 3);
    auto watch = S::WatchEncounter::start(content, {"pure"}).value();
    const auto pair = watch.capture().value();
    const auto token = watch.view().value().choices[0].selection;
    static_cast<void>(watch.view());
    static_cast<void>(watch.view());
    EXPECT_EQ(watch.capture().value(), pair);
    EXPECT_EQ(watch.effects(), 0);
    EXPECT_EQ(ferry.owner.capture().value(), before);
    EXPECT_EQ(ferry.owner.effects(), 1);
    watch.choose(token).value();
    EXPECT_EQ(watch.effects(), 1);
}

TEST(story_continuation, live_authority) {
    auto watch = encounter(), foreign = encounter();
    const auto offered = watch.view().value();
    const auto unchanged = watch.capture().value();
    EXPECT_EQ(watch.choose(foreign.view().value().choices[0].selection).error(), D::SessionError::WrongIncarnation);
    auto other = S::WatchEncounter::start(composed(), {"other-watch"}).value();
    EXPECT_EQ(watch.choose(other.view().value().choices[0].selection).error(), D::SessionError::WrongSession);
    EXPECT_EQ(watch.capture().value(), unchanged);
    ASSERT_TRUE(watch.evidence(false));
    const auto drained = watch.capture().value();
    EXPECT_EQ(watch.choose(offered.choices[0].selection).error(), D::SessionError::Unavailable);
    EXPECT_EQ(watch.capture().value(), drained);
    EXPECT_EQ(watch.effects(), 0);
    EXPECT_FALSE(watch.view().value().choices[0].available);
    watch.choose(offered.choices[1].selection).value();
    EXPECT_EQ(watch.effects(), 0);
    EXPECT_EQ(watch.view().value().kind, D::NodeKind::Terminal);
    EXPECT_EQ(watch.choose(offered.choices[0].selection).error(), D::SessionError::StaleSelection);
    auto active = encounter();
    const auto token = active.view().value().choices[0].selection;
    active.choose(token).value();
    EXPECT_EQ(active.choose(token).error(), D::SessionError::StaleSelection);
    EXPECT_EQ(active.effects(), 1);
}

TEST(story_continuation, paired_resume) {
    const auto content = composed();
    auto watch = S::WatchEncounter::start(content, {"saved"}).value();
    watch.choose(watch.view().value().choices[0].selection).value();
    const auto saved = watch.capture().value();
    const auto token = select(watch.view().value(), "ask-again");
    watch.choose(token).value();
    watch.restore(saved).value();
    EXPECT_EQ(watch.capture().value(), saved);
    EXPECT_EQ(watch.choose(token).error(), D::SessionError::WrongIncarnation);
    watch.choose(select(watch.view().value(), "ask-again")).value();
    EXPECT_EQ(watch.effects(), 1);
    EXPECT_EQ(watch.state().world.facts.coins, 5);
    auto fresh = S::WatchEncounter::start(content, {"fresh"}).value();
    fresh.restore(saved).value();
    fresh.choose(select(fresh.view().value(), "ask-again")).value();
    EXPECT_EQ(fresh.effects(), 0);
    EXPECT_EQ(fresh.state(), saved.state);
}

TEST(story_continuation, restore_refusal) {
    auto watch = encounter();
    const auto original = watch.capture().value();
    const auto token = watch.view().value().choices[0].selection;
    for (int damage = 0; damage < 6; ++damage) {
        auto bad = original;
        switch (damage) {
            case 0:
                ++bad.state.world.facts.coins;
                break;
            case 1:
                bad.state.world.knowledge[2] = S::Knowledge::SawFare;
                break;
            case 2:
                bad.state.world.facts.licensed = false;
                break;
            case 3:
                ++bad.dialogue.schema;
                break;
            case 4:
                ++bad.dialogue.definition_revision;
                break;
            case 5:
                bad.dialogue.node.value = "missing";
                break;
        }
        ASSERT_FALSE(watch.restore(bad));
        EXPECT_EQ(watch.capture().value(), original);
        EXPECT_EQ(watch.effects(), 0);
    }
    watch.choose(token).value();
    auto bad = watch.capture().value();
    bad.state.world = original.state.world; // Edited cursor does not prove knowledge; ask rechecks it.
    watch.restore(bad).value();
    EXPECT_FALSE(watch.view().value().choices[0].available);
    EXPECT_EQ(watch.choose(select(watch.view().value(), "ask-again")).error(), D::SessionError::Unavailable);
    EXPECT_EQ(watch.effects(), 1);
}

TEST(story_continuation, moves_boundaries) {
    auto owner = encounter();
    auto token = owner.view().value().choices[0].selection;
    auto moved = std::move(owner);
    EXPECT_EQ(owner.view().error(), D::SessionError::MovedFrom);
    EXPECT_FALSE(owner.evidence(false));
    EXPECT_FALSE(owner.restore(moved.capture().value()));
    moved.choose(token).value();
    auto end = moved.capture().value();
    end.dialogue.step = std::numeric_limits<std::uint64_t>::max();
    moved.restore(end).value();
    EXPECT_EQ(moved.choose(select(moved.view().value(), "ask-again")).error(), D::SessionError::StepExhausted);
    EXPECT_EQ(moved.effects(), 1);
    EXPECT_FALSE(S::WatchEncounter::start({}, {"empty"}));
}

TEST(story_continuation, context_refusal) {
    auto ferry = source();
    const auto original = ferry.owner.capture().value();
    const auto content = S::compose_watch(S::manual_library(), ferry.graph, original, 0).value();
    auto live = S::WatchEncounter::start(content, {"untouched"}).value();
    const auto pair = live.capture().value();
    const auto token = live.view().value().choices[0].selection;
    for (int damage = 0; damage < 8; ++damage) {
        auto bad = original;
        switch (damage) {
            case 0:
                bad.state.facts.coins = -1;
                break;
            case 1:
                bad.state.facts.declined = true;
                break;
            case 2:
                bad.state.knowledge[1] = S::Knowledge::HeardTestimony;
                break;
            case 3:
                bad.state.knowledge[2] = S::Knowledge::SawFare;
                break;
            case 4:
                bad.dialogue.node.value = "reply-pay";
                break;
            case 5:
                bad.dialogue.node.value = "shore";
                break;
            case 6:
                ++bad.dialogue.definition_revision;
                break;
            case 7:
                bad.dialogue.session.value.clear();
                break;
        }
        EXPECT_FALSE(S::compose_watch(S::manual_library(), ferry.graph, bad, 0));
        EXPECT_EQ(ferry.owner.capture().value(), original);
        EXPECT_EQ(live.capture().value(), pair);
        EXPECT_EQ(content->definition()->entry().choices[0].text, "Show the fare record");
    }
    live.choose(token).value();
    EXPECT_EQ(live.effects(), 1);
    EXPECT_FALSE(S::compose_watch(S::manual_library(), {}, original, 0));
    EXPECT_FALSE(S::compose_watch(S::manual_library(), ferry.graph, original, 2));
}

TEST(story_continuation, ingredient_refusal) {
    auto ferry = source();
    const auto context = ferry.owner.capture().value();
    const auto base = S::manual_library();
    const auto refuse = [&](S::FragmentLibrary l, S::Code code) {
        const auto result = S::compose_watch(l, ferry.graph, context, 0);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, code) << result.error().field;
    };
    auto l = base;
    l.recipes[0].actor = "phantom";
    refuse(l, S::Code::UnknownActor);
    l = base;
    l.outcomes[0].operation = "mint_gold";
    refuse(l, S::Code::UnknownOperation);
    l = base;
    l.outcomes[0].condition = "magic_permission";
    refuse(l, S::Code::UnknownCondition);
    l = base;
    l.recipes[0].reply = "missing";
    refuse(l, S::Code::MissingReference);
    l = base;
    l.recipes[0].outcome = "missing";
    refuse(l, S::Code::MissingReference);
    l = base;
    l.outcomes[0].learned = S::Knowledge::None;
    refuse(l, S::Code::Conflict);
    l = base;
    l.outcomes[0].attitude = 99;
    refuse(l, S::Code::Conflict);
    l = base;
    l.recipes[0].outcome = "repair-confirmed";
    refuse(l, S::Code::Conflict);
    l = base;
    l.recipes[0].reply = "record-word";
    refuse(l, S::Code::Conflict);
    l = base;
    l.fragments[0].kind = static_cast<S::FragmentKind>(99);
    refuse(l, S::Code::Conflict);
    l = base;
    l.fragments[1].id = l.fragments[0].id;
    refuse(l, S::Code::Duplicate);
    l = base;
    l.outcomes[1].id = l.outcomes[0].id;
    refuse(l, S::Code::Duplicate);
    l = base;
    l.recipes[1].id = l.recipes[0].id;
    refuse(l, S::Code::Duplicate);
    l = base;
    l.recipes[0].action = "withhold";
    refuse(l, S::Code::Duplicate);
    l = base;
    l.recipes.erase(l.recipes.begin());
    refuse(l, S::Code::MissingReference);
    l = base;
    l.revision = 0;
    refuse(l, S::Code::Identity);
    S::Error owned{S::Code::Conflict, {}, {}};
    {
        auto broken = base;
        broken.recipes[0].reply = "lost-fragment";
        owned = S::compose_watch(broken, ferry.graph, context, 0).error();
    }
    EXPECT_EQ(owned.field, "fragment.reference");
    EXPECT_EQ(owned.value, "lost-fragment");
    EXPECT_EQ(ferry.owner.capture().value(), context);
}

TEST(story_continuation, count_limits) {
    auto src = source();
    const auto context = src.owner.capture().value();
    const auto library = S::manual_library();
    S::WatchLimits exact;
    exact.fragments = 18;
    exact.outcomes = 3;
    exact.recipes = 3;
    exact.candidates = 3;
    exact.nodes = 5;
    exact.choices = 4;
    ASSERT_EQ(library.fragments.size(), 18);
    ASSERT_TRUE(S::compose_watch(library, src.graph, context, 0, exact));
    for (const auto field :
         {&S::WatchLimits::fragments, &S::WatchLimits::outcomes, &S::WatchLimits::recipes, &S::WatchLimits::candidates,
          &S::WatchLimits::nodes, &S::WatchLimits::choices}) {
        auto limit = exact;
        --(limit.*field);
        const auto r = S::compose_watch(library, src.graph, context, 0, limit);
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code, S::Code::CountLimit);
        limit.*field = 0;
        EXPECT_FALSE(S::compose_watch(library, src.graph, context, 0, limit));
    }
}

TEST(story_continuation, payload_limits) {
    auto src = source();
    const auto context = src.owner.capture().value();
    auto library = S::manual_library();
    const auto old = S::compose_watch(library, src.graph, context, 0).value();
    const auto used = old->usage();
    S::WatchLimits exact;
    exact.text_bytes = used.text_bytes;
    exact.storage_bytes = used.storage_bytes;
    exact.work = used.work;
    ASSERT_TRUE(S::compose_watch(library, src.graph, context, 0, exact));
    for (const auto [field, code] :
         {std::pair{&S::WatchLimits::text_bytes, S::Code::TextLimit}, std::pair{&S::WatchLimits::storage_bytes, S::Code::StorageLimit},
          std::pair{&S::WatchLimits::work, S::Code::WorkLimit}}) {
        auto lower = exact;
        --(lower.*field);
        const auto r = S::compose_watch(library, src.graph, context, 0, lower);
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code, code);
        lower.*field = 0;
        EXPECT_FALSE(S::compose_watch(library, src.graph, context, 0, lower));
    }
    library.fragments[9].variants[0] = std::string(1024, 'x');
    ASSERT_TRUE(S::compose_watch(library, src.graph, context, 0));
    library.fragments[9].variants[0].push_back('x');
    EXPECT_EQ(S::compose_watch(library, src.graph, context, 0).error().code, S::Code::TextLimit);
    library = S::manual_library();
    library.fragments[0].variants[0] = std::string("x\0y", 3);
    EXPECT_EQ(S::compose_watch(library, src.graph, context, 0).error().code, S::Code::TextLimit);
    library = S::manual_library();
    library.id = std::string(64, 'x');
    ASSERT_TRUE(S::compose_watch(library, src.graph, context, 0));
    library.id.push_back('x');
    EXPECT_FALSE(S::compose_watch(library, src.graph, context, 0));
    EXPECT_EQ(old->definition()->entry().choices[0].text, "Show the fare record");
}

TEST(story_continuation, invalid_limits) {
    auto src = source();
    const auto context = src.owner.capture().value();
    const auto library = S::manual_library();
    for (const auto field :
         {&S::WatchLimits::fragments, &S::WatchLimits::outcomes, &S::WatchLimits::recipes, &S::WatchLimits::candidates,
          &S::WatchLimits::nodes, &S::WatchLimits::choices, &S::WatchLimits::text_length, &S::WatchLimits::text_bytes,
          &S::WatchLimits::storage_bytes, &S::WatchLimits::work}) {
        auto limits = S::maximum_watch_limits;
        ++(limits.*field);
        EXPECT_EQ(S::compose_watch(library, src.graph, context, 0, limits).error().code, S::Code::InvalidLimits);
        limits.*field = std::numeric_limits<std::size_t>::max();
        EXPECT_EQ(S::compose_watch(library, src.graph, context, 0, limits).error().code, S::Code::InvalidLimits);
    }
}
