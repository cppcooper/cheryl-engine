#include <core/controls/action-context.h>
#include <core/controls/input-accumulator.h>
#include <core/controls/input-interface.h>
#include <core/controls/polling-backlog.h>
#include <core/controls/tick-input.h>
#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <chrono>
#include <future>
#include <limits>
#include <optional>
#include <utility>

namespace {
    using namespace CE::Input;
    constexpr Actions::Id fire{1}, move{2}, look{3};
    const Control first{1, KeyboardKey::A}, second{2, KeyboardKey::A};
    const Control modifier{1, KeyboardKey::LeftShift};
    const Control stick{2, GamepadControl::LeftStickX}, alternate{3, GamepadControl::LeftStickX};
    const Control scroll{1, MouseControl::ScrollY};

    Actions::ContextDefinition button(const Control control = first) {
        return {.buttons = {{{{control}}, fire}}};
    }

    class StateInput final : public System {
    public:
        Actions::Bindings mapper;
        bool explicit_boundary = false;

        void initialize(CE::Platform::Window&) override {}
        void deinitialize() override { mapper.clear(); }
        void poll() override {
            if (explicit_boundary)
                begin_input_poll();
            static_cast<void>(mapper.publish_actions());
        }
        Actions::Bindings& bindings() override { return mapper; }
        DeviceId keyboard_id() const override { return 1; }
        DeviceId mouse_id() const override { return 4; }
        DeviceId gamepad_id() const override { return 2; }
        bool supports_focus() const override { return true; }
    };
}

TEST(action_contexts, presence) {
    const Actions::Snapshot empty;
    EXPECT_FALSE(empty.has_context(0));
    EXPECT_FALSE(empty.has_context(42));
    Actions::Bindings mapper;
    const auto baseline = mapper.action_snapshot();
    const auto endpoint = mapper.action_contexts();
    auto blank = endpoint.attach({});
    auto inactive = endpoint.attach(button());
    auto abandoned = endpoint.attach({});
    const auto abandoned_id = abandoned.id();
    abandoned.reset();
    EXPECT_TRUE(blank);
    EXPECT_TRUE(inactive);
    EXPECT_FALSE(baseline->has_context(blank.id()));
    EXPECT_FALSE(baseline->has_context(inactive.id()));

    const auto installed = mapper.publish_actions();
    EXPECT_TRUE(installed->has_context(blank.id()));
    EXPECT_TRUE(installed->has_context(inactive.id()));
    EXPECT_FALSE(installed->has_context(abandoned_id));
    EXPECT_FALSE(installed->has_context(0));
    EXPECT_FALSE(installed->button(inactive.id(), fire).held());
    EXPECT_FALSE(installed->has_changes());
    EXPECT_FALSE(baseline->has_context(blank.id()));

    const auto identity = blank.id();
    blank.reset();
    const auto departing = mapper.publish_actions();
    EXPECT_TRUE(departing->has_context(identity));
    EXPECT_FALSE(departing->has_changes());
    const auto retired = mapper.publish_actions();
    EXPECT_FALSE(retired->has_context(identity));
    EXPECT_TRUE(retired->has_context(inactive.id()));
    EXPECT_TRUE(installed->has_context(identity));
}

TEST(action_contexts, device_admission) {
    const Actions::Snapshot empty;
    EXPECT_TRUE(empty.device_enabled(0));
    EXPECT_TRUE(empty.device_enabled(99)); // Admission is not hardware discovery.
    Actions::Bindings mapper;
    const auto initial = mapper.action_snapshot();
    mapper.set_device_enabled(1, false);
    EXPECT_TRUE(mapper.action_snapshot()->device_enabled(1)); // Staging is not publication.
    const auto disabled = mapper.publish_actions();
    const auto copied = *disabled;
    EXPECT_FALSE(disabled->device_enabled(1));
    EXPECT_TRUE(disabled->device_enabled(2));
    EXPECT_FALSE(disabled->has_changes());
    mapper.set_device_enabled(1, true);
    mapper.set_device_enabled(2, false);
    const auto enabled = mapper.publish_actions();
    EXPECT_TRUE(enabled->device_enabled(1));
    EXPECT_FALSE(enabled->device_enabled(2));
    EXPECT_TRUE(enabled->device_enabled(99));
    EXPECT_FALSE(enabled->has_changes());
    EXPECT_TRUE(initial->device_enabled(1));
    EXPECT_FALSE(disabled->device_enabled(1));
    EXPECT_TRUE(disabled->device_enabled(2));
    EXPECT_FALSE(copied.device_enabled(1));
    EXPECT_TRUE(copied.device_enabled(2));
}

