#include <cheryl/core/game-framework/dialogue.h>

#include <gtest/gtest.h>

#include <exception>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
    namespace D = CE::Dialogue;

    D::Draft gate() {
        return {
            {"gate"},
            7,
            {"approach"},
            {{{"approach"},
              "Guard",
              D::NodeKind::Prompt,
              {{{"introduce"}, "Introduce", {"gate"}, {}, D::EffectKey{"introduce"}}, {{"silent"}, "Silence", {"gate"}, {}, {}}}},
             {{"gate"},
              "Gate",
              D::NodeKind::Prompt,
              {{{"pass"}, "Pass", {"admitted"}, D::ConditionKey{"introduced"}, {}}, {{"leave"}, "Leave", {"departed"}, {}, {}}}},
             {{"admitted"}, "Enter", D::NodeKind::Terminal, {}},
             {{"departed"}, "Depart", D::NodeKind::Terminal, {}}}
        };
    }
    D::DefinitionHandle freeze(const D::Draft& draft = gate()) {
        return D::compile(draft, {{{"introduced"}}, {{"introduce"}}}).value();
    }
    D::Session start(D::DefinitionHandle definition = freeze()) {
        return D::Session::start(std::move(definition), {"encounter"}).value();
    }
    const D::ChoiceView& choice(const D::View& view, const std::string& id) {
        for (const auto& row : view.choices)
            if (row.id.value == id)
                return row;
        throw std::logic_error("Missing transition fixture choice");
    }
    struct Flags {
        bool introduced = false;
        int effects = 0;
        D::ConditionHandler test() {
            return [this](const auto&) { return introduced; };
        }
        D::EffectHandler apply() {
            return [this](const auto&) {
                introduced = true;
                ++effects;
            };
        }
    };
    void refused(
        D::Session& session,
        const D::Selection& selection,
        D::SessionError expected,
        const D::ConditionHandler& test = {},
        const D::EffectHandler& apply = {}
    ) {
        const auto result = session.choose_with_transition(selection, test, apply);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), expected);
    }
}

TEST(dialogue_transition, caller) {
    auto session = start();
    Flags flags;
    const auto offered = session.view().value();
    const auto transition = session.choose_with_transition(choice(offered, "introduce").selection, flags.test(), flags.apply());
    ASSERT_TRUE(transition);
    const D::Transition expected{{"gate"}, 7, {"encounter"}, {"approach"}, {"gate"}, {"introduce"}, 0, 1};
    EXPECT_EQ(*transition, expected);
    EXPECT_TRUE(flags.introduced);
    EXPECT_EQ(flags.effects, 1);
    const auto gate_view = session.view(flags.test()).value();
    const auto passed = session.choose_with_transition(choice(gate_view, "pass").selection, flags.test(), flags.apply());
    ASSERT_TRUE(passed);
    EXPECT_EQ(passed->source.value, "gate");
    EXPECT_EQ(passed->target.value, "admitted");
    EXPECT_EQ(passed->choice.value, "pass");
    EXPECT_EQ(passed->old_step, 1);
    EXPECT_EQ(passed->new_step, 2);
    EXPECT_EQ(session.snapshot().value().node.value, "admitted");
    EXPECT_EQ(flags.effects, 1);
}

TEST(dialogue_transition, silent) {
    auto session = start();
    Flags flags;
    const auto initial = session.view().value();
    ASSERT_TRUE(session.choose_with_transition(choice(initial, "silent").selection));
    const auto offered = session.view(flags.test()).value();
    EXPECT_FALSE(choice(offered, "pass").available);
    const auto before = session.snapshot().value();
    refused(session, choice(offered, "pass").selection, D::SessionError::Unavailable, flags.test(), flags.apply());
    EXPECT_EQ(session.snapshot().value(), before);
    const auto left = session.choose_with_transition(choice(offered, "leave").selection);
    ASSERT_TRUE(left);
    EXPECT_EQ(left->target.value, "departed");
    EXPECT_EQ(left->old_step, 1);
    EXPECT_EQ(left->new_step, 2);
    EXPECT_FALSE(flags.introduced);
    EXPECT_EQ(flags.effects, 0);
}

