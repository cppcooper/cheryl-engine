#pragma once

#include <cheryl/export.h>

namespace CE::Rendering {
    /** Presents a completed frame on a window owned by the display adapter.
     * A graphics backend can extend this interface with its context requirements.
     * Call on the platform/presentation owner while that borrowed window is alive;
     * presentation does not publish simulation state or retain a CE::Rendering::Frame.
     */
    class CE_API PresentationSurface {
    public:
        virtual ~PresentationSurface() = default;
        virtual void present() = 0;
    };
}

/**
 * @fn CE::Rendering::PresentationSurface::~PresentationSurface()
 * @brief Destroy the presentation adapter; the display/window lifetime remains with its owning adapter.
 */

/**
 * @fn void CE::Rendering::PresentationSurface::present()
 * @brief Present completed backend rendering on the platform/presentation owner.
 * Keep the borrowed window and required backend context live through the call. This operation neither retains a Frame nor publishes simulation state; concrete presentation errors may propagate.
 */
