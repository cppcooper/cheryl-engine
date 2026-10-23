#include "../src/score-input.h"
#include "../src/player-controller.h"

#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {
    using namespace CE::Input;
    using Tutorial::ScoreInput;
    using Key = KeyboardKey;
    using Status = ScoreInput::Status;
    constexpr DeviceId keyboard = 17;

    struct Polls {
        Actions::Bindings mapper;
        std::shared_ptr<const Actions::Snapshot> previous = mapper.action_snapshot();

        Tick take(std::shared_ptr<const Actions::Snapshot> next) {
            Tick tick(previous, {next});
            previous = std::move(next);
            return tick;
        }
        Tick poll() { return take(mapper.publish_actions()); }
        Tick idle() const { return Tick(previous, {}); }
        void key(Key key, bool held) { mapper.on_button({keyboard, key}, held); }
    };

    static_assert(!std::is_copy_constructible_v<ScoreInput>);
    static_assert(!std::is_copy_constructible_v<ScoreInput::Candidate>);
    static_assert(!std::is_copy_constructible_v<ScoreInput::Prepared>);
    static_assert(std::is_nothrow_move_constructible_v<ScoreInput> && std::is_nothrow_move_assignable_v<ScoreInput>);
    static_assert(std::is_nothrow_move_constructible_v<ScoreInput::Candidate> && std::is_nothrow_move_assignable_v<ScoreInput::Candidate>);
    static_assert(std::is_nothrow_move_constructible_v<ScoreInput::Prepared> && std::is_nothrow_move_assignable_v<ScoreInput::Prepared>);
    static_assert(noexcept(std::declval<ScoreInput&>().adopt(std::declval<ScoreInput::Prepared&&>())));
}

TEST(tutorial_score_input, installation) {
    Polls input;
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    const auto identity = score.active_context();
    EXPECT_NE(identity, 0u);
    EXPECT_EQ(score.key(), Key::Space);
    EXPECT_EQ(score.observe(input.idle()), Status::Waiting);
    const auto installed = input.poll();
    ASSERT_TRUE(installed.latest_poll()->has_context(identity));
    ASSERT_FALSE(installed.latest_poll()->has_changes());
    EXPECT_EQ(score.observe(input.idle()), Status::Waiting); // Membership in the baseline is insufficient.
    EXPECT_FALSE(score.read(installed).pressed());
    EXPECT_EQ(score.observe(installed), Status::Ready);
    EXPECT_EQ(score.observe(input.idle()), Status::Ready); // Exact active lease keeps its witnessed installation.
    input.key(Key::Space, true);
    const auto pressed = input.poll();
    EXPECT_EQ(score.read(pressed).press_count, 1u);
    EXPECT_TRUE(score.read(input.idle()).held());
    EXPECT_FALSE(score.read(input.idle()).pressed());
}

TEST(tutorial_score_input, poll_boundary) {
    Polls input;
    input.mapper.begin_poll();
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    const auto before = input.poll();
    EXPECT_FALSE(before.latest_poll()->has_context(score.active_context()));
    EXPECT_EQ(score.observe(before), Status::Waiting);
    const auto installed = input.poll();
    EXPECT_EQ(score.observe(installed), Status::Ready);
    score.set_generation(1);
    input.mapper.begin_poll();
    auto candidate = score.stage(Key::B);
    ASSERT_TRUE(candidate);
    const auto queued = input.poll();
    EXPECT_FALSE(queued.latest_poll()->has_context(candidate->id()));
    EXPECT_EQ(score.observe(*candidate, queued), Status::Waiting);
    EXPECT_THROW(static_cast<void>(score.prepare(std::move(*candidate))), std::logic_error);
    EXPECT_NE(candidate->id(), 0u); // Failed preparation does not consume the token.
    const auto attached = input.poll();
    EXPECT_TRUE(attached.latest_poll()->has_context(candidate->id()));
    EXPECT_EQ(score.observe(*candidate, input.idle()), Status::Waiting);
    EXPECT_EQ(score.observe(*candidate, attached), Status::Ready);
    EXPECT_EQ(score.observe(*candidate, input.idle()), Status::Ready);
    auto prepared = score.prepare(std::move(*candidate));
    EXPECT_TRUE(score.adopt(std::move(prepared)));
    EXPECT_EQ(score.key(), Key::B);
}