TEST(dialogue_transition, rejections) {
    auto session = start();
    Flags flags;
    const auto offered = session.view().value();
    const auto token = choice(offered, "introduce").selection;
    const auto before = session.snapshot().value();
    auto forged = token;
    forged.session = {"wrong"};
    refused(session, forged, D::SessionError::WrongSession, flags.test(), flags.apply());
    D::Selection no_marker;
    no_marker.session = token.session;
    no_marker.node = token.node;
    no_marker.step = token.step;
    no_marker.choice = token.choice;
    refused(session, no_marker, D::SessionError::WrongIncarnation, flags.test(), flags.apply());
    forged = token;
    ++forged.step;
    refused(session, forged, D::SessionError::StaleSelection, flags.test(), flags.apply());
    forged = token;
    forged.node = {"gate"};
    refused(session, forged, D::SessionError::StaleSelection, flags.test(), flags.apply());
    forged = token;
    forged.choice = {"unknown"};
    refused(session, forged, D::SessionError::UnknownChoice, flags.test(), flags.apply());
    refused(session, token, D::SessionError::MissingEffectHandler);
    EXPECT_EQ(session.snapshot().value(), before);
    EXPECT_EQ(flags.effects, 0);
    ASSERT_TRUE(session.choose(choice(offered, "silent").selection)); // Legacy and observing calls interleave.
    flags.introduced = true;
    const auto available = session.view(flags.test()).value();
    ASSERT_TRUE(choice(available, "pass").available);
    flags.introduced = false;
    refused(session, choice(available, "pass").selection, D::SessionError::Unavailable, flags.test(), flags.apply());
    refused(session, choice(available, "pass").selection, D::SessionError::MissingConditionHandler);
    refused(session, token, D::SessionError::StaleSelection, flags.test(), flags.apply());
    EXPECT_EQ(flags.effects, 0);
    EXPECT_EQ(session.snapshot().value().step, 1);
}

TEST(dialogue_transition, handler_order) {
    auto draft = gate();
    draft.nodes[0].choices[0].condition = D::ConditionKey{"introduced"};
    auto session = start(freeze(draft));
    int guards = 0;
    const D::ConditionHandler test = [&](const auto&) {
        ++guards;
        return true;
    };
    const auto token = session.view(test).value().choices[0].selection;
    const auto before = session.snapshot().value();
    refused(session, token, D::SessionError::MissingConditionHandler);
    refused(session, token, D::SessionError::MissingEffectHandler, test);
    EXPECT_EQ(guards, 1); // Missing-effect validation precedes condition execution.
    const D::EffectHandler apply = [](const auto&) {};
    refused(session, token, D::SessionError::MissingConditionHandler, {}, apply);
    EXPECT_EQ(session.snapshot().value(), before);
}

TEST(dialogue_transition, guards) {
    auto draft = gate();
    draft.nodes[0].choices[0].condition = D::ConditionKey{"introduced"};
    auto session = start(freeze(draft));
    const D::ConditionHandler enabled = [](const auto&) { return true; };
    const auto token = choice(session.view(enabled).value(), "introduce").selection;
    const auto before = session.snapshot().value();
    struct Failure {};
    const auto failure = std::make_exception_ptr(Failure{});
    int effects = 0;
    const D::ConditionHandler test = [&](const auto&) -> bool { std::rethrow_exception(failure); };
    const D::EffectHandler apply = [&](const auto&) { ++effects; };
    try {
        static_cast<void>(session.choose_with_transition(token, test, apply));
        FAIL() << "Predicate must rethrow its original failure";
    } catch (const Failure&) {
        EXPECT_EQ(std::current_exception(), failure);
    }
    EXPECT_EQ(effects, 0);
    EXPECT_EQ(session.snapshot().value(), before);
    ASSERT_TRUE(session.choose_with_transition(token, enabled, apply)); // Healthy; busy guard cleared.
    EXPECT_EQ(effects, 1);
}

