#include <backends/miniaudio.h>
#include <internals/exceptions.h>
#include <core/engine/runtime-services-internal.h>
#include "system-internal.h"
#include "wav.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <future>
#include <limits>
#include <vector>

namespace {
    using CE::Audio::PlaybackEndReason;
    using CE::Audio::PlaybackEnded;
    using CE::Audio::PlaybackState;
    using CE::Audio::Miniaudio::System;
    using CE::Audio::Miniaudio::Detail::SystemAccess;

    auto clip() { return CE::Audio::Clip::from_samples({2, 48000}, std::vector<float>(128, 0.25f)); }

    struct Observations {
        CE::Events::EventBus bus;
        std::vector<PlaybackEnded> records;
        const CE::Events::EventBus::Registration registration = bus.register_listener(
            CE::Audio::playback_ended_event, [this](const PlaybackEnded& record) { records.push_back(record); }
        );
    };
}

TEST(audio_events, generations) {
    Observations observed;
    auto output = System::open_offline();
    output->publish_events_to(observed.bus.publisher());
    auto voice = output->play(clip(), {.looping = true});
    const auto first = voice->snapshot();
    EXPECT_NE(first.system, 0);
    EXPECT_NE(first.voice, 0);
    EXPECT_EQ(first.generation, 1);
    EXPECT_FALSE(first.terminal);
    voice->restart();
    auto second = voice->snapshot();
    EXPECT_EQ(second.voice, first.voice);
    EXPECT_EQ(second.generation, 2);
    EXPECT_FALSE(second.terminal);
    voice->stop();
    const auto terminal = voice->snapshot().terminal;
    ASSERT_TRUE(terminal);
    EXPECT_EQ(terminal->reason, PlaybackEndReason::Stopped);
    voice->stop();
    output->close();
    EXPECT_TRUE(observed.records.empty());
    EXPECT_EQ(voice->snapshot().terminal, terminal);
    output->publish_notifications();
    ASSERT_EQ(observed.records.size(), 2);
    EXPECT_EQ(observed.records[0].terminal.reason, PlaybackEndReason::Replaced);
    EXPECT_EQ(observed.records[0].generation, 1);
    EXPECT_EQ(observed.records[1].terminal.reason, PlaybackEndReason::Stopped);
    EXPECT_EQ(observed.records[1].generation, 2);
    EXPECT_EQ(observed.records[1].terminal.sequence, 2);
    EXPECT_EQ(output->notification_snapshot().pending, 0);
}

TEST(audio_events, completion) {
    Observations observed;
    auto output = System::open_offline();
    output->publish_events_to(observed.bus.publisher());
    auto voice = output->play(clip());
    std::array<float, 1024> mixed{};
    output->render(mixed);
    // Pump scans retained voices, without requiring a Voice::snapshot() first.
    output->publish_notifications();
    ASSERT_EQ(observed.records.size(), 1);
    EXPECT_EQ(observed.records[0].terminal.reason, PlaybackEndReason::Completed);
    voice->set_looping(true);
    voice->resume();
    output->render(mixed);
    EXPECT_TRUE(std::ranges::all_of(mixed, [](float value) { return value == 0; }));
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Finished);
    voice->stop();
    output->close();
    EXPECT_EQ(voice->snapshot().terminal->reason, PlaybackEndReason::Completed);
    output->publish_notifications();
    EXPECT_EQ(observed.records.size(), 1);
}

TEST(audio_events, eof_before_stop) {
    Observations observed;
    auto output = System::open_offline();
    output->publish_events_to(observed.bus.publisher());
    auto voice = output->play(clip());
    std::array<float, 1024> mixed{};
    output->render(mixed);
    voice->stop();
    EXPECT_EQ(voice->snapshot().terminal->reason, PlaybackEndReason::Completed);
    voice->restart();
    EXPECT_FALSE(voice->snapshot().terminal);
    output->close();
    output->publish_notifications();
    ASSERT_EQ(observed.records.size(), 2);
    EXPECT_EQ(observed.records[0].terminal.reason, PlaybackEndReason::Completed);
    EXPECT_EQ(observed.records[1].terminal.reason, PlaybackEndReason::OutputClosed);
    EXPECT_EQ(observed.records[1].generation, 2);
}

TEST(audio_events, overflow) {
    Observations observed;
    auto output = System::open_offline({}, {.capacity = 2});
    output->publish_events_to(observed.bus.publisher());
    std::vector<std::shared_ptr<CE::Audio::Voice>> voices;
    for (int index = 0; index < 5; ++index) {
        voices.push_back(output->play(clip(), {.paused = true}));
        voices.back()->stop();
    }
    const auto status = output->notification_snapshot();
    EXPECT_EQ(status.pending, 2);
    EXPECT_EQ(status.sequence, 5);
    EXPECT_EQ(status.lost_observations, 3);
    EXPECT_EQ(status.first_lost_sequence, 1);
    EXPECT_EQ(status.last_lost_sequence, 3);
    EXPECT_EQ(voices.front()->snapshot().terminal->sequence, 1);
    output->publish_notifications();
    ASSERT_EQ(observed.records.size(), 2);
    EXPECT_EQ(observed.records[0].terminal.sequence, 4);
    EXPECT_EQ(observed.records[1].terminal.sequence, 5);
    EXPECT_EQ(observed.records[0].lost_observations, 3);
    EXPECT_EQ(observed.records[0].last_lost_sequence, 3);
}

TEST(audio_events, cutoff) {
    Observations observed;
    auto output = System::open_offline();
    output->publish_events_to(observed.bus.publisher());
    auto first = output->play(clip(), {.paused = true});
    auto next = output->play(clip(), {.paused = true});
    const auto observer = observed.bus.register_listener(CE::Audio::playback_ended_event, [&](const PlaybackEnded&) {
        EXPECT_THROW(output->publish_notifications(), CE::Exceptions::bad_request);
        EXPECT_THROW(output->publish_events_to({}), CE::Exceptions::bad_request);
        next->stop(); // Must not deadlock, recursively publish, or extend entry cutoff.
    });
    first->stop();
    output->publish_notifications();
    EXPECT_EQ(observed.records.size(), 1);
    EXPECT_EQ(output->notification_snapshot().pending, 1);
    output->publish_notifications();
    EXPECT_EQ(observed.records.size(), 2);
    EXPECT_EQ(output->notification_snapshot().pending, 0);
    observer.disconnect();
}

TEST(audio_events, failures) {
    Observations observed;
    auto output = System::open_offline();
    output->publish_events_to(observed.bus.publisher());
    SystemAccess::fail_next_start(*output);
    EXPECT_THROW(static_cast<void>(output->play(clip())), CE::Exceptions::runtime_exception);
    EXPECT_EQ(output->notification_snapshot().sequence, 0);
    auto voice = output->play(clip(), {.looping = true});
    const auto before = voice->snapshot();
    SystemAccess::fail_next_stop(*output);
    EXPECT_THROW(voice->restart(), CE::Exceptions::runtime_exception);
    EXPECT_EQ(voice->snapshot().generation, before.generation);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Playing);
    EXPECT_FALSE(voice->snapshot().terminal);
    SystemAccess::fail_next_start(*output);
    EXPECT_THROW(voice->restart(), CE::Exceptions::runtime_exception);
    EXPECT_EQ(voice->snapshot().generation, before.generation);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Stopped);
    EXPECT_EQ(voice->snapshot().terminal->reason, PlaybackEndReason::Stopped);
    voice->restart();
    EXPECT_EQ(voice->snapshot().generation, before.generation + 1);
    output->publish_notifications();
    ASSERT_EQ(observed.records.size(), 1);
    EXPECT_EQ(observed.records[0].terminal.reason, PlaybackEndReason::Stopped);
}

TEST(audio_events, preparation) {
    AudioFixture::Wav fixture;
    fixture.write({2, 48000}, std::vector<float>(2048, 0.125f));
    const AudioFixture::Wav unavailable;
    const auto previous_directory = std::filesystem::current_path();
    struct Restore {
        std::filesystem::path path;
        ~Restore() { std::error_code error; std::filesystem::current_path(path, error); }
    } restore{previous_directory};
    std::filesystem::current_path(fixture.file().parent_path());
    auto output = System::open_offline();
    auto voice = output->stream(fixture.file().filename(), {.paused = true});
    std::filesystem::current_path(unavailable.file().parent_path());
    EXPECT_THROW(voice->restart(), CE::Exceptions::runtime_exception);
    EXPECT_EQ(voice->snapshot().generation, 1);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Paused);
    EXPECT_FALSE(voice->snapshot().terminal);
    EXPECT_EQ(output->notification_snapshot().sequence, 0);
}

TEST(audio_events, reclamation) {
    Observations observed;
    auto output = System::open_offline();
    output->publish_events_to(observed.bus.publisher());
    static_cast<void>(output->play(clip(), {.paused = true}));
    output->maintain();
    output->publish_notifications();
    ASSERT_EQ(observed.records.size(), 1);
    EXPECT_EQ(observed.records[0].terminal.reason, PlaybackEndReason::Reclaimed);
    output->close();
    output->publish_notifications();
    EXPECT_EQ(observed.records.size(), 1);
}

