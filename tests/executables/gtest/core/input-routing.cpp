#include <gtest/gtest.h>

#include <core/controls/input-interface.h>
#include <internals/exceptions.h>

#include <functional>
#include <utility>

namespace {
    class RoutedInput final : public CE::Input::iInputSystem {
        CE::Input::InputBindings bindings_;

    public:
        std::function<void()> collect;
        void initialize(CE::iWindow&) override {}
        void deinitialize() override {
            bindings_.clear();
            discard_captured_input();
        }
        void poll() override {
            begin_input_poll();
            if (collect)
                collect();
            (void)publish_input();
        }
        void key(CE::Input::ButtonPhase phase) {
            capture_buffer().record(keyboard_id(), CE::Input::DeviceKind::Keyboard, CE::Input::ButtonEvent{65, phase});
            if (phase != CE::Input::ButtonPhase::Repeat)
                bindings_.on_button({keyboard_id(), 65}, phase == CE::Input::ButtonPhase::Press);
        }
        void text(char32_t codepoint) {
            capture_buffer().record(keyboard_id(), CE::Input::DeviceKind::Keyboard, CE::Input::TextEvent{codepoint});
        }
        void pad(bool held) { bindings_.on_button({gamepad_id(), 65}, held); }
        [[nodiscard]] CE::Input::InputBindings& bindings() override { return bindings_; }
        [[nodiscard]] CE::Input::DeviceId keyboard_id() const override { return 1; }
        [[nodiscard]] CE::Input::DeviceId mouse_id() const override { return 2; }
        [[nodiscard]] CE::Input::DeviceId gamepad_id() const override { return 3; }
        [[nodiscard]] bool supports(CE::Input::InputMode) const override { return true; }
    };
}

TEST(input_routing, an_old_lease_cannot_clear_a_new_owner_even_with_the_same_target_id) {
    CE::Input::InputRouting routing;
    auto first = routing.focus(1);
    const auto epoch = first.epoch();
    auto second = routing.focus(1);
    EXPECT_FALSE(first.owns_focus());
    EXPECT_TRUE(second.owns_focus());
    EXPECT_NE(epoch, second.epoch());
    first.reset();
    EXPECT_TRUE(second.owns_focus());
    auto moved = std::move(second);
    EXPECT_TRUE(moved.owns_focus());
    moved.reset();
    EXPECT_EQ(routing.current()->target, 0u);
}

TEST(input_routing, exclusive_text_focus_suppresses_keyboard_state_but_keeps_the_pad) {
    RoutedInput input;
    constexpr CE::Input::ActionId typing_key{1};
    constexpr CE::Input::ActionId shared_action{2};
    (void)input.bindings().bind_button({1, 65}, typing_key);
    (void)input.bindings().bind_button({1, 65}, shared_action);
    (void)input.bindings().bind_button({3, 65}, shared_action);
    auto events = input.capture(CE::Input::InputMode::Events);
    auto text = input.capture(CE::Input::InputMode::Text);
    auto focus = input.routing().focus(42);
    input.collect = [&] {
        input.key(CE::Input::ButtonPhase::Press);
        input.text(U'a');
        input.pad(true);
    };
    input.poll();
    const auto complete = input.poll_snapshot();
    EXPECT_FALSE(complete->state->button(typing_key).held());
    EXPECT_TRUE(complete->state->button(shared_action).held());
    ASSERT_EQ(complete->records.size(), 2u);
    for (const auto& record : complete->records) {
        EXPECT_EQ(record.target, 42u);
        EXPECT_EQ(record.focus_epoch, focus.epoch());
        EXPECT_FALSE(record.to_gameplay);
    }

    // The physical key remains tracked while blocked. Releasing focus resumes
    // its held State at the next poll, rather than requiring a fresh hardware press.
    focus.reset();
    input.collect = {};
    input.poll();
    EXPECT_TRUE(input.action_snapshot()->button(typing_key).pressed());
    EXPECT_TRUE(input.action_snapshot()->button(shared_action).held());
}

TEST(input_routing, pending_records_keep_the_old_owner_when_focus_changes_during_collection) {
    RoutedInput input;
    auto events = input.capture(CE::Input::InputMode::Events);
    auto text = input.capture(CE::Input::InputMode::Text);
    auto first = input.routing().focus(10);
    CE::Input::FocusLease second;
    input.collect = [&] {
        input.text(U'a');
        second = input.routing().focus(20);
        input.text(U'b');
    };
    input.poll();
    const auto earlier = input.poll_snapshot();
    ASSERT_EQ(earlier->records.size(), 2u);
    EXPECT_EQ(earlier->records[0].target, 10u);
    EXPECT_EQ(earlier->records[1].target, 10u);
    input.collect = [&] { input.text(U'c'); };
    input.poll();
    EXPECT_EQ(input.poll_snapshot()->records[0].target, 20u);
    EXPECT_EQ(earlier->records[0].target, 10u); // An immutable queued poll is never retargeted.
}

TEST(input_routing, pass_through_delivers_controls_to_ui_and_gameplay_without_activating_text_capture) {
    RoutedInput input;
    constexpr CE::Input::ActionId action{1};
    (void)input.bindings().bind_button({1, 65}, action);
    auto events = input.capture(CE::Input::InputMode::Events);
    auto focus = input.routing().focus(7, CE::Input::KeyboardRouting::PassThrough);
    input.collect = [&] {
        input.key(CE::Input::ButtonPhase::Press);
        input.text(U'a');
    };
    input.poll();
    const auto poll = input.poll_snapshot();
    EXPECT_TRUE(poll->state->button(action).pressed());
    ASSERT_EQ(poll->records.size(), 1u);
    EXPECT_EQ(poll->records[0].target, 7u);
    EXPECT_TRUE(poll->records[0].to_gameplay);
}

TEST(input_routing, deinitialization_clears_focus_and_pending_records_without_changing_published_handles) {
    RoutedInput input;
    auto text = input.capture(CE::Input::InputMode::Text);
    auto focus = input.routing().focus(1);
    input.collect = [&] { input.text(U'a'); };
    input.poll();
    const auto saved = input.poll_snapshot();
    input.deinitialize();
    EXPECT_FALSE(focus.owns_focus());
    EXPECT_EQ(input.routing().current()->target, 0u);
    ASSERT_EQ(saved->records.size(), 1u);
    EXPECT_EQ(saved->records[0].target, 1u);
}
