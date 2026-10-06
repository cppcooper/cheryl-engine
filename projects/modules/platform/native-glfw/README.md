# Native GLFW platform module

`Cheryl::NativeGLFW` supplies GLFW-backed display/windows and optional Gainput-backed
input. It implements the engine's display and input contracts from one platform
owner, which also manages GLFW lifetime and native callback diagnostics.

## Selection and composition

The root selects this owner with `CHERYL_BUILD_NATIVE_GLFW=ON`. Set
`CHERYL_BUILD_OPENGL=OFF` to use the platform module without Cheryl's graphics
backend; TGUI selection is independent. Applications link:

```cmake
target_link_libraries(game PRIVATE Cheryl::NativeGLFW)
```

Engine and the public Gainput headers propagate through the target. GLFW stays a
private dependency. Selection options for a fresh module configuration are:

| Option | Default | Effect |
| --- | --- | --- |
| `CHERYL_NATIVE_INPUT` | `ON` | Includes Gainput-backed input and its Linux X11 requirement. |
| `CHERYL_NATIVE_NULL_PLATFORM` | `OFF` | Configures an owned GLFW target without X11/Wayland. Disable native input as well to omit Gainput/X11. |
| `CHERYL_BUILD_TESTS` | `OFF` standalone; `ON` at root | Selects this owner's implementation checks. |

From the repository root, a standalone build can bootstrap an explicit Engine
checkout without selecting other integration owners:

```sh
cmake -S projects/modules/platform/native-glfw -B build/native-module \
  -DCMAKE_BUILD_TYPE=Release \
  -DCHERYL_ENGINE_SOURCE=/absolute/path/to/cheryl-engine
cmake --build build/native-module --target module_native_glfw --parallel 1
```

On Linux, bundled GLFW enables both X11 and Wayland; use
`GLFW_BUILD_WAYLAND=OFF` for X11 only. See the root
[dependency table](../../../../README.md#dependencies) for system requirements and
Gainput's optional HID fetch.

## Engine contracts

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

With native input enabled, the owner reuses a supplied `gainput::gainput`,
`gainputstatic` or `gainput` target, in that order, or adds the selected
`extern/gainput` source. Gainput's public headers propagate through that target;
the owner supplies the missing usage requirement for an owned legacy source build.
Supplied dependency targets keep their host's settings. Owned source composition
suppresses Gainput samples/tests locally, including cached `ON` choices, without
rewriting the host cache. The selected fork's `GAINPUT_ENABLE_HID` setting remains
available; its HID support brings hidapi and the platform's HID development libraries.

## Input lifetime and mapping

The adapter calls Gainput `Init` before creating devices on its first window
attachment, and `Exit` at adapter destruction. Windows initialization receives the
GLFW window's native HWND; other platforms initialize the HID backend without a
window handle. Detachment clears window state while retaining devices for
reattachment. Each `Update` receives elapsed steady-clock seconds between attached
polls; time spent detached does not enter that interval.

Gainput's HID state is process-global. Coordination between simultaneous initialized
native adapters and notification rebinding to a different Windows window remains
[unresolved work](../../../../docs/planning/todo.md).

The input mapper uses Gainput's five-argument `OnDeviceButtonFloat` callback.
It forwards `newValue` as the current axis state; the elapsed-time argument does
not scale that value. Externally driven devices retain their callback-order state
without replaying Gainput notifications.

Target selection, standalone paths and validation status are in
[the module guide](../../../../docs/development/modules.md).

## Checks

With tests selected, `native-glfw-tests` covers mapping and callback diagnostics;
`native-glfw-all` is the owner's aggregate. `CHERYL_BUILD_CONSUMER_TESTS=ON` adds
`cheryl-native-glfw-consumer` and its first-include header probes. Native graphics
runtime checks belong to the OpenGL owner's opt-in acceptance suite. Use the
[composition procedures](../../../../docs/development/architecture-validation.md)
when changing module boundaries.