TEST(action_contexts, queued_metadata) {
    StateInput input;
    input.explicit_boundary = true;
    auto context = input.action_contexts().attach({});
    input.poll();
    const auto baseline = input.action_snapshot();
    Accumulator delayed(baseline, baseline->observed_at());
    auto focus = input.routing().focus(7);
    input.poll();
    const auto disabled = input.poll_snapshot();
    focus.reset();
    input.poll();
    const auto enabled = input.poll_snapshot();
    EXPECT_FALSE(disabled->state->has_changes());
    EXPECT_FALSE(enabled->state->has_changes());
    EXPECT_TRUE(input.action_snapshot()->device_enabled(input.keyboard_id()));
    PollingBacklog backlog({PollingPolicy::Finite, 2, ObservationClock::duration::zero()});
    backlog.complete(disabled, disabled->state->observed_at());
    backlog.complete(enabled, enabled->state->observed_at());
    EXPECT_EQ(backlog.completed_polls(), 2u);
    auto samples = backlog.consume();
    ASSERT_EQ(samples.size(), 2u);

    const auto queued = delayed.consume_polls(enabled->state->observed_at(), {samples.front()});
    ASSERT_EQ(queued.polls().size(), 1u);
    EXPECT_TRUE(queued.latest_poll()->has_context(context.id()));
    EXPECT_FALSE(queued.latest_poll()->device_enabled(input.keyboard_id()));
    const auto idle = delayed.consume_polls(enabled->state->observed_at(), {});
    EXPECT_TRUE(idle.polls().empty());
    EXPECT_EQ(idle.latest_poll(), disabled->state); // Baseline is not a newly consumed poll.
    const auto resumed = delayed.consume_polls(enabled->state->observed_at(), {samples.back()});
    ASSERT_EQ(resumed.polls().size(), 1u);
    EXPECT_GT(resumed.latest_poll()->poll(), queued.latest_poll()->poll());
    EXPECT_TRUE(resumed.latest_poll()->device_enabled(input.keyboard_id()));

    Accumulator batched(baseline, baseline->observed_at());
    const auto together = batched.consume_polls(enabled->state->observed_at(), std::move(samples));
    ASSERT_EQ(together.polls().size(), 2u);
    EXPECT_FALSE(together.polls()[0]->device_enabled(input.keyboard_id()));
    EXPECT_TRUE(together.polls()[1]->device_enabled(input.keyboard_id()));
}

TEST(action_contexts, isolation) {
    Actions::Bindings mapper;
    static_cast<void>(mapper.bind_button({3, KeyboardKey::A}, fire));
    const auto baseline = mapper.action_snapshot();
    const auto endpoint = mapper.action_contexts();
    auto one = endpoint.attach(button());
    auto two = endpoint.attach(button(second));
    mapper.on_button(second, true);
    const auto sample = mapper.publish_actions();
    EXPECT_TRUE(sample->has_changes()); // Only a contextual action changed.
    const Tick tick(baseline, {sample});
    EXPECT_FALSE(tick.button(fire).held());
    EXPECT_FALSE(tick.actions(one).button(fire).held());
    EXPECT_TRUE(tick.actions(two).button(fire).pressed());
    EXPECT_NE(one.id(), two.id());
    EXPECT_FALSE(tick.actions(Actions::Context{}).button(fire).held());
    mapper.on_button({3, KeyboardKey::A}, true);
    const Tick next(sample, {mapper.publish_actions()});
    EXPECT_TRUE(next.button(fire).held());
    EXPECT_TRUE(next.actions(two).button(fire).held());
    EXPECT_FALSE(next.actions(Actions::Context{}).button(fire).held());
    auto moved = std::move(two);
    EXPECT_EQ(two.id(), 0u);
    EXPECT_FALSE(next.actions(two).button(fire).held());
    EXPECT_TRUE(next.actions(moved).button(fire).held());
}

