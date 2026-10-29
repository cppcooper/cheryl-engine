#include <audio/system.h>
#include <internals/exceptions.h>

#include <cmath>

namespace CE::Audio {
    void validate_volume(float volume) {
        if (!std::isfinite(volume) || volume < 0 || volume > 1)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Audio volume must be finite and within [0, 1]");
    }

    void validate_playback_rate(float rate) {
        if (!std::isfinite(rate) || rate < 0.125f || rate > 8.0f)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Audio playback rate must be finite and within [0.125, 8]");
    }

    void PlaybackOptions::validate() const { validate_volume(volume); }

    void Voice::set_playback_rate(float rate) {
        validate_playback_rate(rate);
        throw CE::Exceptions::bad_request(CE_HERE, "Playback-rate control is unsupported by this audio backend");
    }

    void System::publish_events_to(CE::Events::EventBus::Publisher) {
        throw CE::Exceptions::bad_request(CE_HERE, "Playback notifications are unsupported by this audio backend");
    }
    void System::publish_notifications() {
        throw CE::Exceptions::bad_request(CE_HERE, "Playback notifications are unsupported by this audio backend");
    }
    PlaybackNotifications System::notification_snapshot() const { return {}; }
}