TEST(tutorial_score_input, held_seed) {
    Polls input;
    input.key(Key::Space, true);
    input.key(Key::B, true);
    static_cast<void>(input.poll());
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    const auto active = input.poll();
    EXPECT_EQ(score.observe(active), Status::Ready);
    EXPECT_EQ(score.read(active).press_count, 1u);
    auto candidate = score.stage(Key::B);
    ASSERT_TRUE(candidate);
    const auto seeded = input.poll();
    EXPECT_EQ(score.observe(*candidate, seeded), Status::Ready);
    EXPECT_EQ(score.read(seeded).press_count, 0u);
    EXPECT_TRUE(score.read(seeded).held());
    auto prepared = score.prepare(std::move(*candidate));
    ASSERT_TRUE(score.adopt(std::move(prepared)));
    EXPECT_EQ(score.read(seeded).press_count, 1u); // Future modal/rearm policy must suppress this seeded edge.
    EXPECT_TRUE(score.read(input.idle()).held());
    EXPECT_FALSE(score.read(input.idle()).pressed());
}

TEST(tutorial_score_input, unchanged) {
    Polls input;
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    const auto active = input.poll();
    score.observe(active);
    const auto identity = score.active_context();
    score.set_generation(7);
    EXPECT_FALSE(score.stage(Key::Space));
    EXPECT_EQ(score.active_context(), identity);
    EXPECT_EQ(score.key(), Key::Space);
    EXPECT_EQ(score.observe(input.idle()), Status::Ready);
    score.cancel();
    EXPECT_EQ(score.active_context(), identity);
    auto changed = score.stage(Key::B);
    ASSERT_TRUE(changed);
    EXPECT_EQ(changed->generation(), 7u);
    EXPECT_THROW(static_cast<void>(score.stage(Key::Space)), std::logic_error);
    EXPECT_NE(changed->id(), 0u);
}

TEST(tutorial_score_input, coexistence) {
    Polls input;
    const auto endpoint = input.mapper.action_contexts();
    const Actions::Id direct{1}; // Deliberate numeric overlap with context-local actions.
    static_cast<void>(input.mapper.bind_button({keyboard, Key::Space}, direct));
    Tutorial::PlayerController player(endpoint.attach(Tutorial::PlayerController::keyboard(keyboard, Key::A, Key::D)));
    ScoreInput score(endpoint, keyboard);
    input.key(Key::Space, true);
    input.key(Key::D, true);
    const auto active = input.poll();
    score.observe(active);
    const auto old = score.active_context();
    auto candidate = score.stage(Key::B);
    ASSERT_TRUE(candidate);
    const auto replacement = candidate->id();
    input.key(Key::B, true);
    const auto both = input.poll();
    EXPECT_TRUE(both.latest_poll()->has_context(old));
    EXPECT_TRUE(both.latest_poll()->has_context(replacement));
    EXPECT_EQ(score.observe(*candidate, both), Status::Ready);
    EXPECT_EQ(score.active_context(), old);
    EXPECT_EQ(score.key(), Key::Space);
    EXPECT_EQ(score.read(both).press_count, 0u); // Candidate presses never join active score input.
    EXPECT_TRUE(score.read(both).held());
    EXPECT_FLOAT_EQ(player.read(both).horizontal, 1);
    auto prepared = score.prepare(std::move(*candidate));
    EXPECT_EQ(score.active_context(), old);
    EXPECT_EQ(score.read(both).press_count, 0u);
    ASSERT_TRUE(score.adopt(std::move(prepared)));
    EXPECT_EQ(score.active_context(), replacement);
    EXPECT_EQ(score.key(), Key::B);
    EXPECT_EQ(score.read(both).press_count, 1u);
    EXPECT_EQ(prepared.id(), 0u);
    EXPECT_FALSE(score.adopt(std::move(prepared)));
    const auto release = input.poll();
    EXPECT_TRUE(release.latest_poll()->has_context(old));
    EXPECT_TRUE(release.actions(old).button(Actions::Id{1}).released());
    EXPECT_TRUE(release.button(direct).held());
    EXPECT_FALSE(release.button(direct).released());
    EXPECT_FLOAT_EQ(player.read(release).horizontal, 1);
    EXPECT_FALSE(score.read(release).released());
    const auto retired = input.poll();
    EXPECT_FALSE(retired.latest_poll()->has_context(old));
    EXPECT_TRUE(retired.latest_poll()->has_context(replacement));
    EXPECT_TRUE(score.read(retired).held());
}

