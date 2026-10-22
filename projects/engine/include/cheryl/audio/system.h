#pragma once

#include <cheryl/export.h>

#include "clip.h"
#include "playback-events.h"
#include <core/subsystems/event-bus.h>

#include <filesystem>
#include <memory>
#include <optional>

namespace CE::Audio {
    /** Observed voice lifecycle; only restart() restarts stopped/finished playback. */
    enum class PlaybackState { Playing /**< Advancing. */, Paused /**< Suspended with progress retained. */, Finished /**< Reached end. */, Stopped /**< Explicitly stopped. */, Closed /**< Owning system is closed. */ };

    /** Initial per-voice controls; validate before admitting playback. */
    struct PlaybackOptions {
        float volume = 1; ///< Linear normalized gain in[0,1], finite; independent of master gain.
        bool looping = false; ///< Repeat the source at its end.
        bool paused = false; ///< Create without starting the playback clock for this voice.

        CE_API void validate() const;
    };

    /** Owned synchronized observation; native progress can change state immediately after return. */
    struct VoiceSnapshot {
        ::CE::Audio::PlaybackState state = ::CE::Audio::PlaybackState::Closed; ///< Observed playback/system state.
        float volume = 1; ///< Current per-voice normalized linear gain.
        bool looping = false; ///< Current looping policy; changing it does not restart playback.
        std::optional<std::chrono::duration<double>> duration; ///< Source duration in seconds when known; not current play position.
        std::uint64_t system = 0; ///< Checked output identity; zero for backends without outcome support.
        std::uint64_t voice = 0; ///< Logical voice identity, stable across successful restart.
        std::uint64_t generation = 0; ///< Current admitted generation; starts at one when supported.
        std::optional<PlaybackTerminal> terminal; ///< Immutable current-generation outcome, retained after close.
    };

    // Normalized linear gain, inclusive [0, 1]; rejects non-finite values.
    CE_API void validate_volume(float volume);

