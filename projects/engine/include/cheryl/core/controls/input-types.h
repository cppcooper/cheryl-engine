#pragma once

#include <cstdint>
#include <limits>

namespace CE::Input {
    // Device IDs are adapter-owned; standard control IDs are defined by Cheryl.
    using DeviceId = unsigned int;
    using ControlId = unsigned int;
    inline constexpr ControlId invalid_control = std::numeric_limits<ControlId>::max();
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

    // Portable mouse axes and wheel-button pulses.
    namespace MouseControl {
        inline constexpr CE::Input::ControlId X = 0xFFFFFFE0;
        inline constexpr CE::Input::ControlId Y = 0xFFFFFFE1;
        inline constexpr CE::Input::ControlId WheelUp = 0xFFFFFFE2;
        inline constexpr CE::Input::ControlId WheelDown = 0xFFFFFFE3;
        inline constexpr CE::Input::ControlId DeltaX = 0xFFFFFFF0;
        inline constexpr CE::Input::ControlId DeltaY = 0xFFFFFFF1;
        inline constexpr CE::Input::ControlId ScrollX = 0xFFFFFFF2;
        inline constexpr CE::Input::ControlId ScrollY = 0xFFFFFFF3;
    }
}
