#include <core/controls/input-interface.h>
#include <internals/exceptions.h>
#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <vector>

namespace {
    using Change = CE::Input::GamepadAvailabilityChanged;
    using Clock = CE::Input::ObservationClock;

    class SampleInput final : public CE::Input::System {
        CE::Input::Actions::Bindings bindings_;

    public:
        void open() { open_gamepad_availability(); }
        void initialize(CE::Platform::Window&) override { open(); }
        void deinitialize() override { discard_captured_input(); }
        void poll() override { sample(false, Clock::now()); }
        void sample(bool available, Clock::time_point time) {
            begin_input_poll();
            stage_gamepad_availability(gamepad_id(), available);
            static_cast<void>(publish_input(time));
        }
        CE::Input::Actions::Bindings& bindings() override { return bindings_; }
        CE::Input::DeviceId keyboard_id() const override { return 1; }
        CE::Input::DeviceId mouse_id() const override { return 2; }
        CE::Input::DeviceId gamepad_id() const override { return 3; }
    };
}

TEST(gamepad_availability, committed_poll) {
    CE::Events::EventBus bus;
    SampleInput input;
    input.publish_events_to(bus.publisher());
    input.open();
    std::vector<Change> changes;
    const auto id = bus.register_listener(CE::Input::gamepad_availability_changed_event, [&](const Change& change) {
        changes.push_back(change);
        const auto poll = input.poll_snapshot();
        EXPECT_EQ(change.current.poll, poll->state->poll());
        EXPECT_EQ(change.current.observed_at, poll->state->observed_at());
    });
    const auto seed = input.gamepad_availability();
    EXPECT_NE(seed.source, 0u);
    EXPECT_NE(seed.slot, 0u);
    EXPECT_FALSE(seed.known);
    EXPECT_FALSE(seed.closed);
    const auto now = Clock::now();
    input.sample(true, now);
    input.sample(true, now + std::chrono::seconds{1});
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_TRUE(changes[0].current.available);
    EXPECT_TRUE(changes[0].current.known);
    EXPECT_EQ(changes[0].current.device, 3u);
    EXPECT_EQ(changes[0].current.epoch, 1u);
    EXPECT_EQ(changes[0].current.revision, 1u);
    EXPECT_FALSE(changes[0].previous.known);
    EXPECT_TRUE(bus.unregister_and_wait(id));
}

TEST(gamepad_availability, failed_poll) {
    CE::Events::EventBus bus;
    SampleInput input;
    input.publish_events_to(bus.publisher());
    input.open();
    int changes = 0;
    const auto id = bus.register_listener(CE::Input::gamepad_availability_changed_event, [&](const Change&) { ++changes; });
    const auto now = Clock::now();
    input.sample(false, now);
    const auto complete = input.poll_snapshot();
    EXPECT_THROW(input.sample(true, now - std::chrono::seconds{1}), CE::Exceptions::invalid_args);
    EXPECT_EQ(changes, 1);
    EXPECT_FALSE(input.gamepad_availability().available);
    EXPECT_EQ(input.poll_snapshot(), complete);
    input.sample(true, now + std::chrono::seconds{1});
    EXPECT_EQ(changes, 2);
    EXPECT_TRUE(input.gamepad_availability().available);
    EXPECT_TRUE(bus.unregister_and_wait(id));
}

TEST(gamepad_availability, reconnect_and_close) {
    CE::Events::EventBus bus;
    SampleInput input;
    input.publish_events_to(bus.publisher());
    input.open();
    const auto initial = input.gamepad_availability();
    std::vector<Change> changes;
    const auto id =
        bus.register_listener(CE::Input::gamepad_availability_changed_event, [&](const Change& change) { changes.push_back(change); });
    const auto now = Clock::now();
    input.sample(true, now);
    input.sample(false, now + std::chrono::seconds{1});
    input.sample(true, now + std::chrono::seconds{2});
    input.deinitialize();
    input.deinitialize();
    ASSERT_EQ(changes.size(), 4u);
    EXPECT_EQ(changes[2].current.epoch, 2u);
    EXPECT_TRUE(changes[3].current.closed);
    EXPECT_TRUE(changes[3].current.available);
    EXPECT_EQ(changes[3].current.poll, changes[2].current.poll);
    input.open();
    const auto next = input.gamepad_availability();
    EXPECT_NE(next.source, initial.source);
    EXPECT_EQ(next.slot, initial.slot);
    EXPECT_EQ(next.epoch, 2u);
    EXPECT_FALSE(next.known);
    input.sample(true, now + std::chrono::seconds{3});
    EXPECT_EQ(changes.back().current.epoch, 3u);
    EXPECT_TRUE(bus.unregister_and_wait(id));
}

TEST(gamepad_availability, isolation) {
    CE::Events::EventBus bus;
    SampleInput input;
    input.publish_events_to(bus.publisher());
    input.open();
    bus.register_listener(CE::Input::gamepad_availability_changed_event, [](const Change&) { throw std::runtime_error("observer"); });
    int calls = 0;
    const auto id = bus.register_listener(CE::Input::gamepad_availability_changed_event, [&](const Change&) { ++calls; });
    EXPECT_NO_THROW(input.sample(true, Clock::now()));
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(input.gamepad_availability().available);
    EXPECT_TRUE(bus.unregister_and_wait(id));
}

TEST(gamepad_availability, reentrant_order) {
    CE::Events::EventBus bus;
    SampleInput input;
    input.publish_events_to(bus.publisher());
    input.open();
    const auto now = Clock::now();
    bus.register_listener(CE::Input::gamepad_availability_changed_event, [&](const Change& change) {
        if (change.current.revision == 1)
            input.sample(false, now + std::chrono::seconds{1});
    });
    std::vector<std::uint64_t> revisions;
    const auto id = bus.register_listener(CE::Input::gamepad_availability_changed_event, [&](const Change& change) {
        revisions.push_back(change.current.revision);
    });
    input.sample(true, now);
    EXPECT_EQ(revisions, (std::vector<std::uint64_t>{1}));
    EXPECT_EQ(input.gamepad_availability().revision, 2u);
    input.sample(false, now + std::chrono::seconds{2});
    EXPECT_EQ(revisions, (std::vector<std::uint64_t>{1, 2}));
    EXPECT_TRUE(bus.unregister_and_wait(id));
}

TEST(gamepad_availability, reentrant_close) {
    CE::Events::EventBus bus;
    SampleInput input;
    input.publish_events_to(bus.publisher());
    input.open();
    bus.register_listener(CE::Input::gamepad_availability_changed_event, [&](const Change& change) {
        if (!change.current.closed)
            input.deinitialize();
    });
    std::vector<bool> closed;
    const auto id = bus.register_listener(CE::Input::gamepad_availability_changed_event, [&](const Change& change) {
        closed.push_back(change.current.closed);
    });
    input.sample(true, Clock::now());
    EXPECT_EQ(closed, (std::vector<bool>{false, true}));
    EXPECT_TRUE(bus.unregister_and_wait(id));
}
