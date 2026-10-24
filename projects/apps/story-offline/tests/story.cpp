#include "../src/story.h"
#include <cheryl/core/game-framework/dialogue-inspection.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace {
    namespace S = OfflineStory;
    namespace D = CE::Dialogue;
    S::AcceptedHandle accepted(std::size_t set = 0) {
        return S::accept(S::manual_sets()[set], S::ferry_catalogue(), {}).value();
    }
    S::ProjectionHandle projected(std::size_t set = 0, std::size_t flavour = 0) {
        return S::project(accepted(set), flavour).value();
    }
    S::Encounter owner(std::size_t set = 0, std::size_t flavour = 0) {
        return S::Encounter::start(projected(set, flavour), {"ferry"}).value();
    }
    D::Selection select(S::Encounter& encounter, const char* id) {
        const auto view = encounter.view().value();
        for (const auto& row : view.choices)
            if (row.id.value == id)
                return row.selection;
        throw std::logic_error("Missing literal fixture action");
    }
    void
    refused(const S::CandidateSet& set, const S::Catalogue& cat, const S::Context& context, S::Code code, const S::Limits& limits = {}) {
        const auto result = S::accept(set, cat, context, limits);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, code) << result.error().field;
        EXPECT_FALSE(result.error().field.empty());
    }
}

TEST(story_offline, caller) {
    const auto content = accepted();
    const auto graph = S::project(content, 0).value();
    auto encounter = S::Encounter::start(graph, {"ferry-01"}).value();
    const auto view = encounter.view().value();
    ASSERT_EQ(view.choices.size(), 3);
    EXPECT_EQ(view.text, "The last ferry waits. Mara needs fare or two sound planks.");
    EXPECT_EQ(view.choices[0].id.value, "pay");
    EXPECT_EQ(view.choices[1].id.value, "repair");
    EXPECT_EQ(view.choices[2].id.value, "decline");
    EXPECT_TRUE(view.choices[0].available);
    EXPECT_TRUE(view.choices[1].available);
    EXPECT_TRUE(view.choices[2].available);
    EXPECT_EQ(view.choices[0].text, "Pay three coins");
    const auto chosen = encounter.choose(view.choices[0].selection);
    ASSERT_TRUE(chosen);
    EXPECT_EQ(chosen->target.value, "reply-pay");
    const S::Facts facts{5, 3, true, true, true, false, false};
    EXPECT_EQ(encounter.state().facts, facts);
    EXPECT_EQ(encounter.effects(), 1);
    EXPECT_EQ(encounter.view().value().text, "Mara pockets the fare. Your seat is ready.");
}

TEST(story_offline, routes) {
    struct Expected {
        std::size_t set;
        const char* action;
        S::Facts facts;
        std::array<int, 3> attitude;
        const char* terminal;
    };
    const Expected routes[] = {
        {0, "pay", {5, 3, true, true, true, false, false}, {1, -1, 0}, "destination"},
        {0, "repair", {8, 1, true, true, false, true, false}, {3, -2, 0}, "destination"},
        {0, "decline", {8, 3, false, false, false, false, true}, {0, 0, 0}, "shore"},
        {1, "pay", {3, 3, true, true, true, false, false}, {1, -1, 0}, "destination"},
        {1, "repair", {8, 2, true, true, false, true, false}, {4, -3, 0}, "destination"},
        {1, "decline", {8, 3, false, false, false, false, true}, {0, 0, 0}, "shore"}
    };
    for (const auto& expected : routes) {
        auto encounter = owner(expected.set);
        ASSERT_TRUE(encounter.choose(select(encounter, expected.action)));
        EXPECT_EQ(encounter.state().facts, expected.facts);
        EXPECT_EQ(encounter.state().attitude, expected.attitude);
        const auto before = encounter.state();
        ASSERT_TRUE(encounter.choose(select(encounter, "continue")));
        EXPECT_EQ(encounter.capture().value().dialogue.node.value, expected.terminal);
        EXPECT_EQ(encounter.view().value().kind, D::NodeKind::Terminal);
        EXPECT_EQ(encounter.state(), before);
        EXPECT_EQ(encounter.effects(), 1);
    }
}

