#include <cheryl/core/game-framework/dialogue.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <exception>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {
    namespace D = CE::Dialogue;

    // The same four-node authored gate as the definition suite; no console/runtime adapter.
    D::Draft gate() {
        return {
            .id = {"gate-encounter"},
            .revision = 1,
            .entry = {"approach"},
            .nodes = {
                {{"approach"},
                 "A guard waits.",
                 D::NodeKind::Prompt,
                 {{{"introduce"}, "Introduce yourself", {"gate"}, {}, D::EffectKey{"introduce"}},
                  {{"silent"}, "Stay silent", {"gate"}, {}, {}}}},
                {{"gate"},
                 "The gate is closed.",
                 D::NodeKind::Prompt,
                 {{{"pass"}, "Ask to enter", {"admitted"}, D::ConditionKey{"introduced"}, {}}, {{"leave"}, "Leave", {"departed"}, {}, {}}}},
                {{"admitted"}, "You enter", D::NodeKind::Terminal, {}},
                {{"departed"}, "You depart", D::NodeKind::Terminal, {}}
            }
        };
    }

    D::DefinitionHandle definition(D::Draft draft = gate()) {
        return D::compile(draft, {{{"introduced"}}, {{"introduce"}}}).value();
    }

    D::Session start(D::DefinitionHandle content = definition(), D::SessionId id = {"gate-0001"}) {
        return D::Session::start(std::move(content), std::move(id)).value();
    }

    const D::ChoiceView& choice(const D::View& view, const char* id) {
        const auto found = std::find_if(view.choices.begin(), view.choices.end(), [&](const auto& row) { return row.id.value == id; });
        if (found == view.choices.end())
            throw std::logic_error("Missing fixture choice");
        return *found;
    }

    struct Flags {
        bool introduced = false;
    };

    D::ConditionHandler predicate(const Flags& flags) {
        return [&flags](const D::ConditionKey& key) {
            if (key.value != "introduced")
                throw std::logic_error("Unknown fixture condition");
            return flags.introduced;
        };
    }

    D::EffectHandler effect(Flags& flags, int& calls) {
        return [&flags, &calls](const D::EffectKey& key) {
            if (key.value != "introduce")
                throw std::logic_error("Unknown fixture effect");
            ++calls;
            flags.introduced = true;
        };
    }
}

TEST(dialogue_session, consumer) {
    static_assert(!std::is_copy_constructible_v<D::Session>);
    static_assert(!std::is_copy_assignable_v<D::Session>);
    static_assert(std::is_nothrow_move_constructible_v<D::Session>);
    static_assert(std::is_nothrow_move_assignable_v<D::Session>);
    static_assert(!std::is_constructible_v<D::SessionId, D::DefinitionId>);
    static_assert(std::is_same_v<decltype(std::declval<D::Session&>().choose({})), std::expected<void, D::SessionError>>);

    const auto content = definition();
    Flags flags;
    int calls = 0; // Test instrumentation, separate from the one gameplay flag.
    auto session = start(content);
    const auto offer = session.view(predicate(flags));
    ASSERT_TRUE(offer.has_value());
    ASSERT_TRUE(session.choose(choice(*offer, "introduce").selection, predicate(flags), effect(flags, calls)));

    // Capture/adopt both values on this serialized owner, between operations.
    const Flags saved_flags = flags;
    const auto saved = session.snapshot();
    ASSERT_TRUE(saved.has_value());
    EXPECT_EQ(saved->node.value, "gate");
    EXPECT_EQ(saved->step, 1u);
    EXPECT_TRUE(saved_flags.introduced);
    auto loaded = D::Session::resume(content, *saved);
    ASSERT_TRUE(loaded.has_value());
    flags = {};
    session = std::move(*loaded); // Idle session/boolean adoption cannot throw.
    flags = saved_flags;
    const auto resumed = session.view(predicate(flags));
    ASSERT_TRUE(resumed.has_value());
    EXPECT_TRUE(choice(*resumed, "pass").available);
    ASSERT_TRUE(session.choose(choice(*resumed, "pass").selection, predicate(flags), effect(flags, calls)));
    EXPECT_EQ(session.snapshot()->node.value, "admitted");
    EXPECT_EQ(calls, 1);
}

