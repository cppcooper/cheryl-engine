#include <audio/system.h>
#include <internals/exceptions.h>

#include <cmath>

namespace CE::Audio {
    void validate_volume(float volume) {
        if (!std::isfinite(volume) || volume < 0 || volume > 1)
            throw Exceptions::invalid_args(CE_HERE, "Audio volume must be finite and within [0, 1]");
    }

    void PlaybackOptions::validate() const { validate_volume(volume); }
}