TEST(story_offline, flavours) {
    for (std::size_t set = 0; set < 2; ++set) {
        const auto first = projected(set, 0), second = projected(set, 1);
        EXPECT_NE(first->definition()->id(), second->definition()->id());
        EXPECT_EQ(first->definition()->nodes().size(), 6);
        EXPECT_EQ(second->definition()->nodes().size(), 6);
        const auto a = first->definition()->entry().choices, b = second->definition()->entry().choices;
        for (std::size_t i = 0; i < 3; ++i) {
            EXPECT_EQ(a[i].id, b[i].id);
            EXPECT_EQ(a[i].target, b[i].target);
            EXPECT_EQ(a[i].condition, b[i].condition);
            EXPECT_EQ(a[i].effect, b[i].effect);
            EXPECT_NE(a[i].text, b[i].text);
        }
        auto plain = owner(set, 0), river = owner(set, 1);
        ASSERT_TRUE(plain.choose(select(plain, "repair")));
        ASSERT_TRUE(river.choose(select(river, "repair")));
        EXPECT_EQ(plain.state(), river.state());
        EXPECT_NE(plain.view().value().text, river.view().value().text);
    }
}

TEST(story_offline, ownership) {
    static_assert(!std::is_default_constructible_v<S::Accepted>);
    static_assert(!std::is_copy_constructible_v<S::Accepted>);
    static_assert(!std::is_default_constructible_v<S::Projection>);
    S::AcceptedHandle retained;
    {
        auto set = S::manual_sets()[0];
        auto cat = S::ferry_catalogue();
        S::Context context;
        retained = S::accept(set, cat, context).value();
        set.actions[0].flavours[0].wording = "changed";
        set.actions.clear();
        set.proposition.clear();
        cat.outcomes[0].consequence.coins = 99;
        cat.actors[0].name = "changed";
        cat.outcomes.clear();
        context.state.facts.coins = 0;
        context.observers[0] = "changed";
    }
    EXPECT_EQ(retained->set().actions[0].flavours[0].wording, "Pay three coins");
    EXPECT_EQ(retained->catalogue().actors[0].name, "Mara the ferrymaster");
    EXPECT_EQ(retained->context().state.facts.coins, 8);
    auto graph = S::project(retained, 0).value();
    const auto frozen = graph->definition();
    std::weak_ptr<const S::Accepted> weak = retained;
    {
        auto encounter = S::Encounter::start(graph, {"owned"}).value();
        graph.reset();
        retained.reset();
        EXPECT_FALSE(weak.expired());
        ASSERT_TRUE(encounter.choose(select(encounter, "pay")));
        EXPECT_EQ(encounter.state().facts.coins, 5);
    }
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(frozen->entry().choices[0].text, "Pay three coins");
    EXPECT_EQ(frozen->find_node({"reply-pay"})->text, "Mara pockets the fare. Your seat is ready.");
}

TEST(story_offline, pure_preparation) {
    const auto graph = projected();
    auto encounter = S::Encounter::start(graph, {"pure"}).value();
    const auto before = encounter.capture().value();
    const auto report = D::inspect(*graph->definition());
    ASSERT_TRUE(report);
    EXPECT_EQ(report->nodes, 6);
    EXPECT_EQ(report->choices, 6);
    EXPECT_EQ(report->terminals, 2);
    EXPECT_TRUE(report->unreachable.empty());
    static_cast<void>(accepted());
    static_cast<void>(projected(1));
    const auto token = select(encounter, "pay");
    static_cast<void>(encounter.view());
    static_cast<void>(encounter.view());
    EXPECT_EQ(encounter.capture().value(), before);
    EXPECT_EQ(encounter.effects(), 0);
    ASSERT_TRUE(encounter.choose(token));
    EXPECT_EQ(encounter.effects(), 1);
}

