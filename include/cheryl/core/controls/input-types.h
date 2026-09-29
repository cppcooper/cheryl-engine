#pragma once

namespace CE::Input {
    // Opaque numeric identifiers shared by input adapters and game bindings.
    using DeviceId = unsigned int;
    using DeviceButtonId = unsigned int;

    /** Capture channels, not mutually exclusive global modes.
     * State is implemented by ActionSnapshot/TickInput. Events and Text name
     * future contracts; selecting them is not supported by iInputSystem yet.
     * Focus/routing (for example, keyboard input into a textbox instead of
     * gameplay actions) is a separate concern from capture fidelity.
     */
    enum class InputMode { State, Events, Text };
}
