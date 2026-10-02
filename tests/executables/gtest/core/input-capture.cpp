#include <gtest/gtest.h>

#include <core/controls/input-accumulator.h>
#include <core/controls/input-bindings.h>
#include <core/controls/input-capture.h>
#include <internals/exceptions.h>

#include <utility>

namespace {
    constexpr CE::Input::DeviceId keyboard = 1;
    constexpr CE::Input::DeviceButtonId key = 65;
    using CE::Input::ButtonEvent;
    using CE::Input::ButtonPhase;
    using CE::Input::DeviceKind;
    using CE::Input::InputMode;
    using CE::Input::TextEvent;
}

TEST(input_capture, ordered_keys_repeats_and_text_keep_their_shared_order) {
    CE::Input::InputCapture capture;
    auto events = capture.request(InputMode::Events);
    auto text = capture.request(InputMode::Text);
    capture.begin_poll();
    capture.record(keyboard, DeviceKind::Keyboard, ButtonEvent{key, ButtonPhase::Press});
    capture.record(keyboard, DeviceKind::Keyboard, TextEvent{U'\u00e9'});
    capture.record(keyboard, DeviceKind::Keyboard, ButtonEvent{key, ButtonPhase::Repeat});
    capture.record(keyboard, DeviceKind::Keyboard, TextEvent{U'\u00e9'});
    capture.record(keyboard, DeviceKind::Keyboard, ButtonEvent{key, ButtonPhase::Release});
    const auto records = capture.complete();
    ASSERT_EQ(records.size(), 5u);
    EXPECT_EQ(std::get<ButtonEvent>(records[0].data).phase, ButtonPhase::Press);
    EXPECT_EQ(std::get<TextEvent>(records[1].data).codepoint, U'\u00e9');
    EXPECT_EQ(std::get<ButtonEvent>(records[2].data).phase, ButtonPhase::Repeat);
    EXPECT_EQ(std::get<TextEvent>(records[3].data).codepoint, U'\u00e9');
    EXPECT_EQ(std::get<ButtonEvent>(records[4].data).phase, ButtonPhase::Release);
    for (std::size_t i = 1; i < records.size(); ++i)
        EXPECT_LT(records[i - 1].sequence, records[i].sequence);
}

TEST(input_capture, channel_requests_are_independent_and_take_effect_at_the_next_poll) {
    CE::Input::InputCapture capture;
    auto text = capture.request(InputMode::Text);
    capture.begin_poll();
    auto events = capture.request(InputMode::Events);
    capture.record(keyboard, DeviceKind::Keyboard, ButtonEvent{key, ButtonPhase::Press});
    capture.record(keyboard, DeviceKind::Keyboard, TextEvent{U'a'});
    const auto first = capture.complete();
    ASSERT_EQ(first.size(), 1u);
    EXPECT_TRUE(first[0].is_text());

    capture.begin_poll();
    text.reset();
    capture.record(keyboard, DeviceKind::Keyboard, TextEvent{U'b'});
    capture.record(keyboard, DeviceKind::Keyboard, ButtonEvent{key, ButtonPhase::Repeat});
    EXPECT_EQ(capture.complete().size(), 2u); // This poll retained its activation snapshot.
    capture.begin_poll();
    capture.record(keyboard, DeviceKind::Keyboard, TextEvent{U'c'});
    capture.record(keyboard, DeviceKind::Keyboard, ButtonEvent{key, ButtonPhase::Release});
    const auto last = capture.complete();
    ASSERT_EQ(last.size(), 1u);
    EXPECT_FALSE(last[0].is_text());
}

TEST(input_capture, moving_and_releasing_one_request_cannot_disable_another) {
    CE::Input::InputCapture capture;
    auto first = capture.request(InputMode::Events);
    auto second = capture.request(InputMode::Events);
    auto moved = std::move(first);
    moved.reset();
    capture.begin_poll();
    capture.record(keyboard, DeviceKind::Keyboard, ButtonEvent{key, ButtonPhase::Press});
    EXPECT_EQ(capture.complete().size(), 1u);
    second.reset();
    capture.begin_poll();
    capture.record(keyboard, DeviceKind::Keyboard, ButtonEvent{key, ButtonPhase::Release});
    EXPECT_TRUE(capture.complete().empty());
}

TEST(input_capture, complete_poll_records_survive_state_aggregation_and_are_consumed_once) {
    using namespace std::chrono_literals;
    CE::Input::InputBindings bindings;
    const CE::Input::ActionId action{1};
    (void)bindings.bind_button({keyboard, key}, action);
    const auto start = bindings.action_snapshot()->observed_at();
    CE::Input::InputAccumulator accumulator(bindings.action_snapshot(), start);
    CE::Input::InputCapture capture;
    auto events = capture.request(InputMode::Events);
    auto text = capture.request(InputMode::Text);
    capture.begin_poll();
    capture.record(keyboard, DeviceKind::Keyboard, ButtonEvent{key, ButtonPhase::Press}, start + 1ms);
    capture.record(keyboard, DeviceKind::Keyboard, TextEvent{U'x'}, start + 2ms);
    capture.record(keyboard, DeviceKind::Keyboard, ButtonEvent{key, ButtonPhase::Release}, start + 3ms);
    bindings.on_button({keyboard, key}, true);
    bindings.on_button({keyboard, key}, false);
    auto poll = std::make_shared<CE::Input::PollSnapshot>();
    poll->state = bindings.publish_actions(start + 5ms);
    poll->records = capture.complete();
    const auto input = accumulator.consume_polls(start + 10ms, {poll});
    EXPECT_TRUE(input.button(action).pressed());
    EXPECT_TRUE(input.button(action).released());
    ASSERT_EQ(input.records().size(), 3u);
    const auto first_reader = input.records();
    const auto second_reader = input.records();
    EXPECT_EQ(first_reader.data(), second_reader.data());
    EXPECT_EQ(poll->records.size(), 3u); // Publication and other readers remain intact.
    const auto next = accumulator.consume_polls(start + 20ms, {});
    EXPECT_TRUE(next.records().empty());
    EXPECT_FALSE(next.button(action).pressed());
}

TEST(input_capture, text_accepts_supplementary_scalars_and_rejects_surrogates) {
    CE::Input::InputCapture capture;
    auto text = capture.request(InputMode::Text);
    capture.begin_poll();
    capture.record(keyboard, DeviceKind::Keyboard, TextEvent{U'\U0001f642'});
    EXPECT_THROW(capture.record(keyboard, DeviceKind::Keyboard, TextEvent{0xD800}), CE::Exceptions::invalid_args);
    EXPECT_THROW(capture.record(keyboard, DeviceKind::Keyboard, TextEvent{0x110000}), CE::Exceptions::invalid_args);
    EXPECT_EQ(capture.complete().size(), 1u);
}