TEST(story_offline, prose_not_authority) {
    auto set = S::manual_sets()[0];
    const auto cat = S::ferry_catalogue();
    set.actions[0].flavours[0].response = "The tale promises a crown and one thousand coins.";
    auto graph = S::project(S::accept(set, cat, {}).value(), 0).value();
    auto encounter = S::Encounter::start(graph, {"prose"}).value();
    ASSERT_TRUE(encounter.choose(select(encounter, "pay")));
    EXPECT_EQ(encounter.state().facts.coins, 5);
    EXPECT_EQ(encounter.view().value().text, "The tale promises a crown and one thousand coins.");
    set.actions[0].rewards = {"crown"};
    refused(set, cat, {}, S::Code::UnknownReward);
}

TEST(story_offline, eligibility) {
    auto encounter = owner();
    const auto view = encounter.view().value();
    ASSERT_TRUE(view.choices[0].available);
    ASSERT_TRUE(view.choices[1].available);
    ASSERT_TRUE(encounter.resources(0, 3));
    const auto before = encounter.capture().value();
    const auto rejected = encounter.choose(view.choices[0].selection);
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error(), D::SessionError::Unavailable);
    EXPECT_EQ(encounter.capture().value(), before);
    EXPECT_EQ(encounter.effects(), 0);
    EXPECT_FALSE(encounter.resources(-1, 3));
    EXPECT_FALSE(encounter.resources(101, 3));
    ASSERT_TRUE(encounter.choose(view.choices[1].selection));
    EXPECT_EQ(encounter.state().facts, (S::Facts{0, 1, true, true, false, true, false}));
    const auto stale = encounter.choose(view.choices[1].selection);
    ASSERT_FALSE(stale);
    EXPECT_EQ(stale.error(), D::SessionError::StaleSelection);
    EXPECT_EQ(encounter.effects(), 1);
}

TEST(story_offline, observers) {
    auto encounter = owner();
    ASSERT_TRUE(encounter.choose(select(encounter, "repair")));
    auto state = encounter.state();
    EXPECT_EQ(state.knowledge, (std::array{S::Knowledge::SawRepair, S::Knowledge::HeardPassage, S::Knowledge::None}));
    EXPECT_EQ(state.attitude, (std::array<int, 3>{3, -2, 0}));
    EXPECT_EQ(state.believes_unlicensed, (std::array<bool, 3>{false, true, false}));
    EXPECT_TRUE(state.facts.licensed);
    EXPECT_TRUE(state.facts.repaired);
    auto paired = encounter.capture().value();
    paired.state.knowledge[1] = S::Knowledge::None;
    paired.state.attitude[1] = 0;
    paired.state.believes_unlicensed[1] = false;
    ASSERT_TRUE(encounter.restore(paired));
    EXPECT_EQ(encounter.state().facts, state.facts);
    EXPECT_EQ(encounter.effects(), 1);
    EXPECT_EQ(encounter.state().knowledge[2], S::Knowledge::None);
}

TEST(story_offline, paired_resume) {
    const auto graph = projected();
    auto encounter = S::Encounter::start(graph, {"paired"}).value();
    ASSERT_TRUE(encounter.choose(select(encounter, "pay")));
    const auto saved = encounter.capture().value();
    const auto token = select(encounter, "continue");
    ASSERT_TRUE(encounter.choose(token));
    ASSERT_TRUE(encounter.restore(saved));
    EXPECT_EQ(encounter.capture().value(), saved);
    const auto old = encounter.choose(token);
    ASSERT_FALSE(old);
    EXPECT_EQ(old.error(), D::SessionError::WrongIncarnation);
    ASSERT_TRUE(encounter.choose(select(encounter, "continue")));
    EXPECT_EQ(encounter.state().facts.coins, 5);
    EXPECT_EQ(encounter.effects(), 1);
    auto fresh = S::Encounter::start(graph, {"other"}).value();
    ASSERT_TRUE(fresh.restore(saved));
    ASSERT_TRUE(fresh.choose(select(fresh, "continue")));
    EXPECT_EQ(fresh.state(), saved.state);
    EXPECT_EQ(fresh.effects(), 0);
}

