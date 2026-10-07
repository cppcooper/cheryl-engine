#pragma once

namespace CE::RenderAPIs {
    /** Presents a completed frame on a window owned by the display adapter.
     * A graphics backend can extend this interface with its context requirements.
     * Call on the platform/presentation owner while that borrowed window is alive;
     * presentation does not publish simulation state or retain a RenderFrame.
     */
    class iPresentationSurface {
    public:
        virtual ~iPresentationSurface() = default;
        virtual void present() = 0;
    };
}