TEST(dialogue_transition, effects) {
    auto session = start();
    const auto token = choice(session.view().value(), "introduce").selection;
    const auto before = session.snapshot().value();
    struct Failure {};
    const auto failure = std::make_exception_ptr(Failure{});
    bool partial = false;
    int effects = 0;
    const D::EffectHandler apply = [&](const auto&) {
        partial = true;
        ++effects;
        std::rethrow_exception(failure);
    };
    try {
        static_cast<void>(session.choose_with_transition(token, {}, apply));
        FAIL() << "Effect must rethrow its original failure";
    } catch (const Failure&) {
        EXPECT_EQ(std::current_exception(), failure);
    }
    EXPECT_TRUE(partial);
    EXPECT_EQ(effects, 1);
    EXPECT_EQ(before.node.value, "approach");
    refused(session, token, D::SessionError::Faulted, {}, apply);
    const auto legacy = session.choose(token, {}, apply);
    ASSERT_FALSE(legacy);
    EXPECT_EQ(legacy.error(), D::SessionError::Faulted);
    const auto view = session.view();
    ASSERT_FALSE(view);
    EXPECT_EQ(view.error(), D::SessionError::Faulted);
    const auto saved = session.snapshot();
    ASSERT_FALSE(saved);
    EXPECT_EQ(saved.error(), D::SessionError::Faulted);
    EXPECT_EQ(effects, 1);
}

TEST(dialogue_transition, reentrant) {
    auto draft = gate();
    draft.nodes[0].choices[0].condition = D::ConditionKey{"introduced"};
    const auto definition = freeze(draft);
    auto session = start(definition);
    const auto token = session.view([](const auto&) { return true; }).value().choices[0].selection;
    int callbacks = 0;
    const auto busy = [&] {
        ++callbacks;
        const auto view = session.view();
        ASSERT_FALSE(view);
        EXPECT_EQ(view.error(), D::SessionError::Busy);
        const auto saved = session.snapshot();
        ASSERT_FALSE(saved);
        EXPECT_EQ(saved.error(), D::SessionError::Busy);
        const auto legacy = session.choose(token);
        ASSERT_FALSE(legacy);
        EXPECT_EQ(legacy.error(), D::SessionError::Busy);
        refused(session, token, D::SessionError::Busy);
    };
    const D::ConditionHandler test = [&](const auto& key) {
        EXPECT_EQ(&key, &*definition->entry().choices[0].condition);
        busy();
        return true;
    };
    const D::EffectHandler apply = [&](const auto& key) {
        EXPECT_EQ(&key, &*definition->entry().choices[0].effect);
        busy();
    };
    ASSERT_TRUE(session.choose_with_transition(token, test, apply));
    EXPECT_EQ(callbacks, 2);
    EXPECT_EQ(session.snapshot().value().step, 1);
}

TEST(dialogue_transition, moves) {
    auto source = start();
    const auto token = choice(source.view().value(), "silent").selection;
    auto moved = std::move(source);
    refused(source, token, D::SessionError::MovedFrom);
    auto destination = start();
    const auto replaced = choice(destination.view().value(), "silent").selection;
    destination = std::move(moved);
    refused(moved, token, D::SessionError::MovedFrom);
    refused(destination, replaced, D::SessionError::WrongIncarnation);
    const auto transition = destination.choose_with_transition(token);
    ASSERT_TRUE(transition);
    EXPECT_EQ(transition->source.value, "approach");
    EXPECT_EQ(transition->target.value, "gate");
    EXPECT_EQ(transition->session.value, "encounter");
}

TEST(dialogue_transition, resume) {
    const auto definition = freeze();
    auto session = start(definition);
    Flags flags;
    const auto first = choice(session.view().value(), "introduce").selection;
    ASSERT_TRUE(session.choose_with_transition(first, flags.test(), flags.apply()));
    const auto token = choice(session.view(flags.test()).value(), "pass").selection;
    const auto paired_flags = flags.introduced;
    const auto saved = session.snapshot().value();
    auto malformed = saved;
    malformed.node = {"absent"};
    const auto invalid = D::Session::resume(definition, malformed);
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error(), D::SessionError::UnknownNode);
    EXPECT_EQ(session.snapshot().value(), saved);
    auto resumed = D::Session::resume(definition, saved).value();
    flags.introduced = paired_flags;
    EXPECT_EQ(resumed.snapshot().value(), saved);
    refused(resumed, token, D::SessionError::WrongIncarnation, flags.test(), flags.apply());
    const auto fresh = choice(resumed.view(flags.test()).value(), "pass").selection;
    const auto transition = resumed.choose_with_transition(fresh, flags.test(), flags.apply());
    ASSERT_TRUE(transition);
    EXPECT_EQ(transition->old_step, 1);
    EXPECT_EQ(transition->new_step, 2);
    EXPECT_EQ(transition->target.value, "admitted");
    EXPECT_EQ(flags.effects, 1);
    // Failed restore did not revoke the original owner's still-live token.
    ASSERT_TRUE(session.choose_with_transition(token, flags.test(), flags.apply()));
    EXPECT_EQ(flags.effects, 1);
}

