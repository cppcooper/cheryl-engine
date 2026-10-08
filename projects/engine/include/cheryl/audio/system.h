#pragma once

#include "clip.h"

#include <filesystem>
#include <memory>
#include <optional>

namespace CE::Audio {
    enum class PlaybackState { Playing, Paused, Finished, Stopped, Closed };

    struct PlaybackOptions {
        float volume = 1;
        bool looping = false;
        bool paused = false;

        void validate() const;
    };

    struct VoiceSnapshot {
        PlaybackState state = PlaybackState::Closed;
        float volume = 1;
        bool looping = false;
        std::optional<std::chrono::duration<double>> duration;
    };

    // Normalized linear gain, inclusive [0, 1]; rejects non-finite values.
    void validate_volume(float volume);

    /** One independently controlled playback. Controls and snapshots are synchronized.
     * Pause preserves progress; resume affects only Paused voices. Stop is terminal
     * until restart explicitly requests frame zero. Finished voices also need restart.
     * Changing looping does not restart a stopped/finished voice.
     * Snapshot state can change immediately afterward as native playback progresses.
     * A handle may survive its system: snapshot then reports Closed, and controls throw.
     * Releasing a handle does not stop an active voice; the system retains playback.
     */
    class Voice {
    public:
        virtual ~Voice() = default;
        [[nodiscard]] virtual VoiceSnapshot snapshot() const = 0;
        virtual void pause() = 0;
        virtual void resume() = 0;
        virtual void stop() = 0;
        virtual void restart() = 0;
        virtual void set_volume(float volume) = 0;
        virtual void set_looping(bool looping) = 0;
    };

    /** Application-owned audio output and voice lifetime, independent of graphics.
     * Play/stream/control/maintenance/close are synchronized across producers.
     * Playback progresses on the backend clock, without update or render admission.
     * Device opening, decode and initial stream opening may block; prepare them
     * outside latency-sensitive simulation work. Stream paths remain required until
     * playback ends. No game callback runs from the native mixing thread.
     * Destroy/close stops output and retires voices before native output state.
     * The application settles producers before destroying this object. Surviving
     * Voice handles remain safe. Subsequent system operations throw, except closed/close.
     */
    class System {
    public:
        virtual ~System() = default;
        [[nodiscard]] virtual std::shared_ptr<Voice> play(const Clip& clip, PlaybackOptions options = {}) = 0;
        [[nodiscard]] virtual std::shared_ptr<Voice> stream(const std::filesystem::path& file, PlaybackOptions options = {}) = 0;
        virtual void set_volume(float volume) = 0;
        [[nodiscard]] virtual float volume() const = 0;
        // Reclaims finished/stopped/paused voices with no application handle.
        // Call regularly; this does not advance the playback clock.
        virtual void maintain() = 0;
        virtual void close() noexcept = 0;
        [[nodiscard]] virtual bool closed() const = 0;
    };
}
