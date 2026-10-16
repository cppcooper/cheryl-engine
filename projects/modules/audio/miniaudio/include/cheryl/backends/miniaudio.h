#pragma once

#include <cheryl/export.h>

#include <audio/system.h>

#include <cstddef>
#include <span>
#include <string>

namespace CE::Audio::Miniaudio {
    namespace Detail {
        struct SystemState;
    }

    struct DecodeOptions {
        // Whole-clip decode is bounded. Use streamed playback for larger sources.
        std::size_t maximum_bytes = 64 * 1024 * 1024;
    };

    // CPU-only WAV/FLAC/MP3 decode into owned PCM at the source rate/channel count.
    // Rejects unsupported formats/channels/rates, empty data and exceeded budgets.
    [[nodiscard]] CE_AUDIO_MINIAUDIO_API ::CE::Audio::Clip decode_file(const std::filesystem::path& file, DecodeOptions options = {});

    class CE_TYPE_API System final : public ::CE::Audio::System {
        std::shared_ptr<Detail::SystemState> state_;

        CE_AUDIO_MINIAUDIO_API explicit System(std::shared_ptr<Detail::SystemState> state);

    public:
        CE_AUDIO_MINIAUDIO_API ~System() override;
        System(const System&) = delete;
        System& operator=(const System&) = delete;

        // Native selection excludes miniaudio's null backend; device failures throw.
        [[nodiscard]] CE_AUDIO_MINIAUDIO_API static std::unique_ptr<System> open_device(::CE::Audio::Format output = {});
        // No device is opened. Only render() advances this system's mixing clock.
        [[nodiscard]] CE_AUDIO_MINIAUDIO_API static std::unique_ptr<System> open_offline(::CE::Audio::Format output = {});

        [[nodiscard]] CE_AUDIO_MINIAUDIO_API std::shared_ptr<::CE::Audio::Voice>
        play(const ::CE::Audio::Clip& clip, ::CE::Audio::PlaybackOptions options = {}) override;
        [[nodiscard]] CE_AUDIO_MINIAUDIO_API std::shared_ptr<::CE::Audio::Voice>
        stream(const std::filesystem::path& file, ::CE::Audio::PlaybackOptions options = {}) override;
        CE_AUDIO_MINIAUDIO_API void set_volume(float volume) override;
        [[nodiscard]] CE_AUDIO_MINIAUDIO_API float volume() const override;
        CE_AUDIO_MINIAUDIO_API void maintain() override;
        CE_AUDIO_MINIAUDIO_API void close() noexcept override;
        [[nodiscard]] CE_AUDIO_MINIAUDIO_API bool closed() const override;

        [[nodiscard]] CE_AUDIO_MINIAUDIO_API ::CE::Audio::Format output_format() const;
        [[nodiscard]] CE_AUDIO_MINIAUDIO_API std::string backend_name() const;
        // Offline only, interleaved float output with complete frames. Empty spans
        // are permitted. Mixing is serialized with controls and maintenance.
        CE_AUDIO_MINIAUDIO_API void render(std::span<float> output);
    };
}
