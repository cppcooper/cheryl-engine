#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace CE::Audio {
    struct Format {
        std::uint32_t channels = 2;
        std::uint32_t sample_rate = 48000;

        // Initial scope: mono/stereo, 8–192 kHz. Throws for unsupported values.
        void validate() const;
        bool operator==(const Format&) const = default;
    };

    /** Immutable, owned interleaved float PCM. Frames contain one sample per channel.
     * Samples must be finite; [-1, 1] is the nominal range, without implicit clamping.
     * Copies share storage. Moving also preserves the source's valid snapshot.
     * Neither construction nor access requires an audio system or native device.
     */
    class Clip final {
        struct Storage;
        std::shared_ptr<const Storage> storage_;

        explicit Clip(std::shared_ptr<const Storage> storage);

    public:
        Clip(const Clip&) = default;
        Clip& operator=(const Clip&) = default;

        // Rejects empty data, incomplete frames and non-finite samples.
        [[nodiscard]] static Clip from_samples(Format format, std::vector<float> samples);
        [[nodiscard]] Format format() const noexcept;
        [[nodiscard]] std::uint64_t frame_count() const noexcept;
        [[nodiscard]] std::chrono::duration<double> duration() const noexcept;
        [[nodiscard]] std::span<const float> samples() const noexcept;
    };
}