TEST(story_offline, failed_resume) {
    const auto graph = projected();
    auto encounter = S::Encounter::start(graph, {"paired"}).value();
    const auto saved = encounter.capture().value();
    const auto token = select(encounter, "pay");
    for (int damage = 0; damage < 7; ++damage) {
        auto bad = saved;
        switch (damage) {
            case 0:
                bad.state.facts.coins = -1;
                break;
            case 1:
                bad.state.facts.paid = bad.state.facts.repaired = true;
                break;
            case 2:
                ++bad.dialogue.schema;
                break;
            case 3:
                bad.dialogue.definition = {"other"};
                break;
            case 4:
                ++bad.dialogue.definition_revision;
                break;
            case 5:
                bad.dialogue.node = {"absent"};
                break;
            case 6:
                bad.dialogue.session.value.clear();
                break;
        }
        const auto result = encounter.restore(bad);
        ASSERT_FALSE(result);
        EXPECT_EQ(encounter.capture().value(), saved);
        EXPECT_EQ(encounter.effects(), 0);
    }
    ASSERT_TRUE(encounter.choose(token));
    EXPECT_EQ(encounter.effects(), 1);
    auto checkpoint = encounter.capture().value();
    checkpoint.dialogue.step = 73;
    ASSERT_TRUE(encounter.restore(checkpoint));
    EXPECT_EQ(encounter.state().facts.coins, 5);
    EXPECT_EQ(encounter.effects(), 1);
    const auto wrong_variant = S::project(accepted(), 1).value();
    auto other = S::Encounter::start(wrong_variant, {"other"}).value();
    ASSERT_FALSE(other.restore(checkpoint)); // Flavour is part of exact definition identity.
}

TEST(story_offline, unknowns) {
    const auto base = S::manual_sets()[0];
    const auto cat = S::ferry_catalogue();
    auto set = base;
    set.actions[0].actor = "phantom";
    refused(set, cat, {}, S::Code::UnknownActor);
    set = base;
    set.actions[0].operation = "mint_gold";
    refused(set, cat, {}, S::Code::UnknownOperation);
    set = base;
    set.actions[0].condition = "can_teleport";
    refused(set, cat, {}, S::Code::UnknownCondition);
    set = base;
    set.actions[0].outcome = "lost";
    refused(set, cat, {}, S::Code::MissingReference);
    set = base;
    set.actions[0].rewards = {"gold"};
    refused(set, cat, {}, S::Code::UnknownReward);
    auto changed = cat;
    changed.outcomes[0].actor = "phantom";
    refused(base, changed, {}, S::Code::UnknownActor);
    changed = cat;
    changed.operations.push_back("mint_gold");
    refused(base, changed, {}, S::Code::UnknownOperation);
    changed = cat;
    changed.conditions.push_back("can_teleport");
    refused(base, changed, {}, S::Code::UnknownCondition);
    changed = cat;
    changed.outcomes[0].method = static_cast<S::Method>(99);
    refused(base, changed, {}, S::Code::UnknownOperation);
    changed = cat;
    changed.outcomes[0].rewards = {"gold"};
    refused(base, changed, {}, S::Code::UnknownReward);
    S::Context context;
    context.observers[2] = "phantom";
    refused(base, cat, context, S::Code::UnknownActor);
}

TEST(story_offline, duplicates) {
    const auto base = S::manual_sets()[0];
    const auto cat = S::ferry_catalogue();
    auto set = base;
    set.actions[1].id = set.actions[0].id;
    refused(set, cat, {}, S::Code::Duplicate);
    set = base;
    set.actions[0].flavours[1].id = set.actions[0].flavours[0].id;
    refused(set, cat, {}, S::Code::Duplicate);
    set = base;
    set.actions[0].rewards.push_back("passage");
    refused(set, cat, {}, S::Code::Duplicate);
    auto changed = cat;
    changed.actors[1].id = changed.actors[0].id;
    refused(base, changed, {}, S::Code::Duplicate);
    changed = cat;
    changed.outcomes[1].id = changed.outcomes[0].id;
    refused(base, changed, {}, S::Code::Duplicate);
    changed = cat;
    changed.operations.push_back("pay_fare");
    refused(base, changed, {}, S::Code::Duplicate);
    changed = cat;
    changed.conditions.push_back("can_pay");
    refused(base, changed, {}, S::Code::Duplicate);
    changed = cat;
    changed.rewards.push_back("passage");
    refused(base, changed, {}, S::Code::Duplicate);
    EXPECT_TRUE(S::accept(base, cat, {})); // Same flavour/reward names in different actions are valid.
}

