#pragma once

#include <cstdint>

namespace CE::Input {
    // Opaque numeric identifiers shared by input adapters and game bindings.
    using DeviceId = unsigned int;
    using ControlId = unsigned int;
    namespace Routing {
        using TargetId = std::uint64_t; // Zero denotes no focused UI owner.
        enum class KeyboardPolicy { Exclusive, PassThrough };

    }

    namespace Capture {
        /** Independently requested capture channels. State is always collected;
     * Events/Text requests preserve records without replacing ordinary State.
     * Focus/routing remains independent of channel activation.
     */
        enum class Channel { State, Events, Text };

    }

    // Portable IDs reserved for relative mouse inputs; position IDs remain backend-defined.
    namespace MouseControl {
        inline constexpr CE::Input::ControlId DeltaX = 0xFFFFFFF0;
        inline constexpr CE::Input::ControlId DeltaY = 0xFFFFFFF1;
        inline constexpr CE::Input::ControlId ScrollX = 0xFFFFFFF2;
        inline constexpr CE::Input::ControlId ScrollY = 0xFFFFFFF3;
    }
}