TEST(tutorial_score_input, rejection) {
    Polls input;
    const auto endpoint = input.mapper.action_contexts();
    ScoreInput score(endpoint, keyboard);
    const auto identity = score.active_context();
    for (const auto key : {Key::A, Key::D, Key::J, Key::L, Key::Unknown, Key::Enter, Key::Keypad0}) {
        EXPECT_THROW(static_cast<void>(ScoreInput(endpoint, keyboard, key)), std::invalid_argument);
        EXPECT_THROW(static_cast<void>(score.stage(key)), std::invalid_argument);
        EXPECT_EQ(score.active_context(), identity);
        EXPECT_EQ(score.key(), Key::Space);
    }
    auto candidate = score.stage(Key::Digit0);
    ASSERT_TRUE(candidate);
    const auto pending = candidate->id();
    EXPECT_THROW(static_cast<void>(score.stage(Key::C)), std::logic_error);
    EXPECT_THROW(static_cast<void>(score.stage(Key::A)), std::invalid_argument);
    EXPECT_EQ(candidate->id(), pending);
    EXPECT_EQ(score.observe(*candidate, input.poll()), Status::Ready);
    auto prepared = score.prepare(std::move(*candidate));
    EXPECT_TRUE(score.adopt(std::move(prepared)));
    EXPECT_EQ(score.key(), Key::Digit0);
}

TEST(tutorial_score_input, stale_generation) {
    Polls input;
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    const auto active = score.active_context();
    score.set_generation(1);
    auto candidate = score.stage(Key::B);
    ASSERT_TRUE(candidate);
    const auto identity = candidate->id();
    const auto published = input.poll();
    EXPECT_EQ(score.observe(*candidate, published), Status::Ready);
    score.set_generation(2);
    EXPECT_EQ(score.observe(*candidate, published), Status::Stale);
    EXPECT_TRUE(published.latest_poll()->has_context(identity));
    EXPECT_THROW(static_cast<void>(score.prepare(std::move(*candidate))), std::logic_error);
    score.set_generation(1); // Reusing a number cannot revive the invalidated token.
    EXPECT_EQ(score.observe(*candidate, published), Status::Stale);
    auto current = score.stage(Key::C);
    ASSERT_TRUE(current);
    EXPECT_EQ(score.observe(*current, input.poll()), Status::Ready);
    auto prepared = score.prepare(std::move(*current));
    score.set_generation(3);
    EXPECT_FALSE(score.adopt(std::move(prepared)));
    EXPECT_EQ(score.active_context(), active);
    EXPECT_EQ(score.key(), Key::Space);
}

TEST(tutorial_score_input, cancel_before) {
    Polls input;
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    auto candidate = score.stage(Key::B);
    ASSERT_TRUE(candidate);
    const auto identity = candidate->id();
    const auto active = score.active_context();
    score.cancel();
    const auto published = input.poll();
    EXPECT_FALSE(published.latest_poll()->has_context(identity));
    EXPECT_TRUE(published.latest_poll()->has_context(active));
    EXPECT_EQ(score.observe(*candidate, published), Status::Cancelled);
    EXPECT_THROW(static_cast<void>(score.prepare(std::move(*candidate))), std::logic_error);
    EXPECT_EQ(score.active_context(), active);
    EXPECT_EQ(score.key(), Key::Space);
}

TEST(tutorial_score_input, cancel_after) {
    Polls input;
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    auto candidate = score.stage(Key::B);
    ASSERT_TRUE(candidate);
    const auto identity = candidate->id();
    input.key(Key::B, true);
    const auto published = input.poll();
    EXPECT_EQ(score.observe(*candidate, published), Status::Ready);
    score.cancel();
    EXPECT_EQ(score.observe(*candidate, published), Status::Cancelled);
    EXPECT_TRUE(published.latest_poll()->has_context(identity));
    const auto release = input.poll();
    EXPECT_TRUE(release.latest_poll()->has_context(identity));
    EXPECT_TRUE(release.actions(identity).button(Actions::Id{1}).released());
    EXPECT_FALSE(input.poll().latest_poll()->has_context(identity));
    EXPECT_EQ(score.key(), Key::Space);
}

TEST(tutorial_score_input, drop_before) {
    Polls input;
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    auto candidate = score.stage(Key::B);
    ASSERT_TRUE(candidate);
    const auto identity = candidate->id();
    candidate.reset();
    EXPECT_FALSE(input.poll().latest_poll()->has_context(identity));
    EXPECT_EQ(score.key(), Key::Space);
    EXPECT_TRUE(score.stage(Key::C)); // The expired weak slot permits another request.
}