TEST(dialogue_session, routes) {
    for (const bool introduce : {false, true}) {
        Flags flags;
        int calls = 0;
        auto session = start();
        const auto initial = session.view();
        ASSERT_TRUE(initial);
        EXPECT_EQ(initial->choices[0].id.value, "introduce");
        EXPECT_EQ(initial->choices[1].id.value, "silent");
        ASSERT_TRUE(session.choose(choice(*initial, introduce ? "introduce" : "silent").selection, predicate(flags), effect(flags, calls)));
        const auto gate_view = session.view(predicate(flags));
        ASSERT_TRUE(gate_view);
        EXPECT_EQ(choice(*gate_view, "pass").available, introduce);
        if (!introduce) {
            const auto before = session.snapshot();
            EXPECT_EQ(
                session.choose(choice(*gate_view, "pass").selection, predicate(flags), effect(flags, calls)).error(),
                D::SessionError::Unavailable
            );
            EXPECT_EQ(session.snapshot(), before);
        }
        ASSERT_TRUE(session.choose(choice(*gate_view, introduce ? "pass" : "leave").selection, predicate(flags), effect(flags, calls)));
        const auto ending = session.view();
        ASSERT_TRUE(ending);
        EXPECT_EQ(ending->node.value, introduce ? "admitted" : "departed");
        EXPECT_EQ(ending->kind, D::NodeKind::Terminal);
        EXPECT_TRUE(ending->choices.empty());
        EXPECT_EQ(calls, introduce ? 1 : 0);
        auto token = choice(*gate_view, "leave").selection;
        token.node = ending->node;
        token.step = ending->step;
        EXPECT_EQ(session.choose(token, predicate(flags), effect(flags, calls)).error(), D::SessionError::Finished);
    }
}

TEST(dialogue_session, conditions) {
    auto draft = gate();
    draft.entry = {"gate"};
    draft.nodes[1].choices[0].effect = D::EffectKey{"introduce"};
    auto session = start(definition(draft));
    Flags flags{true};
    int calls = 0;
    const auto offered = session.view(predicate(flags));
    ASSERT_TRUE(offered);
    ASSERT_TRUE(choice(*offered, "pass").available);
    flags.introduced = false;
    const auto before = session.snapshot();
    EXPECT_EQ(
        session.choose(choice(*offered, "pass").selection, predicate(flags), effect(flags, calls)).error(), D::SessionError::Unavailable
    );
    EXPECT_EQ(session.snapshot(), before);
    EXPECT_EQ(calls, 0);
    const auto disabled = session.view(predicate(flags));
    ASSERT_TRUE(disabled);
    ASSERT_FALSE(choice(*disabled, "pass").available);
    flags.introduced = true;
    ASSERT_TRUE(session.choose(choice(*disabled, "pass").selection, predicate(flags), effect(flags, calls)));
    EXPECT_EQ(calls, 1);
}

TEST(dialogue_session, scope) {
    auto session = start();
    Flags flags;
    int calls = 0;
    const auto offered = session.view();
    ASSERT_TRUE(offered);
    const auto token = choice(*offered, "introduce").selection;
    const auto before = session.snapshot();
    auto wrong = token;
    wrong.session = {"other-session"};
    EXPECT_EQ(session.choose(wrong, predicate(flags), effect(flags, calls)).error(), D::SessionError::WrongSession);
    wrong = token;
    wrong.node = {"gate"};
    EXPECT_EQ(session.choose(wrong, predicate(flags), effect(flags, calls)).error(), D::SessionError::StaleSelection);
    wrong = token;
    ++wrong.step;
    EXPECT_EQ(session.choose(wrong, predicate(flags), effect(flags, calls)).error(), D::SessionError::StaleSelection);
    wrong = token;
    wrong.choice = {"not-a-choice"};
    EXPECT_EQ(session.choose(wrong, predicate(flags), effect(flags, calls)).error(), D::SessionError::UnknownChoice);
    D::Selection unissued;
    unissued.session = token.session;
    unissued.node = token.node;
    unissued.choice = token.choice;
    EXPECT_EQ(session.choose(unissued, predicate(flags), effect(flags, calls)).error(), D::SessionError::WrongIncarnation);
    EXPECT_EQ(session.snapshot(), before);
    EXPECT_FALSE(flags.introduced);
    EXPECT_EQ(calls, 0);
}

TEST(dialogue_session, cycles) {
    auto draft = gate();
    draft.nodes[1].choices[1].target = {"approach"};
    auto session = start(definition(draft));
    Flags flags;
    int calls = 0;
    const auto first = session.view();
    ASSERT_TRUE(first);
    const auto old = choice(*first, "introduce").selection;
    ASSERT_TRUE(session.choose(choice(*first, "silent").selection));
    EXPECT_EQ(session.choose(old, predicate(flags), effect(flags, calls)).error(), D::SessionError::StaleSelection);
    const auto at_gate = session.view(predicate(flags));
    ASSERT_TRUE(at_gate);
    ASSERT_TRUE(session.choose(choice(*at_gate, "leave").selection));
    EXPECT_EQ(session.snapshot()->node.value, old.node.value);
    EXPECT_EQ(session.choose(old, predicate(flags), effect(flags, calls)).error(), D::SessionError::StaleSelection);
    const auto fresh = session.view();
    ASSERT_TRUE(fresh);
    ASSERT_TRUE(session.choose(choice(*fresh, "introduce").selection, predicate(flags), effect(flags, calls)));
    EXPECT_EQ(
        session.choose(choice(*fresh, "introduce").selection, predicate(flags), effect(flags, calls)).error(),
        D::SessionError::StaleSelection
    );
    EXPECT_EQ(session.snapshot()->step, 3u);
    EXPECT_EQ(calls, 1);
}

TEST(dialogue_session, incarnations) {
    const auto content = definition();
    auto original = start(content);
    const auto before = original.snapshot();
    const auto presented = original.view();
    ASSERT_TRUE(before);
    ASSERT_TRUE(presented);
    const auto token = choice(*presented, "silent").selection;
    auto resumed = D::Session::resume(content, *before);
    ASSERT_TRUE(resumed);
    EXPECT_EQ(resumed->snapshot(), before); // Every persisted field is identical.
    EXPECT_EQ(resumed->choose(token).error(), D::SessionError::WrongIncarnation);
    auto same_name = start(content);
    EXPECT_EQ(same_name.choose(token).error(), D::SessionError::WrongIncarnation);
    ASSERT_TRUE(original.choose(token));
    original = D::Session::resume(content, *before).value(); // Rewind the same logical session.
    EXPECT_EQ(original.choose(token).error(), D::SessionError::WrongIncarnation);
    const auto current = original.view();
    ASSERT_TRUE(current);
    ASSERT_TRUE(original.choose(choice(*current, "silent").selection));
}

TEST(dialogue_session, independent) {
    const auto content = definition();
    auto first = start(content, {"one"});
    auto second = start(content, {"two"});
    const auto offered = first.view();
    ASSERT_TRUE(offered);
    const auto before = second.snapshot();
    EXPECT_EQ(second.choose(choice(*offered, "silent").selection).error(), D::SessionError::WrongSession);
    ASSERT_TRUE(first.choose(choice(*offered, "silent").selection));
    EXPECT_EQ(second.snapshot(), before);
    EXPECT_EQ(first.snapshot()->step, 1u);
    EXPECT_EQ(second.snapshot()->step, 0u);
}