TEST(dialogue_transition, cycles) {
    const auto definition = freeze({{"loop"}, 1, {"a"}, {{{"a"}, "", D::NodeKind::Prompt, {{{"again"}, "", {"a"}, {}, {}}}}}});
    auto session = start(definition);
    const auto old = session.view().value().choices[0].selection;
    const auto first = session.choose_with_transition(old).value();
    EXPECT_EQ(first.source, first.target);
    EXPECT_EQ(first.old_step, 0);
    EXPECT_EQ(first.new_step, 1);
    refused(session, old, D::SessionError::StaleSelection);
    D::Selection observation_only;
    observation_only.session = first.session;
    observation_only.node = first.target;
    observation_only.choice = first.choice;
    observation_only.step = first.new_step;
    refused(session, observation_only, D::SessionError::WrongIncarnation);
    const auto second = session.choose_with_transition(session.view().value().choices[0].selection).value();
    EXPECT_EQ(second.old_step, 1);
    EXPECT_EQ(second.new_step, 2);
}

TEST(dialogue_transition, boundaries) {
    const auto definition = freeze();
    auto session = start(definition);
    auto saved = session.snapshot().value();
    saved.step = std::numeric_limits<std::uint64_t>::max() - 1;
    saved.node = {"gate"};
    session = D::Session::resume(definition, saved).value();
    const auto token = choice(session.view([](const auto&) { return true; }).value(), "leave").selection;
    const auto transition = session.choose_with_transition(token).value();
    EXPECT_EQ(transition.old_step, std::numeric_limits<std::uint64_t>::max() - 1);
    EXPECT_EQ(transition.new_step, std::numeric_limits<std::uint64_t>::max());
    EXPECT_EQ(session.view().value().kind, D::NodeKind::Terminal);
    EXPECT_EQ(session.snapshot().value().step, transition.new_step);
    auto terminal = token; // Retains the right marker; terminal errors precede membership/overflow.
    terminal.node = transition.target;
    terminal.step = transition.new_step;
    refused(session, terminal, D::SessionError::Finished);
    saved.node = {"approach"};
    saved.step = std::numeric_limits<std::uint64_t>::max();
    session = D::Session::resume(definition, saved).value();
    const auto exhausted = choice(session.view().value(), "introduce").selection;
    int effects = 0;
    const D::EffectHandler apply = [&](const auto&) { ++effects; };
    refused(session, exhausted, D::SessionError::StepExhausted, {}, apply);
    EXPECT_EQ(session.snapshot().value(), saved);
    EXPECT_EQ(effects, 0);
}

TEST(dialogue_transition, retained) {
    D::Transition owned;
    std::weak_ptr<const D::Definition> weak;
    const std::string prefix(256, 'x');
    const auto from = prefix + std::string("\0from", 5), to = prefix + std::string("\0\xff", 2);
    const auto name = prefix + std::string("\0choice", 7);
    {
        D::Draft draft{
            {prefix + "definition"},
            23,
            {from},
            {{{from}, "", D::NodeKind::Prompt, {{{name}, "", {to}, {}, {}}}}, {{to}, "", D::NodeKind::Terminal, {}}}
        };
        const auto definition = freeze(draft);
        weak = definition;
        auto session = D::Session::start(definition, {prefix + "session"}).value();
        owned = session.choose_with_transition(session.view().value().choices[0].selection).value();
        draft.nodes.clear();
        draft.id.value.clear();
    }
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(owned.definition.value, prefix + "definition");
    EXPECT_EQ(owned.definition_revision, 23);
    EXPECT_EQ(owned.session.value, prefix + "session");
    EXPECT_EQ(owned.source.value, from);
    EXPECT_EQ(owned.target.value, to);
    EXPECT_EQ(owned.choice.value, name);
    EXPECT_EQ(owned.old_step, 0);
    EXPECT_EQ(owned.new_step, 1);
    auto independent = owned;
    independent.source.value.clear();
    EXPECT_EQ(owned.source.value, from);
}