TEST(tutorial_score_input, drop_after) {
    Polls input;
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    auto candidate = score.stage(Key::B);
    ASSERT_TRUE(candidate);
    const auto identity = candidate->id();
    input.key(Key::B, true);
    static_cast<void>(input.poll());
    candidate.reset();
    const auto final_sample = input.poll();
    EXPECT_TRUE(final_sample.latest_poll()->has_context(identity));
    EXPECT_TRUE(final_sample.actions(identity).button(Actions::Id{1}).released());
    EXPECT_FALSE(input.poll().latest_poll()->has_context(identity));
    EXPECT_EQ(score.key(), Key::Space);
    EXPECT_TRUE(score.stage(Key::C));
}

TEST(tutorial_score_input, prepared_lifetime) {
    Polls input;
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    auto candidate = score.stage(Key::B);
    ASSERT_TRUE(candidate);
    const auto identity = candidate->id();
    const auto active = score.active_context();
    EXPECT_EQ(score.observe(*candidate, input.poll()), Status::Ready);
    {
        auto prepared = score.prepare(std::move(*candidate));
        EXPECT_EQ(candidate->id(), 0u);
        EXPECT_EQ(prepared.id(), identity);
        EXPECT_THROW(static_cast<void>(score.stage(Key::C)), std::logic_error);
    }
    EXPECT_EQ(score.active_context(), active);
    auto again = score.stage(Key::C);
    ASSERT_TRUE(again);
    EXPECT_EQ(score.observe(*again, input.poll()), Status::Ready);
    auto prepared = score.prepare(std::move(*again));
    score.cancel();
    EXPECT_FALSE(score.adopt(std::move(prepared)));
    EXPECT_EQ(score.active_context(), active);
}

TEST(tutorial_score_input, clear) {
    Polls input;
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    auto candidate = score.stage(Key::B);
    ASSERT_TRUE(candidate);
    const auto active = score.active_context(), pending = candidate->id();
    const auto historical = input.poll();
    score.observe(historical);
    EXPECT_EQ(score.observe(*candidate, historical), Status::Ready);
    input.mapper.clear();
    const auto final_sample = input.take(input.mapper.action_snapshot());
    EXPECT_TRUE(final_sample.latest_poll()->has_context(active));
    EXPECT_TRUE(final_sample.latest_poll()->has_context(pending));
    EXPECT_EQ(score.observe(final_sample), Status::Invalidated);
    EXPECT_EQ(score.observe(*candidate, historical), Status::Invalidated);
    EXPECT_FALSE(score.read(historical).held());
    EXPECT_THROW(static_cast<void>(score.prepare(std::move(*candidate))), std::logic_error);
    EXPECT_THROW(static_cast<void>(score.stage(Key::C)), std::logic_error);
    EXPECT_EQ(score.active_context(), active);
    EXPECT_EQ(score.key(), Key::Space);
    const auto retired = input.poll();
    EXPECT_FALSE(retired.latest_poll()->has_context(active));
    EXPECT_FALSE(retired.latest_poll()->has_context(pending));
}

TEST(tutorial_score_input, shutdown) {
    Actions::ContextSubmission endpoint;
    std::optional<ScoreInput> score;
    ScoreInput::Prepared prepared;
    std::optional<Tick> historical;
    Actions::ContextId active = 0, pending = 0;
    {
        Polls input;
        endpoint = input.mapper.action_contexts();
        score.emplace(endpoint, keyboard);
        active = score->active_context();
        auto candidate = score->stage(Key::B);
        ASSERT_TRUE(candidate);
        pending = candidate->id();
        historical.emplace(input.poll());
        score->observe(*historical);
        EXPECT_EQ(score->observe(*candidate, *historical), Status::Ready);
        prepared = score->prepare(std::move(*candidate));
    }
    EXPECT_TRUE(historical->latest_poll()->has_context(active));
    EXPECT_TRUE(historical->latest_poll()->has_context(pending));
    EXPECT_EQ(score->observe(*historical), Status::Invalidated);
    EXPECT_FALSE(score->adopt(std::move(prepared)));
    EXPECT_EQ(score->active_context(), active);
    EXPECT_EQ(score->key(), Key::Space);
    EXPECT_THROW(static_cast<void>(score->stage(Key::C)), std::logic_error);
    EXPECT_THROW(static_cast<void>(ScoreInput(endpoint, keyboard)), CE::Exceptions::failed_operation);
    EXPECT_THROW(static_cast<void>(ScoreInput(Actions::ContextSubmission{}, keyboard)), CE::Exceptions::failed_operation);
}

