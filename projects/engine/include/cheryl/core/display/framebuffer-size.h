#pragma once

namespace CE::Platform {
    /** Copied drawable dimensions in framebuffer pixels; zero extent can represent a minimized window. */
    struct FramebufferSize {
        int width = 1; ///< Drawable width in pixels, independent of logical window coordinates.
        int height = 1; ///< Drawable height in pixels.

        bool operator==(const FramebufferSize&) const = default;
    };
}

/**
 * @fn bool CE::Platform::FramebufferSize::operator==(const FramebufferSize&) const
 * @brief Compare both pixel dimensions by value; performs no native size query.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */
