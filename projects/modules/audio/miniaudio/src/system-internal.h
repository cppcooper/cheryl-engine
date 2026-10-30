#pragma once

#include <backends/miniaudio.h>

namespace CE::Audio::Miniaudio::Detail {
    // Deterministic native failure/counter boundaries for the offline contract checks.
    struct SystemAccess {
        static CE_AUDIO_MINIAUDIO_API void fail_next_stop(System& system);
        static CE_AUDIO_MINIAUDIO_API void fail_next_start(System& system);
        static CE_AUDIO_MINIAUDIO_API void reserve_last_terminal(System& system);
    };
}
