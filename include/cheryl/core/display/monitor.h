#pragma once
#include "viewport.h"

#include <cstdint>

namespace CE {
    // A display-owned snapshot of a monitor's current video mode.
    struct Monitor : ViewPort<int> {
        Monitor(std::uint64_t id, int width, int height);
        Monitor(const Monitor&) = default;
        [[nodiscard]] std::uint64_t id() const { return id_; }

    private:
        std::uint64_t id_;
    };
}