    /** One independently controlled playback. Controls and snapshots are synchronized.
     * Pause preserves progress; resume affects only Paused voices. Stop is terminal
     * until restart explicitly requests frame zero. Finished voices also need restart.
     * Changing looping does not restart a stopped/finished voice.
     * Snapshot state can change immediately afterward as native playback progresses.
     * A handle may survive its system: snapshot then reports Closed, and controls throw.
     * Releasing a handle does not stop an active voice; the system retains playback.
     */
    class CE_API Voice {
    public:
        virtual ~Voice() = default;
        [[nodiscard]] virtual ::CE::Audio::VoiceSnapshot snapshot() const = 0;
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
     * Voice handles remain safe. After close, only closure/notification operations remain available.
     */
    class CE_API System {
    public:
        virtual ~System() = default;
        [[nodiscard]] virtual std::shared_ptr<::CE::Audio::Voice>
        play(const ::CE::Audio::Clip& clip, ::CE::Audio::PlaybackOptions options = {}) = 0;
        [[nodiscard]] virtual std::shared_ptr<::CE::Audio::Voice>
        stream(const std::filesystem::path& file, ::CE::Audio::PlaybackOptions options = {}) = 0;
        virtual void set_volume(float volume) = 0;
        [[nodiscard]] virtual float volume() const = 0;
        // Reclaims finished/stopped/paused voices with no application handle.
        // Call regularly; this does not advance the playback clock.
        virtual void maintain() = 0;
        virtual void close() noexcept = 0;
        [[nodiscard]] virtual bool closed() const = 0;
        virtual void publish_events_to(::CE::Events::EventBus::Publisher publisher);
        virtual void publish_notifications();
        [[nodiscard]] virtual PlaybackNotifications notification_snapshot() const;
    };
}

/** @fn void CE::Audio::validate_volume(float volume)
 * Reject nonfinite values or values outside inclusive[0,1] with invalid_args. Gain is linear, not decibels.
 */

/** @fn void CE::Audio::PlaybackOptions::validate() const
 * Validate normalized finite volume; boolean looping/paused options require no further validation.
 */

/** @fn CE::Audio::Voice::~Voice()
 * Release the application handle without implicitly stopping an active voice; the owning system retains active playback.
 */

/** @fn CE::Audio::VoiceSnapshot CE::Audio::Voice::snapshot() const
 * Return a synchronized owned state observation. Surviving handles report Closed after system close; no progress lock is retained by the returned value.
 */

/** @fn void CE::Audio::Voice::pause()
 * Suspend a Playing voice while retaining progress; other states need not change. Controls reject after system close and native failures can propagate.
 */

/** @fn void CE::Audio::Voice::resume()
 * Resume only a Paused voice from retained progress. Stopped/Finished voices require restart(); controls reject after system close.
 */

/** @fn void CE::Audio::Voice::stop()
 * Stop playback terminally until restart() explicitly requests frame zero. Does not destroy this handle; closed systems reject controls.
 * Outcome-capable backends preserve an already observed Completed terminal reason rather than
 * overwriting it with Stopped. Controls stage outcomes; only the owner pump invokes listeners.
 */

/** @fn void CE::Audio::Voice::restart()
 * Request fresh playback from frame zero, preserving volume/looping. Source/device failures can propagate; streaming restart may reopen the retained file. Concrete backends define failure effects beyond the common closed-system rejection.
 */

/** @fn void CE::Audio::Voice::set_volume(float volume)
 * Set synchronized per-voice linear gain, inclusive[0,1]. Invalid/nonfinite volume rejects; system closure rejects even if the handle survives.
 */

/** @fn void CE::Audio::Voice::set_looping(bool looping)
 * Change looping without restarting a stopped/finished voice. Closed-system controls reject.
 */

/** @fn CE::Audio::System::~System()
 * Close output and retire voices after producers quiesce. Voice handles can survive safely but cannot continue playback.
 */

/** @fn std::shared_ptr<CE::Audio::Voice> CE::Audio::System::play(const Clip& clip, PlaybackOptions options)
 * Admit an independent voice retaining the immutable clip data. Options are copied; initial conversion/admission may block and fail. Playback progresses on the backend clock without game updates. Return a retained control handle; releasing it does not stop active playback.
 * @see [Audio output](cheryl-guide:modules/miniaudio#preparation-and-output)
 */

/** @fn std::shared_ptr<CE::Audio::Voice> CE::Audio::System::stream(const std::filesystem::path& file, PlaybackOptions options)
 * Open an independent streaming voice, copying path/options. Keep source files available until playback ends and for later restart; opening may block/fail. No game callback executes on the native mixer thread.
 */

/** @fn void CE::Audio::System::set_volume(float volume)
 * Set synchronized master linear gain in inclusive[0,1], independently of per-voice gain. Invalid/nonfinite values and closed systems reject.
 */

/** @fn float CE::Audio::System::volume() const
 * Return synchronized master gain; closed-system queries reject.
 */

/** @fn void CE::Audio::System::maintain()
 * Reclaim Finished/Stopped/Paused voices with no application handle. Call regularly; this does not advance the backend clock. Closed systems reject.
 */

/** @fn void CE::Audio::System::close() noexcept
 * Idempotently stop output and retire voices before backend state. Synchronize producers before destruction; surviving Voice snapshots become Closed and controls throw.
 * Notification-capable backends stage unfinished terminal outcomes without listeners and permit a
 * final publish_notifications() after close. Destruction discards unpublished observations.
 */

/** @fn bool CE::Audio::System::closed() const
 * Return synchronized output closure state; remains callable after close().
 */

/** @fn void CE::Audio::System::publish_events_to(CE::Events::EventBus::Publisher publisher)
 * Choose a weak notification destination and designate the calling thread as publication owner.
 * Call before pumping/attaching services. A configured source rejects foreign-owner changes and
 * changes during publication. Empty publishers discard attempts. Unsupported backends throw.
 */
/** @fn void CE::Audio::System::publish_notifications()
 * Observe completion and attempt pending outcomes through the entry sequence cutoff on the designated
 * owner (the first pump designates it if unconfigured). Never call recursively. Listeners run outside
 * audio locks; their new records wait for a later pump. Allowed after close for final outcomes.
 * Each record has one attempt; bus failure has no retry. Unsupported backends throw.
 */
/** @fn CE::Audio::PlaybackNotifications CE::Audio::System::notification_snapshot() const
 * Return synchronized closure, sequence and bounded-loss observations without invoking listeners.
 * Default backends return system zero. Retain individual Voice snapshots to reconcile a live
 * generation after a gap; neither snapshot recovers superseded playback history.
 */