TEST(story_offline, conflicts) {
    const auto base = S::manual_sets()[0];
    const auto cat = S::ferry_catalogue();
    auto set = base;
    set.actions[0].claimed.coins = -100;
    refused(set, cat, {}, S::Code::Conflict);
    set = base;
    set.actions[0].claimed.repaired = true;
    refused(set, cat, {}, S::Code::Conflict);
    set = base;
    set.actions[0].actor = "ivo";
    refused(set, cat, {}, S::Code::Conflict);
    set = base;
    set.actions[0].operation = "repair_hull";
    refused(set, cat, {}, S::Code::Conflict);
    set = base;
    set.actions[1] = set.actions[0];
    set.actions[1].id = "alternate";
    refused(set, cat, {}, S::Code::Conflict);
    auto changed = cat;
    changed.outcomes[0].consequence.repaired = true;
    refused(base, changed, {}, S::Code::Conflict);
    changed = cat;
    changed.outcomes[0].consequence.coins = std::numeric_limits<int>::max();
    refused(base, changed, {}, S::Code::Conflict);
    S::Context context;
    context.state.facts.coins = -1;
    refused(base, cat, context, S::Code::Context);
    context = {};
    context.state.facts.coins = context.state.facts.planks = 0;
    refused(base, cat, context, S::Code::Context);
    set = base;
    set.revision = 0;
    refused(set, cat, {}, S::Code::Identity);
    set = base;
    set.origin = static_cast<S::Origin>(99);
    refused(set, cat, {}, S::Code::Identity);
}

TEST(story_offline, owning_diagnostics) {
    S::Error error{S::Code::Conflict, {}, {}};
    {
        auto set = S::manual_sets()[0];
        auto cat = S::ferry_catalogue();
        S::Context context;
        set.actions[1].actor = "phantom";
        auto result = S::accept(set, cat, context);
        ASSERT_FALSE(result);
        error = result.error();
    }
    EXPECT_EQ(error.code, S::Code::UnknownActor);
    EXPECT_EQ(error.field, "action.actor");
    EXPECT_EQ(error.row, 1);
    EXPECT_EQ(error.value, "phantom");
}

TEST(story_offline, count_limits) {
    const auto set = S::manual_sets()[0];
    const auto cat = S::ferry_catalogue();
    S::Limits exact;
    exact.actors = 3;
    exact.outcomes = 5;
    exact.actions = 3;
    exact.flavours = 2;
    exact.names = 3;
    exact.reward_refs = 1;
    ASSERT_TRUE(S::accept(set, cat, {}, exact));
    for (const auto member :
         {&S::Limits::actors, &S::Limits::outcomes, &S::Limits::actions, &S::Limits::flavours, &S::Limits::names,
          &S::Limits::reward_refs}) {
        auto limit = exact;
        --(limit.*member);
        refused(set, cat, {}, S::Code::CountLimit, limit);
        limit.*member = 0;
        refused(set, cat, {}, S::Code::CountLimit, limit);
    }
    auto extra = set;
    extra.actions.push_back(extra.actions[0]);
    refused(extra, cat, {}, S::Code::CountLimit);
    extra = set;
    extra.actions[0].flavours.resize(5);
    refused(extra, cat, {}, S::Code::CountLimit);
}

TEST(story_offline, text_limits) {
    auto set = S::manual_sets()[0];
    const auto cat = S::ferry_catalogue();
    set.actions[0].flavours[0].response = std::string(1024, 'x');
    ASSERT_TRUE(S::accept(set, cat, {}));
    set.actions[0].flavours[0].response.push_back('x');
    refused(set, cat, {}, S::Code::TextLimit);
    set = S::manual_sets()[0];
    set.id = std::string(64, 'x');
    const auto max_id = S::accept(set, cat, {});
    ASSERT_TRUE(max_id);
    ASSERT_TRUE(S::project(*max_id, 0));
    set.id.push_back('x');
    refused(set, cat, {}, S::Code::Identity);
    set = S::manual_sets()[0];
    set.proposition = std::string("bad\0text", 8);
    refused(set, cat, {}, S::Code::TextLimit);
    set = S::manual_sets()[0];
    set.id = "Bad";
    refused(set, cat, {}, S::Code::Identity);
    const auto normal = S::manual_sets()[0];
    const auto usage = accepted()->usage();
    S::Limits exact;
    exact.text_bytes = usage.text_bytes;
    ASSERT_TRUE(S::accept(normal, cat, {}, exact));
    --exact.text_bytes;
    refused(normal, cat, {}, S::Code::TextLimit, exact);
}

TEST(story_offline, storage_work) {
    const auto set = S::manual_sets()[0];
    const auto cat = S::ferry_catalogue();
    const auto old = accepted();
    const auto usage = old->usage();
    S::Limits exact;
    exact.text_bytes = usage.text_bytes;
    exact.storage_bytes = usage.storage_bytes;
    exact.work = usage.work;
    const auto again = S::accept(set, cat, {}, exact);
    ASSERT_TRUE(again);
    EXPECT_EQ((*again)->usage().work, usage.work);
    auto short_limit = exact;
    --short_limit.work;
    refused(set, cat, {}, S::Code::WorkLimit, short_limit);
    short_limit = exact;
    --short_limit.storage_bytes;
    refused(set, cat, {}, S::Code::StorageLimit, short_limit);
    short_limit = exact;
    short_limit.work = 0;
    refused(set, cat, {}, S::Code::WorkLimit, short_limit);
    short_limit = exact;
    short_limit.storage_bytes = 0;
    refused(set, cat, {}, S::Code::StorageLimit, short_limit);
    EXPECT_EQ(old->set().proposition, "The last ferry waits. Mara needs fare or two sound planks.");
    const auto graph = S::project(old, 0).value();
    const auto projected_usage = graph->usage();
    exact = {};
    exact.work = projected_usage.work;
    exact.text_bytes = projected_usage.text_bytes;
    exact.storage_bytes = projected_usage.storage_bytes;
    ASSERT_TRUE(S::project(old, 0, exact));
    for (const auto [field, code] :
         {std::pair{&S::Limits::work, S::Code::WorkLimit}, std::pair{&S::Limits::text_bytes, S::Code::TextLimit},
          std::pair{&S::Limits::storage_bytes, S::Code::StorageLimit}}) {
        short_limit = exact;
        --(short_limit.*field);
        const auto result = S::project(old, 0, short_limit);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, code);
    }
    EXPECT_EQ(graph->definition()->entry().choices[0].text, "Pay three coins");
}

TEST(story_offline, invalid_limits) {
    const auto set = S::manual_sets()[0];
    const auto cat = S::ferry_catalogue();
    const auto old = accepted();
    for (const auto field :
         {&S::Limits::actors, &S::Limits::outcomes, &S::Limits::actions, &S::Limits::flavours, &S::Limits::names, &S::Limits::reward_refs,
          &S::Limits::text_length, &S::Limits::text_bytes, &S::Limits::storage_bytes, &S::Limits::work}) {
        auto limits = S::maximum_limits;
        ++(limits.*field);
        refused(set, cat, {}, S::Code::InvalidLimits, limits);
        limits.*field = std::numeric_limits<std::size_t>::max();
        refused(set, cat, {}, S::Code::InvalidLimits, limits);
        const auto projection = S::project(old, 0, limits);
        ASSERT_FALSE(projection);
        EXPECT_EQ(projection.error().code, S::Code::InvalidLimits);
    }
    const auto variant = S::project(old, 2);
    ASSERT_FALSE(variant);
    EXPECT_EQ(variant.error().code, S::Code::Variant);
    const auto empty = S::project({}, 0);
    ASSERT_FALSE(empty);
    EXPECT_EQ(empty.error().code, S::Code::EmptyHandle);
    const auto encounter = S::Encounter::start({}, {"empty"});
    ASSERT_FALSE(encounter);
    EXPECT_EQ(encounter.error().code, S::Code::EmptyHandle);
}
