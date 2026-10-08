#pragma once

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
    [[nodiscard]] Clip decode_file(const std::filesystem::path& file, DecodeOptions options = {});

    class System final : public CE::Audio::System {
        std::shared_ptr<Detail::SystemState> state_;

        explicit System(std::shared_ptr<Detail::SystemState> state);

    public:
        ~System() override;
        System(const System&) = delete;
        System& operator=(const System&) = delete;

        // Native selection excludes miniaudio's null backend; device failures throw.
        [[nodiscard]] static std::unique_ptr<System> open_device(Format output = {});
        // No device is opened. Only render() advances this system's mixing clock.
        [[nodiscard]] static std::unique_ptr<System> open_offline(Format output = {});

        [[nodiscard]] std::shared_ptr<Voice> play(const Clip& clip, PlaybackOptions options = {}) override;
        [[nodiscard]] std::shared_ptr<Voice> stream(const std::filesystem::path& file, PlaybackOptions options = {}) override;
        void set_volume(float volume) override;
        [[nodiscard]] float volume() const override;
        void maintain() override;
        void close() noexcept override;
        [[nodiscard]] bool closed() const override;

        [[nodiscard]] Format output_format() const;
        [[nodiscard]] std::string backend_name() const;
        // Offline only, interleaved float output with complete frames. Empty spans
        // are permitted. Mixing is serialized with controls and maintenance.
        void render(std::span<float> output);
    };
}
