#include <gtest/gtest.h>

#include <core/controls/input-accumulator.h>
#include <core/controls/input-bindings.h>
#include <core/controls/input-capture.h>
#include <internals/exceptions.h>

#include <utility>

namespace {
    constexpr CE::Input::DeviceId keyboard = 1;
    constexpr CE::Input::ControlId key = 65;
    using CE::Input::DeviceKind;
    using CE::Input::Capture::Channel;
    using CE::Input::Events::Button;
    using CE::Input::Events::ButtonPhase;
    using CE::Input::Events::Text;
}

TEST(input_capture, key_text_order) {
    CE::Input::Capture::Collector capture;
    auto events = capture.request(CE::Input::Capture::Channel::Events);
    auto text = capture.request(CE::Input::Capture::Channel::Text);
    capture.begin_poll();
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Button{key, CE::Input::Events::ButtonPhase::Press});
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Text{U'\u00e9'});
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Button{key, CE::Input::Events::ButtonPhase::Repeat});
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Text{U'\u00e9'});
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Button{key, CE::Input::Events::ButtonPhase::Release});
    const auto records = capture.complete();
    ASSERT_EQ(records.size(), 5u);
    EXPECT_EQ(std::get<CE::Input::Events::Button>(records[0].data).phase, CE::Input::Events::ButtonPhase::Press);
    EXPECT_EQ(std::get<CE::Input::Events::Text>(records[1].data).codepoint, U'\u00e9');
    EXPECT_EQ(std::get<CE::Input::Events::Button>(records[2].data).phase, CE::Input::Events::ButtonPhase::Repeat);
    EXPECT_EQ(std::get<CE::Input::Events::Text>(records[3].data).codepoint, U'\u00e9');
    EXPECT_EQ(std::get<CE::Input::Events::Button>(records[4].data).phase, CE::Input::Events::ButtonPhase::Release);
    for (std::size_t i = 1; i < records.size(); ++i)
        EXPECT_LT(records[i - 1].sequence, records[i].sequence);
}

TEST(input_capture, channel_activation) {
    CE::Input::Capture::Collector capture;
    auto text = capture.request(CE::Input::Capture::Channel::Text);
    capture.begin_poll();
    auto events = capture.request(CE::Input::Capture::Channel::Events);
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Button{key, CE::Input::Events::ButtonPhase::Press});
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Text{U'a'});
    const auto first = capture.complete();
    ASSERT_EQ(first.size(), 1u);
    EXPECT_TRUE(first[0].is_text());

    capture.begin_poll();
    text.reset();
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Text{U'b'});
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Button{key, CE::Input::Events::ButtonPhase::Repeat});
    EXPECT_EQ(capture.complete().size(), 2u); // This poll retained its activation snapshot.
    capture.begin_poll();
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Text{U'c'});
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Button{key, CE::Input::Events::ButtonPhase::Release});
    const auto last = capture.complete();
    ASSERT_EQ(last.size(), 1u);
    EXPECT_FALSE(last[0].is_text());
}

TEST(input_capture, independent_capture_requests) {
    CE::Input::Capture::Collector capture;
    auto first = capture.request(CE::Input::Capture::Channel::Events);
    auto second = capture.request(CE::Input::Capture::Channel::Events);
    auto moved = std::move(first);
    moved.reset();
    capture.begin_poll();
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Button{key, CE::Input::Events::ButtonPhase::Press});
    EXPECT_EQ(capture.complete().size(), 1u);
    second.reset();
    capture.begin_poll();
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Button{key, CE::Input::Events::ButtonPhase::Release});
    EXPECT_TRUE(capture.complete().empty());
}

TEST(input_capture, poll_record_consumption) {
    using namespace std::chrono_literals;
    CE::Input::Actions::Bindings bindings;
    const CE::Input::Actions::Id action{1};
    (void)bindings.bind_button({keyboard, key}, action);
    const auto start = bindings.action_snapshot()->observed_at();
    CE::Input::Accumulator accumulator(bindings.action_snapshot(), start);
    CE::Input::Capture::Collector capture;
    auto events = capture.request(CE::Input::Capture::Channel::Events);
    auto text = capture.request(CE::Input::Capture::Channel::Text);
    capture.begin_poll();
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Button{key, CE::Input::Events::ButtonPhase::Press}, start + 1ms);
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Text{U'x'}, start + 2ms);
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Button{key, CE::Input::Events::ButtonPhase::Release}, start + 3ms);
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

TEST(input_capture, unicode_scalars) {
    CE::Input::Capture::Collector capture;
    auto text = capture.request(CE::Input::Capture::Channel::Text);
    capture.begin_poll();
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Text{U'\U0001f642'});
    EXPECT_THROW(capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Text{0xD800}), CE::Exceptions::invalid_args);
    EXPECT_THROW(capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Text{0x110000}), CE::Exceptions::invalid_args);
    EXPECT_EQ(capture.complete().size(), 1u);
}

