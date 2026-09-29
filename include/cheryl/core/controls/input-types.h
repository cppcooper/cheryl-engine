#pragma once

namespace CE::Input {
    // Opaque numeric identifiers shared by input adapters and game bindings.
    using DeviceId = unsigned int;
    using DeviceButtonId = unsigned int;

    /** Independently requested capture channels. State is always collected;
     * Events/Text requests preserve records without replacing ordinary State.
     * Focus/routing remains independent of channel activation.
     */
    enum class InputMode { State, Events, Text };

    // Portable IDs reserved for relative mouse inputs; position IDs remain backend-defined.
    namespace MouseControl {
        inline constexpr DeviceButtonId DeltaX = 0xFFFFFFF0;
        inline constexpr DeviceButtonId DeltaY = 0xFFFFFFF1;
        inline constexpr DeviceButtonId ScrollX = 0xFFFFFFF2;
        inline constexpr DeviceButtonId ScrollY = 0xFFFFFFF3;
    }
}
