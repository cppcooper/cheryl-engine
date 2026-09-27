#pragma once

#include "input-bindings.h"

#include <memory>

namespace CE {
    class iWindow;
}

namespace CE::Input {
    /** Engine-facing input adapter. Poll on the platform thread, then hand the complete immutable
     * action snapshot to simulation. Backends publish after processing physical input for each poll.
     * Legacy binding callbacks still run on the poll thread and require their own thread discipline.
     */
    class iInputSystem {
    public:
        virtual ~iInputSystem() = default;
        virtual void initialize(iWindow& window) = 0;
        virtual void poll() = 0;
        virtual void deinitialize() = 0;
        [[nodiscard]] virtual InputBindings& bindings() = 0;
        [[nodiscard]] virtual std::shared_ptr<const ActionSnapshot> action_snapshot() { return bindings().action_snapshot(); }
        [[nodiscard]] virtual DeviceId keyboard_id() const = 0;
        [[nodiscard]] virtual DeviceId mouse_id() const = 0;
        [[nodiscard]] virtual DeviceId gamepad_id() const = 0;
    };
}
