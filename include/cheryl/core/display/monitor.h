#pragma once
#include "viewport.h"

#include <cstdint>

namespace CE {
    class DisplaySystem;

    // A display-owned snapshot of a monitor's current video mode.
    struct Monitor : ViewPort<int> {
        Monitor(const Monitor&) = default;

    private:
        friend class DisplaySystem;
        Monitor(std::uint64_t id, int width, int height);

        std::uint64_t id_;
    };
}
