#pragma once

#include <cheryl/export.h>
#include "viewport.h"

#include <cstdint>

namespace CE::Platform {
    /** Value snapshot of monitor video-mode dimensions with a display-defined ID.
     * Copying it does not refresh dimensions or retain a native monitor handle.
     * Native lookup remains subject to the originating display's inventory lifetime.
     */
    struct Monitor : Dimensions<int> {
        CE_API Monitor(std::uint64_t id, int width, int height);
        Monitor(const Monitor&) = default;
        // Inventory identity, not an OS handle or persistent hardware identifier.
        [[nodiscard]] std::uint64_t id() const { return id_; }

    private:
        std::uint64_t id_;
    };
}

/**
 * @fn CE::Platform::Monitor::Monitor(std::uint64_t id, int width, int height)
 * @brief Copy a display-defined identity and video-mode dimensions.
 * @param id Identity understood by the originating display inventory, not an OS handle.
 * @param width Snapshot video-mode width.
 * @param height Snapshot video-mode height.
 * No native monitor ownership or future refresh is acquired.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn CE::Platform::Monitor::Monitor(const Monitor&)
 * @brief Copy identity and dimensions without retaining the display or refreshing native state.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn std::uint64_t CE::Platform::Monitor::id() const
 * @brief Read the copied inventory identity.
 * @return Display-defined value; meaningful for lookup only within its original inventory lifetime.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */
