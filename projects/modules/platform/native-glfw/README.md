# Native GLFW platform module

`Cheryl::NativeGLFW` supplies GLFW-backed display/windows and optional Gainput-backed
input. It implements the engine's display and input contracts from one platform
owner, which also manages GLFW lifetime and native callback diagnostics.

| Engine contract | Implementation | Responsibility |
| --- | --- | --- |
| [`CE::iDisplaySystem`](../../../engine/include/cheryl/core/display/display-system-interface.h) | [`CE::DisplaySystem`](include/cheryl/core/display/display-system.h) | Monitor snapshots, owned windows and active-window selection. |
| [`CE::iWindow`](../../../engine/include/cheryl/core/display/window-interface.h) | [`CE::Window`](include/cheryl/core/display/window.h) | Window sizes/modes, resizing, cursor visibility and deferred callback failures. |
| [`CE::Input::iInputSystem`](../../../engine/include/cheryl/core/controls/input-interface.h) | [`CE::Input::InputSystem`](include/cheryl/core/controls/input-system.h) | Device bindings and published State/Events/Text polls with capture and focus routing. Selected by `CHERYL_NATIVE_INPUT`. |

The existing directory structure follows those contract areas:

```text
include/cheryl/core/display/   # Concrete display/window headers
include/cheryl/core/controls/  # Concrete input and mapping headers
src/core/display/             # Window/display implementation and GLFW diagnostics
src/core/controls/            # Native input collection and mappings
tests/                       # Implementation cases and consumer/header probes
```

The graphics owner selects its window client API and supplies rendering and
presentation. GLFW/OpenGL context selection, buffer swapping and the native graphics
factory live in the OpenGL module. A different graphics implementation can use the
platform window/input facilities with its own API-specific setup.

Target selection, standalone paths and validation status are in
[the module guide](../../../../docs/development/modules.md).