TEST(tutorial_score_input, owner_move) {
    Polls input;
    ScoreInput original(input.mapper.action_contexts(), keyboard);
    original.set_generation(13);
    auto candidate = original.stage(Key::B);
    ASSERT_TRUE(candidate);
    const auto installed = input.poll();
    original.observe(installed);
    EXPECT_EQ(original.observe(*candidate, installed), Status::Ready);
    const auto active = original.active_context();
    ScoreInput moved(std::move(original));
    EXPECT_EQ(moved.active_context(), active);
    EXPECT_EQ(moved.observe(input.idle()), Status::Ready);
    EXPECT_EQ(original.active_context(), 0u);
    EXPECT_EQ(original.observe(installed), Status::Invalidated);
    EXPECT_EQ(original.observe(*candidate, installed), Status::Foreign);
    original.cancel();
    original.set_generation(99);
    EXPECT_THROW(static_cast<void>(original.stage(Key::C)), std::logic_error);
    auto prepared = moved.prepare(std::move(*candidate));
    EXPECT_FALSE(original.adopt(std::move(prepared)));
    EXPECT_TRUE(moved.adopt(std::move(prepared)));
    EXPECT_EQ(moved.key(), Key::B);
}

TEST(tutorial_score_input, owner_assign) {
    Polls input;
    const auto endpoint = input.mapper.action_contexts();
    ScoreInput target(endpoint, keyboard, Key::Digit0), source(endpoint, keyboard);
    target.set_generation(5);
    source.set_generation(9);
    auto old = target.stage(Key::C), incoming = source.stage(Key::B);
    ASSERT_TRUE(old);
    ASSERT_TRUE(incoming);
    const auto retired_active = target.active_context(), retired_pending = old->id(), source_active = source.active_context();
    const auto installed = input.poll();
    target.observe(installed);
    source.observe(installed);
    EXPECT_EQ(target.observe(*old, installed), Status::Ready);
    EXPECT_EQ(source.observe(*incoming, installed), Status::Ready);
    auto retired_prepared = target.prepare(std::move(*old));
    target = std::move(source);
    EXPECT_EQ(target.active_context(), source_active);
    EXPECT_EQ(target.key(), Key::Space);
    EXPECT_EQ(source.active_context(), 0u);
    EXPECT_FALSE(target.adopt(std::move(retired_prepared)));
    auto prepared = target.prepare(std::move(*incoming));
    EXPECT_TRUE(target.adopt(std::move(prepared)));
    const auto final_sample = input.poll();
    EXPECT_TRUE(final_sample.latest_poll()->has_context(retired_active));
    EXPECT_TRUE(final_sample.latest_poll()->has_context(retired_pending));
    const auto retired = input.poll();
    EXPECT_FALSE(retired.latest_poll()->has_context(retired_active));
    EXPECT_FALSE(retired.latest_poll()->has_context(retired_pending));
    EXPECT_EQ(target.key(), Key::B);
}

TEST(tutorial_score_input, candidate_moves) {
    Polls input;
    const auto endpoint = input.mapper.action_contexts();
    ScoreInput first(endpoint, keyboard), second(endpoint, keyboard);
    auto one = first.stage(Key::B), two = second.stage(Key::C);
    ASSERT_TRUE(one);
    ASSERT_TRUE(two);
    const auto dropped = one->id(), kept = two->id();
    const auto installed = input.poll();
    EXPECT_EQ(first.observe(*one, installed), Status::Ready);
    EXPECT_EQ(second.observe(*two, installed), Status::Ready);
    ScoreInput::Candidate moved(std::move(*one));
    EXPECT_EQ(one->id(), 0u);
    EXPECT_EQ(moved.id(), dropped);
    moved = std::move(*two);
    EXPECT_EQ(two->id(), 0u);
    EXPECT_EQ(moved.id(), kept);
    EXPECT_EQ(first.observe(moved, installed), Status::Foreign);
    EXPECT_TRUE(first.stage(Key::Digit0)); // The overwritten token's lease was released once.
    auto prepared = second.prepare(std::move(moved));
    EXPECT_TRUE(second.adopt(std::move(prepared)));
    const auto final_sample = input.poll();
    EXPECT_TRUE(final_sample.latest_poll()->has_context(dropped));
    EXPECT_FALSE(input.poll().latest_poll()->has_context(dropped));
    EXPECT_EQ(first.key(), Key::Space);
    EXPECT_EQ(second.key(), Key::C);
}

