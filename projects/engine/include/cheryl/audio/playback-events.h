#pragma once

#include <core/subsystems/event-channel.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace CE::Audio {
    /** Immutable logical outcome, selected when the owner observes native state. */
    enum class PlaybackEndReason {
        Completed,    ///< Native EOF was observed before another terminal operation.
        Stopped,      ///< Explicit stop succeeded, including a failed restart's stop.
        Replaced,     ///< A fresh generation successfully replaced unfinished playback.
        OutputClosed, ///< Output closed before another outcome was observed.
        Reclaimed     ///< An unreferenced inactive generation was reclaimed.
    };

    /** One terminal fact; later controls/closure preserve it until a successful restart. */
    struct PlaybackTerminal {
        PlaybackEndReason reason = PlaybackEndReason::Completed; ///< Selected terminal cause.
        std::uint64_t sequence = 0; ///< Checked source sequence, shared by all voices.
        std::chrono::steady_clock::time_point observed_at; ///< Owner observation, not sample-accurate mixer time.
        bool operator==(const PlaybackTerminal&) const = default;
    };

    /** Optional observation of one admitted playback generation; retains no Voice/native objects. */
    struct PlaybackEnded {
        std::uint64_t system = 0; ///< Checked permanent output identity.
        std::uint64_t voice = 0; ///< Checked logical voice identity, stable across restart.
        std::uint64_t generation = 0; ///< Starts at one; advances only on successful restart.
        PlaybackTerminal terminal; ///< Immutable outcome for this generation.
        std::uint64_t lost_observations = 0; ///< Cumulative dropped outbox records at publication.
        std::uint64_t first_lost_sequence = 0; ///< First dropped sequence, zero if none.
        std::uint64_t last_lost_sequence = 0; ///< Latest dropped sequence; the range may contain delivered records.
    };

    /** Current source reconciliation; does not recover retired generation history. */
    struct PlaybackNotifications {
        std::uint64_t system = 0; ///< Zero means unsupported by this backend.
        std::uint64_t sequence = 0; ///< Latest committed terminal outcome.
        std::uint64_t lost_observations = 0; ///< Records dropped by bounded overflow.
        std::uint64_t first_lost_sequence = 0; ///< First dropped sequence, zero if none.
        std::uint64_t last_lost_sequence = 0; ///< Latest dropped sequence.
        std::size_t pending = 0; ///< Staged publication attempts; callbacks never run while staging.
        std::size_t capacity = 0; ///< Fixed allocation established before opening output.
        bool closed = false; ///< Native output has closed; an explicit final pump remains allowed.
    };

    /** Bounded optional observation storage; overflow drops oldest records without rejecting playback. */
    struct PlaybackNotificationOptions {
        std::size_t capacity = 1024; ///< Positive record capacity, allocated before native output opens.
    };

    /** Typed optional outcome channel; immediate listeners run on the designated publication owner. */
    inline const CE::Events::EventChannel<PlaybackEnded> playback_ended_event{"playback-ended"};
}
