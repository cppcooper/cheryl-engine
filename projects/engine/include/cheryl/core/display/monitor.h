#pragma once
#include "viewport.h"

#include <cstdint>

namespace CE {
    /** Value snapshot of monitor video-mode dimensions with a display-defined ID.
     * Copying it does not refresh dimensions or retain a native monitor handle.
     * Native lookup remains subject to the originating display's inventory lifetime.
     */
    struct Monitor : ViewPort<int> {
        Monitor(std::uint64_t id, int width, int height);
        Monitor(const Monitor&) = default;
        // Inventory identity, not an OS handle or persistent hardware identifier.
        [[nodiscard]] std::uint64_t id() const { return id_; }

    private:
        std::uint64_t id_;
    };
}