TEST(dialogue_session, ownership) {
    std::weak_ptr<const D::Definition> lifetime;
    std::optional<D::View> retained;
    std::optional<D::Snapshot> saved;
    {
        auto session = [&] {
            auto content = definition();
            lifetime = content;
            return start(std::move(content));
        }();
        EXPECT_FALSE(lifetime.expired());
        retained = session.view().value();
        saved = session.snapshot().value();
        auto copied = *retained;
        copied.text = "Changed owned view";
        copied.choices.clear();
        EXPECT_EQ(session.view()->text, "A guard waits.");
    }
    EXPECT_TRUE(lifetime.expired()); // Views retain their small marker, not the graph.
    ASSERT_TRUE(retained);
    EXPECT_EQ(retained->text, "A guard waits.");
    EXPECT_EQ(retained->choices[0].text, "Introduce yourself");
    EXPECT_EQ(retained->choices[0].selection.session.value, "gate-0001");
    EXPECT_EQ(saved->definition.value, "gate-encounter");
    EXPECT_EQ(saved->node.value, "approach");
    auto resumed = D::Session::resume(definition(), *saved);
    ASSERT_TRUE(resumed);
    EXPECT_EQ(resumed->choose(choice(*retained, "silent").selection).error(), D::SessionError::WrongIncarnation);
}

TEST(dialogue_session, moves) {
    auto source = start();
    const auto offered = source.view();
    ASSERT_TRUE(offered);
    auto moved = std::move(source);
    EXPECT_EQ(source.view().error(), D::SessionError::MovedFrom);
    EXPECT_EQ(source.snapshot().error(), D::SessionError::MovedFrom);
    EXPECT_EQ(source.choose(choice(*offered, "silent").selection).error(), D::SessionError::MovedFrom);
    auto replaced = start();
    const auto discarded = replaced.view();
    ASSERT_TRUE(discarded);
    replaced = std::move(moved);
    EXPECT_EQ(moved.snapshot().error(), D::SessionError::MovedFrom);
    EXPECT_EQ(replaced.choose(choice(*discarded, "silent").selection).error(), D::SessionError::WrongIncarnation);
    ASSERT_TRUE(replaced.choose(choice(*offered, "silent").selection)); // Marker survived both moves.
}

TEST(dialogue_session, handlers) {
    auto session = start();
    std::weak_ptr<int> lifetime;
    {
        auto owner = std::make_shared<int>(0);
        lifetime = owner;
        const D::EffectHandler apply = [owner](const D::EffectKey&) { ++*owner; };
        const auto offered = session.view();
        ASSERT_TRUE(offered);
        ASSERT_TRUE(session.choose(choice(*offered, "introduce").selection, {}, apply));
        EXPECT_EQ(*owner, 1);
    }
    EXPECT_TRUE(lifetime.expired());
    {
        auto owner = std::make_shared<int>(0);
        lifetime = owner;
        const D::ConditionHandler test = [owner](const D::ConditionKey&) { return *owner == 0; };
        ASSERT_TRUE(session.view(test));
    }
    EXPECT_TRUE(lifetime.expired());
    EXPECT_EQ(session.snapshot()->node.value, "gate");
}

TEST(dialogue_session, guard_failure) {
    auto draft = gate();
    draft.entry = {"gate"};
    draft.nodes[1].choices[0].effect = D::EffectKey{"introduce"};
    auto session = start(definition(draft));
    const auto before = session.snapshot();
    const auto offered = session.view([](const D::ConditionKey&) { return true; });
    ASSERT_TRUE(before);
    ASSERT_TRUE(offered);
    const auto failure = std::make_exception_ptr(std::runtime_error("predicate failure"));
    const D::ConditionHandler fail = [&](const D::ConditionKey&) -> bool { std::rethrow_exception(failure); };
    int calls = 0;
    const D::EffectHandler apply = [&](const D::EffectKey&) { ++calls; };
    for (const bool viewing : {false, true}) {
        try {
            if (viewing)
                static_cast<void>(session.view(fail));
            else
                static_cast<void>(session.choose(choice(*offered, "pass").selection, fail, apply));
            FAIL() << "Predicate should throw";
        } catch (...) {
            EXPECT_EQ(std::current_exception(), failure);
        }
        EXPECT_EQ(session.snapshot(), before);
        EXPECT_EQ(calls, 0);
    }
    ASSERT_TRUE(session.choose(choice(*offered, "pass").selection, [](const D::ConditionKey&) { return true; }, apply));
    EXPECT_EQ(calls, 1);
}