TEST(action_contexts, mappings) {
    Actions::Bindings mapper;
    const auto baseline = mapper.action_snapshot();
    Actions::ContextDefinition definition;
    definition.buttons = {{{{modifier, first}}, fire}, {{{second}}, fire}};
    definition.axes = {
        {{}, stick, move, {.dead_zone = 0.2f}},
        {{}, alternate, move, {.scale = 2}},
        {{{modifier}}, scroll, look, {.scale = 2, .kind = AxisKind::Relative}}
    };
    auto context = mapper.action_contexts().attach(std::move(definition));
    mapper.on_button(modifier, true);
    mapper.on_button(first, true);
    mapper.on_axis(stick, 0.6f);
    mapper.on_axis(alternate, 0.25f);
    mapper.on_delta(scroll, 3);
    mapper.on_button(second, true);
    mapper.on_button(first, false); // Alternative preserves the same semantic hold.
    const auto sample = mapper.publish_actions();
    const Tick tick(baseline, {sample});
    const auto actions = tick.actions(context);
    EXPECT_EQ(actions.button(fire).press_count, 1u);
    EXPECT_EQ(actions.button(fire).release_count, 0u);
    EXPECT_NEAR(actions.axis(move).current, 1, 0.0001f);
    EXPECT_FLOAT_EQ(actions.axis(look).current, 6);
}

TEST(action_contexts, departure) {
    Actions::Bindings mapper;
    static_cast<void>(mapper.bind_button(first, fire));
    auto definition = button();
    definition.axes = {{{}, stick, move, {}}};
    auto context = mapper.action_contexts().attach(std::move(definition));
    const auto identity = context.id();
    const auto baseline = mapper.action_snapshot();
    mapper.on_button(first, true);
    mapper.on_axis(stick, 0.75f);
    const auto active = mapper.publish_actions();
    EXPECT_TRUE(active->has_context(identity));
    const Tick old(baseline, {active});
    const auto saved = old.actions(context);
    context.reset();
    EXPECT_FALSE(context);
    const auto removed = mapper.publish_actions();
    EXPECT_TRUE(removed->has_context(identity));
    const Tick tick(active, {removed});
    EXPECT_TRUE(tick.actions(identity).button(fire).released());
    EXPECT_FALSE(tick.actions(identity).button(fire).held());
    EXPECT_FLOAT_EQ(tick.actions(identity).axis(move).current, 0);
    EXPECT_TRUE(tick.button(fire).held()); // Direct registrations survive.
    EXPECT_TRUE(saved.button(fire).held());
    EXPECT_FLOAT_EQ(saved.axis(move).current, 0.75f);
    const Tick later(removed, {mapper.publish_actions()});
    EXPECT_FALSE(later.latest_poll()->has_context(identity));
    EXPECT_TRUE(active->has_context(identity));
    EXPECT_FALSE(later.actions(identity).button(fire).released());
}

TEST(action_contexts, poll_boundary) {
    Actions::Bindings mapper;
    const auto endpoint = mapper.action_contexts();
    mapper.on_button(first, true); // Starts an implicit State-only adapter poll.
    mapper.on_axis(stick, 0.75f);
    mapper.on_delta(scroll, 10);
    auto definition = button();
    definition.axes = {{{}, stick, move, {}}, {{}, scroll, look, {.kind = AxisKind::Relative}}};
    auto context = endpoint.attach(std::move(definition));
    const auto before = mapper.publish_actions();
    EXPECT_FALSE(before->has_context(context.id()));
    EXPECT_FALSE(before->button(context.id(), fire).held());
    const auto attached = mapper.publish_actions(); // Empty next poll still applies requests.
    EXPECT_TRUE(attached->has_context(context.id()));
    const Tick tick(before, {attached});
    EXPECT_TRUE(tick.actions(context).button(fire).pressed());
    EXPECT_FLOAT_EQ(tick.actions(context).axis(move).current, 0.75f);
    EXPECT_FLOAT_EQ(tick.actions(context).axis(look).current, 0);

    auto held = endpoint.attach(button());
    mapper.begin_poll(); // Attach to the existing physical hold before its release.
    mapper.on_button(first, false);
    const Tick released(attached, {mapper.publish_actions()});
    EXPECT_TRUE(released.latest_poll()->has_context(held.id()));
    EXPECT_TRUE(released.actions(held).button(fire).pressed());
    EXPECT_TRUE(released.actions(held).button(fire).released());
}