TEST(tutorial_score_input, prepared_moves) {
    Polls input;
    const auto endpoint = input.mapper.action_contexts();
    ScoreInput first(endpoint, keyboard), second(endpoint, keyboard);
    auto one = first.stage(Key::B), two = second.stage(Key::C);
    ASSERT_TRUE(one);
    ASSERT_TRUE(two);
    const auto installed = input.poll();
    EXPECT_EQ(first.observe(*one, installed), Status::Ready);
    EXPECT_EQ(second.observe(*two, installed), Status::Ready);
    auto original = first.prepare(std::move(*one));
    const auto dropped = original.id();
    auto other = second.prepare(std::move(*two));
    const auto kept = other.id();
    ScoreInput::Prepared moved(std::move(original));
    EXPECT_EQ(original.id(), 0u);
    moved = std::move(other);
    EXPECT_EQ(other.id(), 0u);
    EXPECT_EQ(moved.id(), kept);
    EXPECT_FALSE(first.adopt(std::move(moved)));
    EXPECT_EQ(moved.id(), kept); // Wrong-owner rejection neither consumes nor mutates it.
    EXPECT_TRUE(second.adopt(std::move(moved)));
    EXPECT_TRUE(first.stage(Key::Digit0));
    EXPECT_TRUE(input.poll().latest_poll()->has_context(dropped));
    EXPECT_FALSE(input.poll().latest_poll()->has_context(dropped));
}

TEST(tutorial_score_input, owner_lifetime) {
    Polls input;
    std::optional<ScoreInput::Candidate> candidate;
    Actions::ContextId active = 0, pending = 0;
    {
        ScoreInput score(input.mapper.action_contexts(), keyboard);
        active = score.active_context();
        candidate = score.stage(Key::B);
        ASSERT_TRUE(candidate);
        pending = candidate->id();
        const auto installed = input.poll();
        EXPECT_EQ(score.observe(*candidate, installed), Status::Ready);
    }
    EXPECT_EQ(candidate->id(), 0u); // Outstanding tokens cannot extend their score owner's request.
    const auto final_sample = input.poll();
    EXPECT_TRUE(final_sample.latest_poll()->has_context(active));
    EXPECT_TRUE(final_sample.latest_poll()->has_context(pending));
    const auto retired = input.poll();
    EXPECT_FALSE(retired.latest_poll()->has_context(active));
    EXPECT_FALSE(retired.latest_poll()->has_context(pending));
}

TEST(tutorial_score_input, foreign) {
    Polls input;
    const auto endpoint = input.mapper.action_contexts();
    ScoreInput owner(endpoint, keyboard), stranger(endpoint, keyboard);
    owner.set_generation(4);
    stranger.set_generation(4);
    auto candidate = owner.stage(Key::B);
    ASSERT_TRUE(candidate);
    const auto installed = input.poll();
    EXPECT_EQ(stranger.observe(*candidate, installed), Status::Foreign);
    EXPECT_THROW(static_cast<void>(stranger.prepare(std::move(*candidate))), std::logic_error);
    EXPECT_EQ(owner.observe(*candidate, installed), Status::Ready);
    auto prepared = owner.prepare(std::move(*candidate));
    const auto identity = prepared.id(), stranger_active = stranger.active_context();
    EXPECT_FALSE(stranger.adopt(std::move(prepared)));
    EXPECT_EQ(prepared.id(), identity);
    EXPECT_EQ(stranger.active_context(), stranger_active);
    EXPECT_EQ(stranger.key(), Key::Space);
    EXPECT_TRUE(owner.adopt(std::move(prepared)));
}

TEST(tutorial_score_input, empty_tokens) {
    Polls input;
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    ScoreInput::Candidate empty;
    EXPECT_EQ(empty.id(), 0u);
    EXPECT_EQ(score.observe(empty, input.idle()), Status::Invalidated);
    EXPECT_THROW(static_cast<void>(score.prepare(std::move(empty))), std::logic_error);
    ScoreInput::Prepared prepared;
    const auto active = score.active_context();
    EXPECT_FALSE(score.adopt(std::move(prepared)));
    EXPECT_EQ(score.active_context(), active);
}
