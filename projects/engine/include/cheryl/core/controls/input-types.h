#pragma once

#include <cstdint>
#include <limits>

namespace CE::Input {
    // Device IDs are adapter-owned; standard control IDs are defined by Cheryl.
    /** Adapter-owned device identity; acquire/query on the platform input owner. */
    using DeviceId = unsigned int;
    /** Physical control token within a device; portable families use disjoint ranges. */
    using ControlId = unsigned int;
    /** Sentinel returned for Unknown/out-of-range portable controls; never a valid mapping control. */
    inline constexpr ControlId invalid_control = std::numeric_limits<ControlId>::max();
    namespace Routing {
        /** Application-defined routing owner; zero denotes no focused owner, not a valid focus request. */
        using TargetId = std::uint64_t;
        /** Gameplay admission for focused keyboard records; pointer selection is separate. */
        enum class KeyboardPolicy { Exclusive /**< Suppress gameplay keyboard mapping. */, PassThrough /**< Deliver to focused UI and gameplay. */ };

    }

    namespace Capture {
        /** Independently requested capture channels. State is always collected;
     * Events/Text requests preserve records without replacing ordinary State.
     * Focus/routing remains independent of channel activation.
     */
        enum class Channel { State /**< Always-collected semantic state. */, Events /**< Ordered physical observations. */, Text /**< OS-committed Unicode scalars. */ };

    }

    // Portable mouse axes and wheel-button pulses.
    namespace MouseControl {
        /** Absolute logical cursor X. */
        inline constexpr CE::Input::ControlId X = 0xFFFFFFE0;
        /** Absolute logical cursor Y. */
        inline constexpr CE::Input::ControlId Y = 0xFFFFFFE1;
        /** Positive vertical wheel pulse button. */
        inline constexpr CE::Input::ControlId WheelUp = 0xFFFFFFE2;
        /** Negative vertical wheel pulse button. */
        inline constexpr CE::Input::ControlId WheelDown = 0xFFFFFFE3;
        /** Relative logical cursor X motion per poll. */
        inline constexpr CE::Input::ControlId DeltaX = 0xFFFFFFF0;
        /** Relative logical cursor Y motion per poll. */
        inline constexpr CE::Input::ControlId DeltaY = 0xFFFFFFF1;
        /** Fractional horizontal wheel delta per poll. */
        inline constexpr CE::Input::ControlId ScrollX = 0xFFFFFFF2;
        /** Fractional vertical wheel delta per poll. */
        inline constexpr CE::Input::ControlId ScrollY = 0xFFFFFFF3;
    }
}