TEST(action_contexts, latched_departure) {
    Actions::Bindings mapper;
    mapper.on_button(first, true);
    const auto baseline = mapper.publish_actions();
    auto context = mapper.action_contexts().attach(button());
    const auto identity = context.id();
    mapper.begin_poll();
    context.reset(); // Owner has already latched this poll's namespace membership.
    const auto attached = mapper.publish_actions();
    EXPECT_TRUE(attached->has_context(identity)); // Historical presence survives reset.
    const auto current = Tick(baseline, {attached}).actions(identity).button(fire);
    EXPECT_TRUE(current.held());
    EXPECT_TRUE(current.pressed());
    EXPECT_FALSE(current.released());
    const auto final_sample = mapper.publish_actions();
    EXPECT_TRUE(final_sample->has_context(identity));
    const auto departure = Tick(attached, {final_sample}).actions(identity).button(fire);
    EXPECT_FALSE(departure.held());
    EXPECT_TRUE(departure.released());
    EXPECT_FALSE(mapper.publish_actions()->has_context(identity));
}

TEST(action_contexts, clear) {
    Actions::Bindings mapper;
    const auto endpoint = mapper.action_contexts();
    auto active = endpoint.attach(button());
    auto blank = endpoint.attach({});
    mapper.set_device_enabled(2, false);
    mapper.on_button(first, true);
    const auto held = mapper.publish_actions();
    const auto active_id = active.id(), blank_id = blank.id();
    ASSERT_TRUE(held->has_context(active_id));
    ASSERT_TRUE(held->button(active_id, fire).held());
    mapper.begin_poll();
    auto pending = endpoint.attach({});
    const auto pending_id = pending.id();
    mapper.clear();
    EXPECT_FALSE(active);
    EXPECT_FALSE(blank);
    EXPECT_FALSE(pending);
    const auto final_sample = mapper.action_snapshot();
    EXPECT_TRUE(final_sample->has_context(active_id));
    EXPECT_TRUE(final_sample->has_context(blank_id));
    EXPECT_FALSE(final_sample->has_context(pending_id));
    EXPECT_TRUE(final_sample->button(active_id, fire).released());
    EXPECT_TRUE(final_sample->device_enabled(2));
    EXPECT_FALSE(held->device_enabled(2));
    EXPECT_TRUE(held->has_context(active_id));
    const auto retired = mapper.publish_actions();
    EXPECT_FALSE(retired->has_context(active_id));
    EXPECT_FALSE(retired->has_context(blank_id));
    auto fresh = endpoint.attach({});
    EXPECT_TRUE(fresh);
    EXPECT_FALSE(retired->has_context(fresh.id()));
    EXPECT_TRUE(mapper.publish_actions()->has_context(fresh.id()));
}

TEST(action_contexts, focus_gate) {
    StateInput input;
    input.explicit_boundary = true;
    auto context = input.action_contexts().attach(button());
    input.mapper.on_button(first, true);
    input.poll();
    const auto active = input.action_snapshot();
    auto focus = input.routing().focus(7);
    input.poll();
    const auto suppressed = input.action_snapshot();
    EXPECT_TRUE(Tick(active, {suppressed}).actions(context).button(fire).released());
    focus.reset();
    input.poll();
    EXPECT_TRUE(Tick(suppressed, {input.action_snapshot()}).actions(context).button(fire).pressed());
}

TEST(action_contexts, attach_under_focus) {
    StateInput input;
    input.explicit_boundary = true;
    input.mapper.on_button(first, true);
    input.poll();
    const auto baseline = input.action_snapshot();
    auto context = input.action_contexts().attach(button());
    auto focus = input.routing().focus(7);
    input.poll();
    const auto suppressed = input.action_snapshot();
    const auto actions = Tick(baseline, {suppressed}).actions(context).button(fire);
    EXPECT_FALSE(actions.held());
    EXPECT_FALSE(actions.pressed());
    EXPECT_FALSE(actions.released());

    const auto identity = context.id();
    context.reset();
    focus.reset();
    input.poll();
    const auto removed = Tick(suppressed, {input.action_snapshot()}).actions(identity).button(fire);
    EXPECT_FALSE(removed.held());
    EXPECT_FALSE(removed.pressed());
    EXPECT_FALSE(removed.released());
}

TEST(action_contexts, departure_after_gate) {
    Actions::Bindings mapper;
    mapper.set_device_enabled(1, false);
    auto context = mapper.action_contexts().attach(button());
    mapper.on_button(first, true);
    const auto suppressed = mapper.publish_actions();
    mapper.set_device_enabled(1, true); // Policy setup precedes the next poll.
    const auto identity = context.id();
    context.reset();
    const auto removed = Tick(suppressed, {mapper.publish_actions()}).actions(identity).button(fire);
    EXPECT_FALSE(removed.held());
    EXPECT_FALSE(removed.pressed());
    EXPECT_FALSE(removed.released());
}