TEST(dialogue_session, effect_failure) {
    auto session = start();
    const auto presented = session.view();
    ASSERT_TRUE(presented);
    const auto failure = std::make_exception_ptr(std::runtime_error("partial application mutation"));
    Flags flags;
    int calls = 0;
    const D::EffectHandler fail = [&](const D::EffectKey&) {
        ++calls;
        flags.introduced = true;
        std::rethrow_exception(failure);
    };
    try {
        static_cast<void>(session.choose(choice(*presented, "introduce").selection, {}, fail));
        FAIL() << "Effect should throw";
    } catch (...) {
        EXPECT_EQ(std::current_exception(), failure);
    }
    EXPECT_TRUE(flags.introduced); // No rollback of application effects is claimed.
    EXPECT_EQ(session.view().error(), D::SessionError::Faulted);
    EXPECT_EQ(session.snapshot().error(), D::SessionError::Faulted);
    EXPECT_EQ(session.choose(choice(*presented, "introduce").selection, {}, fail).error(), D::SessionError::Faulted);
    auto moved = std::move(session);
    EXPECT_EQ(moved.snapshot().error(), D::SessionError::Faulted);
    EXPECT_EQ(calls, 1);
}

TEST(dialogue_session, reentrant) {
    auto draft = gate();
    draft.entry = {"gate"};
    draft.nodes[1].choices[0].effect = D::EffectKey{"introduce"};
    auto session = start(definition(draft));
    const auto offered = session.view([](const D::ConditionKey&) { return true; });
    ASSERT_TRUE(offered);
    const auto token = choice(*offered, "pass").selection;
    int observations = 0;
    const auto reject = [&] {
        EXPECT_EQ(session.view().error(), D::SessionError::Busy);
        EXPECT_EQ(session.snapshot().error(), D::SessionError::Busy);
        EXPECT_EQ(session.choose(token).error(), D::SessionError::Busy);
        ++observations;
    };
    const D::ConditionHandler test = [&](const D::ConditionKey&) {
        reject();
        return true;
    };
    const D::EffectHandler apply = [&](const D::EffectKey&) { reject(); };
    ASSERT_TRUE(session.view(test));
    ASSERT_TRUE(session.choose(token, test, apply));
    EXPECT_EQ(observations, 3);
    EXPECT_EQ(session.snapshot()->step, 1u);
}

TEST(dialogue_session, lifecycle) {
    // An active object's storage/marker must not disappear beneath its callback.
    EXPECT_DEATH(
        {
            auto session = start();
            const auto offered = session.view().value();
            static_cast<void>(session.choose(choice(offered, "introduce").selection, {}, [&](const D::EffectKey&) {
                auto invalid = std::move(session);
                static_cast<void>(invalid);
            }));
        },
        "terminate"
    );
    EXPECT_DEATH(
        {
            auto session = start();
            auto replacement = start();
            const auto offered = session.view().value();
            static_cast<void>(session.choose(choice(offered, "introduce").selection, {}, [&](const D::EffectKey&) {
                session = std::move(replacement);
            }));
        },
        "terminate"
    );
    EXPECT_DEATH(
        {
            auto session = std::make_unique<D::Session>(start());
            const auto offered = session->view().value();
            static_cast<void>(session->choose(choice(offered, "introduce").selection, {}, [&](const D::EffectKey&) { session.reset(); }));
        },
        "terminate"
    );
}

