#pragma once

#include "input-bindings.h"

namespace CE {
    class iWindow;
}

namespace CE::Input {
    // The input adapter used by an engine. Device and button IDs are opaque to
    // the engine; each adapter supplies its own event translation.
    // TODO: Define which thread poll() runs on and where game-facing input is delivered. A concurrent
    // implementation should separate platform event collection from simulation consumption rather than
    // making bindings implicitly execute on whichever thread owns an input backend.
    class iInputSystem {
    public:
        virtual ~iInputSystem() = default;
        virtual void initialize(iWindow& window) = 0;
        virtual void poll() = 0;
        virtual void deinitialize() = 0;
        [[nodiscard]] virtual InputBindings& bindings() = 0;
        [[nodiscard]] virtual DeviceId keyboard_id() const = 0;
        [[nodiscard]] virtual DeviceId mouse_id() const = 0;
        [[nodiscard]] virtual DeviceId gamepad_id() const = 0;
    };
}
