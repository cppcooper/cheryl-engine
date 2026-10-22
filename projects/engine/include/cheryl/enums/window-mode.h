#pragma once

namespace CE::Platform {
    /** Window presentation choice interpreted by the selected platform backend. */
    enum class WindowMode {
        NORMAL = 1, ///< Decorated window using the requested client dimensions.
        BORDERLESS, ///< Undecorated window; does not itself select an exclusive monitor mode.
        FULLSCREEN  ///< Fullscreen presentation on the selected display monitor.
    };
}
