#include <audio.h>
#include <internals/exceptions.h>
#include <gtest/gtest.h>

#include <limits>
#include <utility>
#include <vector>

TEST(audio_clip, owned) {
    std::vector<float> source{0.25f, -0.5f, 0.75f, -1.0f};
    const auto clip = CE::Audio::Clip::from_samples({2, 48000}, source);
    source.assign(4, 0);
    EXPECT_EQ(clip.format(), (CE::Audio::Format{2, 48000}));
    EXPECT_EQ(clip.frame_count(), 2);
    EXPECT_DOUBLE_EQ(clip.duration().count(), 2.0 / 48000);
    EXPECT_FLOAT_EQ(clip.samples()[0], 0.25f);
    EXPECT_FLOAT_EQ(clip.samples()[3], -1.0f);
    auto copy = clip;
    EXPECT_EQ(copy.samples().data(), clip.samples().data());
    const auto moved = std::move(copy);
    EXPECT_EQ(moved.samples().data(), copy.samples().data());
}

TEST(audio_clip, mono) {
    const auto clip = CE::Audio::Clip::from_samples({1, 8000}, std::vector<float>(8000, 0.5f));
    EXPECT_EQ(clip.frame_count(), 8000);
    EXPECT_DOUBLE_EQ(clip.duration().count(), 1);
    // Float PCM is not implicitly clamped during storage.
    const auto headroom = CE::Audio::Clip::from_samples({1, 192000}, {1.25f});
    EXPECT_FLOAT_EQ(headroom.samples()[0], 1.25f);
}

TEST(audio_clip, invalid) {
    using CE::Audio::Clip;
    using CE::Exceptions::invalid_args;
    for (const CE::Audio::Format format : {CE::Audio::Format{0, 48000}, {3, 48000}, {1, 0}, {1, 7999}, {2, 192001}})
        EXPECT_THROW(static_cast<void>(Clip::from_samples(format, {0, 0})), invalid_args);
    EXPECT_THROW(static_cast<void>(Clip::from_samples({1, 48000}, {})), invalid_args);
    EXPECT_THROW(static_cast<void>(Clip::from_samples({2, 48000}, {0})), invalid_args);
    for (const float sample : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::quiet_NaN()})
        EXPECT_THROW(static_cast<void>(Clip::from_samples({1, 48000}, {sample})), invalid_args);
}

TEST(audio_clip, volume) {
    for (const float volume : {0.0f, 0.5f, 1.0f})
        EXPECT_NO_THROW((CE::Audio::PlaybackOptions{volume, true, true}).validate());
    for (const float volume : {-0.01f, 1.01f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        EXPECT_THROW((CE::Audio::PlaybackOptions{volume}).validate(), CE::Exceptions::invalid_args);
        EXPECT_THROW(CE::Audio::validate_volume(volume), CE::Exceptions::invalid_args);
    }
}