TEST(action_contexts, state_adapter) {
    StateInput input;
    const auto endpoint = input.action_contexts();
    auto pending = std::async(std::launch::async, [endpoint] { return endpoint.attach(button()); });
    auto context = pending.get();
    const auto baseline = input.action_snapshot();
    input.mapper.on_button(first, true);
    input.poll(); // Does not call begin_input_poll().
    const auto sample = input.poll_snapshot();
    ASSERT_TRUE(sample);
    EXPECT_TRUE(Tick(baseline, {sample->state}).actions(context).button(fire).held());
    const auto identity = context.id();
    auto removal = std::async(std::launch::async, [context = std::move(context)]() mutable { context.reset(); });
    removal.get();
    input.poll();
    EXPECT_TRUE(Tick(sample->state, {input.poll_snapshot()->state}).actions(identity).button(fire).released());
}

TEST(action_contexts, lifetime) {
    Actions::ContextSubmission endpoint;
    Actions::Context retained;
    std::optional<Tick> old;
    {
        Actions::Bindings mapper;
        endpoint = mapper.action_contexts();
        auto abandoned = endpoint.attach(button());
        abandoned.reset();
        retained = endpoint.attach(button());
        const auto baseline = mapper.action_snapshot();
        mapper.on_button(first, true);
        old.emplace(baseline, std::vector{mapper.publish_actions()});
        EXPECT_TRUE(old->latest_poll()->has_context(retained.id()));
        EXPECT_TRUE(old->actions(retained).button(fire).held());
    }
    EXPECT_FALSE(retained);
    EXPECT_TRUE(old->latest_poll()->has_context(retained.id())); // Presence does not prove a live lease.
    EXPECT_TRUE(old->actions(retained).button(fire).held());
    EXPECT_THROW(static_cast<void>(endpoint.attach(button())), CE::Exceptions::failed_operation);
    EXPECT_THROW(static_cast<void>(Actions::ContextSubmission{}.attach(button())), CE::Exceptions::failed_operation);
    retained.reset();
}

TEST(action_contexts, validation) {
    Actions::Bindings mapper;
    const auto endpoint = mapper.action_contexts();
    auto definition = button();
    definition.axes = {{{}, stick, fire, {}}};
    EXPECT_THROW(static_cast<void>(endpoint.attach(definition)), CE::Exceptions::invalid_args);
    definition.axes.clear();
    definition.buttons.front().chord.required.clear();
    EXPECT_THROW(static_cast<void>(endpoint.attach(definition)), CE::Exceptions::invalid_args);
    EXPECT_FALSE(mapper.publish_actions()->button(fire).held());
}

TEST(action_contexts, retry) {
    Actions::Bindings mapper;
    const auto baseline = mapper.action_snapshot();
    const auto endpoint = mapper.action_contexts();
    auto earlier = endpoint.attach(button());
    auto definition = button();
    definition.axes = {{{}, stick, move, {}}, {{}, alternate, move, {}}};
    auto context = endpoint.attach(std::move(definition));
    static_cast<void>(mapper.bind_button(first, fire));
    mapper.on_button(first, true);
    mapper.on_axis(stick, std::numeric_limits<float>::max());
    mapper.on_axis(alternate, std::numeric_limits<float>::max());
    mapper.set_device_enabled(4, false);
    EXPECT_THROW(static_cast<void>(mapper.publish_actions()), CE::Exceptions::invalid_args);
    EXPECT_EQ(mapper.action_snapshot(), baseline);
    EXPECT_FALSE(mapper.action_snapshot()->has_context(context.id()));
    EXPECT_FALSE(mapper.action_snapshot()->has_context(earlier.id()));
    EXPECT_TRUE(mapper.action_snapshot()->device_enabled(4));
    mapper.on_axis(alternate, 0);
    const Tick recovered(baseline, {mapper.publish_actions()});
    EXPECT_TRUE(recovered.latest_poll()->has_context(context.id()));
    EXPECT_TRUE(recovered.latest_poll()->has_context(earlier.id()));
    EXPECT_FALSE(recovered.latest_poll()->device_enabled(4));
    EXPECT_TRUE(baseline->device_enabled(4));
    EXPECT_EQ(recovered.button(fire).press_count, 1u);
    EXPECT_EQ(recovered.actions(context).button(fire).press_count, 1u);
    EXPECT_EQ(recovered.actions(earlier).button(fire).press_count, 1u);
}