TEST(dialogue_session, exhaustion) {
    auto draft = gate();
    draft.entry = {"gate"};
    draft.nodes[1].choices[0].effect = D::EffectKey{"introduce"};
    const auto content = definition(draft);
    auto initial = start(content).snapshot().value();
    initial.step = std::numeric_limits<std::uint64_t>::max();
    auto session = D::Session::resume(content, initial).value();
    int predicates = 0;
    int effects = 0;
    const D::ConditionHandler test = [&](const D::ConditionKey&) {
        ++predicates;
        return true;
    };
    const D::EffectHandler apply = [&](const D::EffectKey&) { ++effects; };
    const auto offered = session.view(test);
    ASSERT_TRUE(offered);
    predicates = 0;
    EXPECT_EQ(session.choose(choice(*offered, "pass").selection, test, apply).error(), D::SessionError::StepExhausted);
    EXPECT_EQ(session.snapshot().value(), initial);
    EXPECT_EQ(predicates, 0);
    EXPECT_EQ(effects, 0);
}

TEST(dialogue_session, resume_rejects) {
    const auto content = definition();
    auto session = start(content);
    const auto good = session.snapshot().value();
    const auto reject = [&](const D::Snapshot& saved, D::SessionError error) {
        const auto result = D::Session::resume(content, saved);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), error);
        EXPECT_EQ(session.snapshot().value(), good);
    };
    auto invalid = good;
    invalid.schema = 0;
    reject(invalid, D::SessionError::SnapshotSchemaMismatch);
    invalid.schema = D::Snapshot::current_schema + 1;
    reject(invalid, D::SessionError::SnapshotSchemaMismatch);
    invalid = good;
    invalid.definition = {"other-definition"};
    reject(invalid, D::SessionError::DefinitionMismatch);
    invalid.definition.value.clear();
    reject(invalid, D::SessionError::DefinitionMismatch);
    invalid = good;
    ++invalid.definition_revision;
    reject(invalid, D::SessionError::RevisionMismatch);
    invalid.definition_revision = 0;
    reject(invalid, D::SessionError::RevisionMismatch);
    invalid = good;
    invalid.session.value.clear();
    reject(invalid, D::SessionError::EmptySessionId);
    invalid = good;
    invalid.node = {"absent"};
    reject(invalid, D::SessionError::UnknownNode);
    invalid.node.value.clear();
    reject(invalid, D::SessionError::UnknownNode);
    EXPECT_EQ(D::Session::resume({}, good).error(), D::SessionError::InvalidDefinition);
    EXPECT_EQ(D::Session::start({}, {"id"}).error(), D::SessionError::InvalidDefinition);
    EXPECT_EQ(D::Session::start(content, {}).error(), D::SessionError::EmptySessionId);
}

TEST(dialogue_session, boundaries) {
    const auto content = definition();
    auto session = start(content);
    const auto initial = session.snapshot().value();
    EXPECT_EQ(initial.step, 0u);
    auto resumed = D::Session::resume(content, initial).value();
    const auto offered = resumed.view().value();
    ASSERT_TRUE(resumed.choose(choice(offered, "silent").selection));
    const Flags flags;
    const auto at_gate = resumed.view(predicate(flags)).value();
    ASSERT_TRUE(resumed.choose(choice(at_gate, "leave").selection));
    const auto terminal = resumed.snapshot().value();
    auto ended = D::Session::resume(content, terminal).value();
    EXPECT_EQ(ended.snapshot().value(), terminal);
    EXPECT_EQ(ended.view()->kind, D::NodeKind::Terminal);
    auto draft = gate();
    draft.entry = {"admitted"};
    auto terminal_start = start(definition(draft));
    EXPECT_EQ(terminal_start.snapshot()->node.value, "admitted");
    EXPECT_EQ(terminal_start.snapshot()->step, 0u);
}

TEST(dialogue_session, missing_handlers) {
    auto session = start();
    const auto offered = session.view().value();
    const auto before = session.snapshot();
    EXPECT_EQ(session.choose(choice(offered, "introduce").selection).error(), D::SessionError::MissingEffectHandler);
    EXPECT_EQ(session.snapshot(), before);
    ASSERT_TRUE(session.choose(choice(offered, "silent").selection));
    EXPECT_EQ(session.view().error(), D::SessionError::MissingConditionHandler);
    const Flags flags;
    const auto gate_view = session.view(predicate(flags)).value();
    EXPECT_EQ(session.choose(choice(gate_view, "pass").selection).error(), D::SessionError::MissingConditionHandler);
}
