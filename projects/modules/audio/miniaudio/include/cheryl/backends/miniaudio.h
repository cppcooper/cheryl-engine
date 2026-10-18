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

    /** Bound whole-file decoded PCM memory; streaming is a separate playback path. */
    struct DecodeOptions {
        // Whole-clip decode is bounded. Use streamed playback for larger sources.
        std::size_t maximum_bytes = 64 * 1024 * 1024; ///< Maximum decoded float-PCM bytes, not compressed source-file bytes; at least sizeof(float).
    };

    // CPU-only WAV/FLAC/MP3 decode into owned PCM at the source rate/channel count.
    // Rejects unsupported formats/channels/rates, empty data and exceeded budgets.
    [[nodiscard]] CE_AUDIO_MINIAUDIO_API ::CE::Audio::Clip decode_file(const std::filesystem::path& file, DecodeOptions options = {});

    /** Native-device or explicit offline miniaudio output, implementing the neutral synchronized voice/system contracts.
     * @see [Miniaudio integration](cheryl-guide:modules/miniaudio)
     */
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

/** @fn CE::Audio::Clip CE::Audio::Miniaudio::decode_file(const std::filesystem::path& file, DecodeOptions options)
 * Decode WAV/FLAC/MP3 on CPU into owned channel-interleaved float PCM at the source rate/channel count. Reject empty/NUL paths, unsupported formats/channels/rates, empty data and a decoded budget overflow. File/decode/allocation failures throw; no device is opened.
 * @param options Maximum decoded PCM byte budget, default 64MiB.
 */

/** @fn CE::Audio::Miniaudio::System::~System()
 * Close output after external producers stop using this object; surviving Voice handles remain safe.
 */

/** @fn std::unique_ptr<CE::Audio::Miniaudio::System> CE::Audio::Miniaudio::System::open_device(Format output)
 * Open native output at a validated mono/stereo 8..192kHz format. Excludes miniaudio's null backend; device/native/allocation failures throw. Playback advances independently on the native clock.
 */

/** @fn std::unique_ptr<CE::Audio::Miniaudio::System> CE::Audio::Miniaudio::System::open_offline(Format output)
 * Create output without opening a device. Only render() advances the mixing clock; validate output as for open_device().
 */

/** @fn std::shared_ptr<CE::Audio::Voice> CE::Audio::Miniaudio::System::play(const Clip& clip, PlaybackOptions options)
 * Validate options, convert/retain PCM and publish a voice under the system lock. Reclaims unreferenced inactive voices first. Closed/native/allocation failures throw; independent voices may share source PCM.
 */

/** @fn std::shared_ptr<CE::Audio::Voice> CE::Audio::Miniaudio::System::stream(const std::filesystem::path& file, PlaybackOptions options)
 * Validate path/options and synchronously open streamed playback under the system lock. Keep the file available for playback/restart. Closed/native/allocation failures propagate; options configure initial paused/looping/gain state.
 */

/** @fn void CE::Audio::Miniaudio::System::set_volume(float volume)
 * Set master linear gain after finite[0,1] validation. Native failure throws before committing the cached gain; closed systems reject.
 */

/** @fn float CE::Audio::Miniaudio::System::volume() const
 * Return the locked master gain; reject closed output.
 */

/** @fn void CE::Audio::Miniaudio::System::maintain()
 * Under the system lock, retire nonplaying voices whose only owner is the system. Active playback and externally retained voices remain; closed output rejects.
 */

/** @fn void CE::Audio::Miniaudio::System::close() noexcept
 * Idempotently stop/retire all voices and uninitialize output under the shared lock. Voice handles then report Closed.
 */

/** @fn bool CE::Audio::Miniaudio::System::closed() const
 * Return whether output has been uninitialized, under the shared lock.
 */

/** @fn CE::Audio::Format CE::Audio::Miniaudio::System::output_format() const
 * Return a copied configured output format; closed systems reject.
 */

/** @fn std::string CE::Audio::Miniaudio::System::backend_name() const
 * Return an owned backend identity string for diagnostics; closed systems reject.
 */

/** @fn void CE::Audio::Miniaudio::System::render(std::span<float> output)
 * Mix into borrowed interleaved float storage in offline mode only, serialized with controls/maintenance. Size must contain complete output frames; empty spans are permitted. Native/closed mode and malformed sizes reject. Successful short reads are padded with silence while advancing the full requested interval. No span is retained; native failures can propagate after partial output/time changes.
 */