TEST(input_capture, portable_buttons) {
    using CE::Input::KeyboardKey;
    using CE::Input::Modifiers;
    using CE::Input::MouseButton;
    const CE::Input::Events::Button legacy{key, CE::Input::Events::ButtonPhase::Press};
    EXPECT_EQ(legacy.key, KeyboardKey::Unknown);
    EXPECT_EQ(legacy.mouse_button, MouseButton::Unknown);
    EXPECT_FALSE(legacy.position);

    CE::Input::Actions::Bindings bindings;
    CE::Input::Accumulator accumulator(bindings.action_snapshot(), bindings.action_snapshot()->observed_at());
    CE::Input::Routing::Manager routing;
    auto focus = routing.focus(7);
    CE::Input::Capture::Collector capture;
    auto events = capture.request(CE::Input::Capture::Channel::Events);
    auto text = capture.request(CE::Input::Capture::Channel::Text);
    capture.begin_poll(*routing.current());
    // Opaque/native tokens deliberately differ from any familiar dependency's
    // constants. Portable identity and committed text survive independently.
    capture.record(
        keyboard, DeviceKind::Keyboard,
        CE::Input::Events::Button{700, CE::Input::Events::ButtonPhase::Repeat, Modifiers::Control, 900, 1200, KeyboardKey::A}
    );
    capture.record(keyboard, DeviceKind::Keyboard, CE::Input::Events::Text{U'\u00e9'});
    capture.record(
        2, DeviceKind::Mouse,
        CE::Input::Events::Button{
            800, CE::Input::Events::ButtonPhase::Press, Modifiers::None, 901, -1, KeyboardKey::Unknown, MouseButton::Left,
            CE::Input::Events::Pointer{12.5, 24.5}
        }
    );
    capture.record(2, DeviceKind::Mouse, CE::Input::Events::Scroll{0.5, -0.25, CE::Input::Events::Pointer{13.0, 25.0}});
    capture.record(2, DeviceKind::Mouse, CE::Input::Events::Pointer{100.0, 200.0});
    auto poll = std::make_shared<CE::Input::PollSnapshot>();
    poll->state = bindings.publish_actions();
    poll->records = capture.complete();
    const auto tick = accumulator.consume_polls(CE::Input::ObservationClock::now(), {poll});
    ASSERT_EQ(tick.records().size(), 5u);
    const auto& key_record = tick.records()[0];
    const auto& button = std::get<CE::Input::Events::Button>(key_record.data);
    EXPECT_EQ(button.key, KeyboardKey::A);
    EXPECT_EQ(button.button, 700u);
    EXPECT_EQ(button.phase, CE::Input::Events::ButtonPhase::Repeat);
    EXPECT_EQ(button.modifiers, Modifiers::Control);
    EXPECT_EQ(button.native_code, 900);
    EXPECT_EQ(button.scancode, 1200);
    EXPECT_EQ(button.mouse_button, MouseButton::Unknown);
    EXPECT_EQ(key_record.target, focus.target());
    EXPECT_EQ(key_record.focus_epoch, focus.epoch());
    EXPECT_FALSE(key_record.to_gameplay);
    EXPECT_EQ(std::get<CE::Input::Events::Text>(tick.records()[1].data).codepoint, U'\u00e9');
    const auto& mouse_record = tick.records()[2];
    EXPECT_EQ(std::get<CE::Input::Events::Button>(mouse_record.data).mouse_button, MouseButton::Left);
    const auto& click_position = std::get<CE::Input::Events::Button>(mouse_record.data).position;
    ASSERT_TRUE(click_position);
    EXPECT_DOUBLE_EQ(click_position->x, 12.5);
    EXPECT_DOUBLE_EQ(click_position->y, 24.5);
    EXPECT_EQ(mouse_record.target, 0u);
    EXPECT_TRUE(mouse_record.to_gameplay);
    EXPECT_LT(key_record.sequence, tick.records()[1].sequence);
    EXPECT_LT(tick.records()[1].sequence, mouse_record.sequence);
    const auto& scroll = std::get<CE::Input::Events::Scroll>(tick.records()[3].data);
    EXPECT_DOUBLE_EQ(scroll.x, 0.5);
    EXPECT_DOUBLE_EQ(scroll.y, -0.25);
    ASSERT_TRUE(scroll.position);
    EXPECT_DOUBLE_EQ(scroll.position->x, 13.0);
    EXPECT_DOUBLE_EQ(scroll.position->y, 25.0);
    EXPECT_DOUBLE_EQ(std::get<CE::Input::Events::Pointer>(tick.records()[4].data).x, 100.0);
    EXPECT_EQ(poll->records.size(), 5u); // Other readers retain the same complete poll.
}
