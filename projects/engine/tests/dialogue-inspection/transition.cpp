#include <cheryl/core/game-framework/dialogue.h>

#include <gtest/gtest.h>

#include <memory>
#include <type_traits>

#ifdef CE_BUILDING_ENGINE
#error This consumer must import the actual Engine session implementation.
#endif

namespace {
    namespace D = CE::Dialogue;
    D::DefinitionHandle definition() {
        return D::compile(
                   {{"public-transition"},
                    11,
                    {"source"},
                    {{{"end"}, "", D::NodeKind::Terminal, {}},
                     {{"source"}, "", D::NodeKind::Prompt, {{{"go"}, "", {"end"}, {}, D::EffectKey{"apply"}}}}}},
                   {{}, {{"apply"}}}
        ).value();
    }
}

TEST(dialogue_transition_consumer, caller) {
    static_assert(std::is_nothrow_move_constructible_v<D::Transition>);
    static_assert(std::is_nothrow_move_constructible_v<std::expected<D::Transition, D::SessionError>>);
    const auto frozen = definition();
    auto session = D::Session::start(frozen, {"public-session"}).value();
    const auto presented = session.view().value();
    int effects = 0;
    const D::EffectHandler apply = [&](const auto& key) {
        EXPECT_EQ(&key, &*frozen->entry().choices[0].effect);
        ++effects;
    };
    const auto transition = session.choose_with_transition(presented.choices[0].selection, {}, apply);
    ASSERT_TRUE(transition);
    const D::Transition expected{{"public-transition"}, 11, {"public-session"}, {"source"}, {"end"}, {"go"}, 0, 1};
    EXPECT_EQ(*transition, expected);
    EXPECT_EQ(effects, 1);
    EXPECT_EQ(session.snapshot().value().node, transition->target);
}

TEST(dialogue_transition_consumer, retained) {
    D::Transition owned;
    std::weak_ptr<const D::Definition> weak;
    {
        const auto frozen = definition();
        weak = frozen;
        auto session = D::Session::start(frozen, {"public-session"}).value();
        const D::EffectHandler apply = [](const auto&) {};
        owned = session.choose_with_transition(session.view().value().choices[0].selection, {}, apply).value();
    }
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(owned.definition.value, "public-transition");
    EXPECT_EQ(owned.definition_revision, 11);
    EXPECT_EQ(owned.session.value, "public-session");
    EXPECT_EQ(owned.source.value, "source");
    EXPECT_EQ(owned.target.value, "end");
    EXPECT_EQ(owned.choice.value, "go");
}

TEST(dialogue_transition_consumer, authority) {
    const auto frozen = definition();
    auto session = D::Session::start(frozen, {"same"}).value();
    const auto token = session.view().value().choices[0].selection;
    const auto saved = session.snapshot().value();
    auto resumed = D::Session::resume(frozen, saved).value();
    int effects = 0;
    const D::EffectHandler apply = [&](const auto&) { ++effects; };
    const auto wrong = resumed.choose_with_transition(token, {}, apply);
    ASSERT_FALSE(wrong);
    EXPECT_EQ(wrong.error(), D::SessionError::WrongIncarnation);
    EXPECT_EQ(effects, 0);
    EXPECT_EQ(resumed.snapshot().value(), saved);
    const auto fresh = resumed.view().value().choices[0].selection;
    ASSERT_TRUE(resumed.choose_with_transition(fresh, {}, apply));
    EXPECT_EQ(effects, 1);
    const auto stale = resumed.choose_with_transition(fresh, {}, apply);
    ASSERT_FALSE(stale);
    EXPECT_EQ(stale.error(), D::SessionError::StaleSelection);
    EXPECT_EQ(effects, 1);
}