TEST(audio_events, owner) {
    Observations observed;
    auto output = System::open_offline();
    output->publish_events_to(observed.bus.publisher());
    auto producer = std::async(std::launch::async, [&] {
        EXPECT_THROW(output->publish_notifications(), CE::Exceptions::bad_request);
        EXPECT_THROW(output->publish_events_to({}), CE::Exceptions::bad_request);
        auto voice = output->play(clip(), {.paused = true});
        voice->stop();
        return voice;
    });
    const auto voice = producer.get();
    EXPECT_TRUE(observed.records.empty());
    output->publish_notifications();
    ASSERT_EQ(observed.records.size(), 1);
    EXPECT_EQ(observed.records[0].voice, voice->snapshot().voice);
}

TEST(audio_events, capacity) {
    EXPECT_THROW(static_cast<void>(System::open_offline({}, {.capacity = 0})), CE::Exceptions::invalid_args);
    auto output = System::open_offline();
    SystemAccess::reserve_last_terminal(*output);
    auto voice = output->play(clip(), {.paused = true});
    EXPECT_THROW(static_cast<void>(output->play(clip())), CE::Exceptions::failed_operation);
    EXPECT_THROW(voice->restart(), CE::Exceptions::failed_operation);
    EXPECT_EQ(voice->snapshot().generation, 1);
    EXPECT_FALSE(voice->snapshot().terminal);
    EXPECT_NO_THROW(output->close());
    EXPECT_EQ(voice->snapshot().terminal->sequence, std::numeric_limits<std::uint64_t>::max());
    EXPECT_EQ(voice->snapshot().terminal->reason, PlaybackEndReason::OutputClosed);
}

TEST(audio_events, observation_failure) {
    Observations observed;
    const auto failing = observed.bus.register_listener(CE::Audio::playback_ended_event, [](const PlaybackEnded&) {
        throw std::runtime_error("observer failed");
    });
    auto output = System::open_offline();
    output->publish_events_to(observed.bus.publisher());
    auto voice = output->play(clip());
    voice->stop();
    EXPECT_NO_THROW(output->publish_notifications());
    EXPECT_EQ(observed.records.size(), 1);
    EXPECT_EQ(output->notification_snapshot().pending, 0);
    EXPECT_NO_THROW(output->publish_notifications());
    EXPECT_EQ(observed.records.size(), 1);
    failing.disconnect();
}

TEST(audio_events, service) {
    Observations observed;
    auto services = CE::Runtime::Detail::RuntimeServicesAccess::make(observed.bus);
    CE::Runtime::Detail::RuntimeServicesAccess::begin(services, 1, 2);
    auto output = System::open_offline();
    output->publish_events_to(observed.bus.publisher());
    auto pumping = output->pump_with(services);
    auto voice = output->play(clip());
    voice->stop();
    CE::Runtime::Detail::RuntimeServicesAccess::maintenance(services, CE::Runtime::MaintenanceReason::Running);
    EXPECT_EQ(observed.records.size(), 1);
    voice->restart();
    output->close();
    CE::Runtime::Detail::RuntimeServicesAccess::maintenance(services, CE::Runtime::MaintenanceReason::FinalCollection);
    ASSERT_EQ(observed.records.size(), 2);
    EXPECT_EQ(observed.records.back().terminal.reason, PlaybackEndReason::OutputClosed);
    output.reset();
    EXPECT_TRUE(pumping.attached());
    EXPECT_NO_THROW(CE::Runtime::Detail::RuntimeServicesAccess::maintenance(services, CE::Runtime::MaintenanceReason::FinalCollection));
    EXPECT_EQ(observed.records.size(), 2);
    pumping.reset();
    CE::Runtime::Detail::RuntimeServicesAccess::close(services);
}

TEST(audio_events, destruction) {
    Observations observed;
    std::shared_ptr<CE::Audio::Voice> voice;
    {
        auto output = System::open_offline();
        output->publish_events_to(observed.bus.publisher());
        voice = output->play(clip());
    }
    EXPECT_TRUE(observed.records.empty());
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Closed);
    ASSERT_TRUE(voice->snapshot().terminal);
    EXPECT_EQ(voice->snapshot().terminal->reason, PlaybackEndReason::OutputClosed);
}

TEST(audio_events, callback_destruction) {
    Observations observed;
    auto output = System::open_offline();
    output->publish_events_to(observed.bus.publisher());
    auto first = output->play(clip());
    auto second = output->play(clip());
    first->stop();
    const auto destroying = observed.bus.register_listener(CE::Audio::playback_ended_event, [&](const PlaybackEnded&) {
        output.reset();
    });
    // The pump pins its internal state, without retaining the destroyed facade.
    EXPECT_NO_THROW(output->publish_notifications());
    EXPECT_FALSE(output);
    EXPECT_EQ(observed.records.size(), 1);
    EXPECT_EQ(second->snapshot().terminal->reason, PlaybackEndReason::OutputClosed);
    destroying.disconnect();
}
