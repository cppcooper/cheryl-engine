(native-glfw-platform-module)=
# Native GLFW platform reference

`Cheryl::NativeGLFW` supplies GLFW-backed display/windows and optional Gainput-backed
input. It implements the engine's display and input contracts from one platform
owner, which also manages GLFW lifetime and native callback diagnostics.

## Selection and composition

The root selects this owner with `CHERYL_BUILD_NATIVE_GLFW=ON`. Set
`CHERYL_BUILD_OPENGL=OFF` to use the platform module without Cheryl's graphics
backend; UI selection is independent. The library's build target is
`module_native_glfw`, with output name `cheryl-module-native-glfw`. Applications link:

```cmake
target_link_libraries(game PRIVATE Cheryl::NativeGLFW)
```

Engine usage settings propagate through the target. GLFW, Gainput and selected
HID implementations are privately owned by this module; consumer headers expose
Cheryl-owned types and binding/query operations. Selection options are:

| Option | Default | Effect |
| --- | --- | --- |
| `CHERYL_NATIVE_INPUT` | `ON` | Includes Gainput-backed input and its Linux X11 requirement. |
| `CHERYL_NATIVE_NULL_PLATFORM` | `OFF` | Configures an owned GLFW target without X11/Wayland. Disable native input as well to omit Gainput/X11. |
| `CHERYL_BUILD_TESTS` | `OFF` | Selects this owner's implementation checks. |

From the repository root, a standalone build can bootstrap an explicit Engine
checkout without selecting other integration owners:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S projects/modules/platform/native-glfw -B build/native-module -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCHERYL_REPOSITORY_ROOT="$PWD"
)
```

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake --build build/native-module --target module_native_glfw --parallel
)
```

On Linux, bundled GLFW enables both X11 and Wayland; use
`GLFW_BUILD_WAYLAND=OFF` for X11 only. See the
[build guide](../development/building.md) for system requirements and
Gainput's optional HID fetch.

## Use with an application

For a GLFW/OpenGL game, use the [standard Startup path](startup.md#standard-application-path)
to assemble the compatible display, input, presentation, renderer and resources.
Link `Cheryl::Cheryl` for the configured assembly, or select granular targets as
shown in [Source and SDK integration](../development/consuming-engine.md#granular-composition).
The runtime initializes input before the game's `init_platform()` and detaches it while
its display-owned window is still live.

Bind controls during platform-owned initialization using the
[binding task](../getting-started/how-to/bind-input.md). Reusable runtime consumers
can attach/remove [action contexts](../runtime/input-state-model.md#action-contexts)
through queued requests; direct binding edits still belong to the polling owner.
During simulation, use the
tick's action state and copied logical/framebuffer dimensions instead of polling
or querying a live window. For a platform-only application, link Native GLFW and
use its display/window interfaces directly; graphics/presentation setup belongs
to the selected graphics implementation.

### Choose a controller path

| Configuration | Current usable scope |
| --- | --- |
| Linux with `GAINPUT_ENABLE_HID=OFF` | The joydev path reads `/dev/input/js0`; physical acceptance covers Bluetooth DualSense ordinary controls. It needs a readable device node. |
| Direct HID selection | Integration is unfinished. Enabling the CMake option alone does not establish active polling, correct report identity or retained pad state. |
| Windows or macOS controllers | Backend-specific gaps remain and the Linux observation does not accept those platforms. |

[Linux controller mapping](#linux-controller-mapping) explains supported reports
and fixed device selection; [controller backend limits](#controller-backend-limits)
retains the source diagnosis for extending the integration. To investigate a
missing press, start with [controller diagnostics](#controller-diagnostics).

## Engine contracts

| Engine contract | Implementation | Responsibility |
| --- | --- | --- |
| [`CE::Platform::DisplaySystem`](../../../projects/engine/include/cheryl/core/display/display-system-interface.h) | [`CE::Platform::GLFW::DisplaySystem`](../../../projects/modules/platform/native-glfw/include/cheryl/core/display/display-system.h) | Monitor snapshots, owned windows and active-window selection. |
| [`CE::Platform::Window`](../../../projects/engine/include/cheryl/core/display/window-interface.h) | [`CE::Platform::GLFW::Window`](../../../projects/modules/platform/native-glfw/include/cheryl/core/display/window.h) | Window sizes/modes, resizing, cursor visibility and deferred callback failures. |
| [`CE::Input::System`](../../../projects/engine/include/cheryl/core/controls/input-interface.h) | [`CE::Platform::GLFW::InputSystem`](../../../projects/modules/platform/native-glfw/include/cheryl/core/controls/input-system.h) | Device bindings and published State/Events/Text polls with capture and focus routing. Selected by `CHERYL_NATIVE_INPUT`. |

The graphics owner selects its window client API and supplies rendering and
presentation. GLFW/OpenGL context selection, buffer swapping and the native graphics
factory live in the OpenGL module. A different graphics implementation can use the
platform window/input facilities with its own API-specific setup.

The [display/window contract](../runtime/display-and-window-contract.md)
defines platform affinity, borrowed lifetimes, construction-time monitor snapshots,
size observations and scale/cursor capability limits. Framebuffer changes publish
the typed `CE::Platform::window_resized_event` channel alongside the legacy named notification;
the contract defines their ordering and failure boundary.

With native input enabled, the owner reuses a supplied `gainput::gainput`,
`gainputstatic` or `gainput` target, in that order, or adds the selected
`extern/gainput` source. Supplied compiled implementations must be compatible
static inputs owned by Native GLFW; they do not propagate another implementation
copy or upstream public headers to application consumers.
Supplied dependency targets keep their host's settings. Owned source composition
suppresses Gainput samples/tests locally, including cached `ON` choices, without
rewriting the host cache. The selected fork's `GAINPUT_ENABLE_HID` setting remains
available; its HID support brings hidapi and the platform's HID development libraries.
GLFW and enabled HIDAPI use shared runtimes independently of Cheryl's own linkage.
HIDAPI selection reuses a supplied target, a package or Linux pkg-config before
fetching the pinned source. Linux defaults to HID disabled; when enabled, its
default backend is hidraw. Windows forces HID support and needs a shared HIDAPI
DLL in addition to the OS HID APIs.

## Input lifetime and mapping

Gainput's HID state has one initialized native adapter as its process-wide owner.
The adapter claims ownership before `Init`, creates devices on its first window
attachment, and calls `Exit` at destruction. Another native adapter's initialization
fails explicitly while that owner exists, including while its callbacks are detached.
Failed ownership acquisition leaves the existing adapter intact; an exception from
`Init` releases the claim. Native manager state is private; applications must
not run an independently initialized Gainput implementation alongside this adapter.

Application bindings accept `CE::Input::KeyboardKey`, `MouseButton` and
`GamepadControl` directly, paired with this adapter's device IDs. Pointer axes
and wheel-button pulses use `CE::Input::MouseControl`. Include
`<cheryl/core/controls/button-codes.h>`; no public GLFW translation helper is
required. The [control guide](../runtime/input-state-model.md#control-identities-and-binding-tokens)
lists the families and binding examples.

Private native translation tables are generated from the Engine enums and pinned
GLFW/Gainput headers. After changing those declarations, run
`python3 tools/input/generate_controls.py` from the checkout root and commit its
output; `--check` verifies freshness and runs in CI before module builds.
The [generator](../../../tools/input/generate_controls.py) keeps semantic naming
exceptions separate from mechanically derived mappings. Consumers use the
committed implementation and need neither the generator nor upstream headers.

`InputSystem::device()` returns a copied
device snapshot and `backend_time_ms()` observes the backend clock. These APIs
replace public Gainput inheritance, manager members and SDK object access.
The inherited `gamepad_availability()` snapshot and
`CE::Input::gamepad_availability_changed_event` observe the single convenience
slot after complete poll publication. Runtime selects its Context bus; standalone
hosts call `input.publish_events_to(events.publisher())` before polling. See
[gamepad subscription and seed](../runtime/input-state-model.md#gamepad-availability)
for identity, closure and delivery rules.

Window/context-current, procedure lookup, swap interval and buffer swapping are
exported native operations used by OpenGL, so OpenGL uses Native GLFW's integration
with the shared GLFW runtime.

When HID is compiled in, the selected Gainput fork discards the HID backend's
initialization return code.
Ownership rollback covers exceptions; successful `Init` alone does not establish
HID device/notification readiness. Native HID acceptance verifies that behavior
independently of owner-policy checks.

Windows initialization receives the GLFW window's native HWND. Reattachment requires
that same native window, which remains live until the adapter is destroyed; recreate
the adapter before replacing the notification window. Other platforms initialize
the HID backend without a window handle and can reattach to a different live GLFW
window. Detachment clears window state while retaining devices and ownership.
Each `Update` receives elapsed steady-clock seconds between attached polls; time
spent detached does not enter that interval.

Reusable controller automation with HID disabled is in the
[Desktop and controller checks](../development/native-desktop-checks.md#linux-controller-automation);
Windows acceptance is shelved in the
[platform plan](../../planning/long-term/platform-acceptance.md).

The private controller listener uses Gainput's five-argument
`OnDeviceButtonFloat` callback. It normalizes the native control ID and forwards
`newValue` as the current Cheryl axis state; the elapsed-time argument does not
scale that value. Key/mouse callbacks feed Cheryl bindings directly in their
shared observation order, including `F20`–`F25`. Those devices also retain a
private Gainput state mirror without replaying its notifications into bindings.

Target selection and standalone paths are in
[the module guide](../development/modules.md).

## Linux controller mapping

The selected Gainput Linux joystick backend opens `/dev/input/js0` for the first
pad. Existing named PS3 and Xbox 360 dialects retain their mappings. Other
controllers use `JSIOCGBTNMAP` and `JSIOCGAXMAP` to translate driver event slots into
gamepad controls following the
[Linux gamepad specification](https://docs.kernel.org/input/gamepad.html).
DualSense Cross maps from `BTN_SOUTH` through Gainput's `PadButtonA` to
`CE::Input::GamepadControl::A`; right-stick and trigger axes
use their kernel codes, and hat axes produce directional button state. Unsupported
codes are ignored. If mapping ioctls fail, the previous raw-axis fallback remains;
button support is unavailable for an unrecognized dialect.

Translated reports update the created pad's retained state and emit its actual
logical device ID. Disconnect clears held buttons/axes and closes the descriptor;
reopening the joystick reloads its mappings. This path requires a readable joydev
node and keeps the existing fixed joystick-index selection. It does not scan for a
particular physical controller or identify a reconnected device across node changes.
Use this path with `GAINPUT_ENABLE_HID=OFF` while the HID integration below
remains unresolved.

The demo binds Cross/A to the HUD press counter. Its other gamepad controls have
no gameplay bindings; their traces can verify decoding without moving the camera.
Physical controller acceptance covers a Bluetooth DualSense on Linux/X11 with HID
disabled; USB, HID and Windows controller behavior remain separate coverage.
The reusable
[joystick controller checks](../development/native-desktop-checks.md#linux-joystick-controller-checks)
exercise reports, held input, reconnection and startup attachment in both runtimes.

## Controller backend limits

The pinned Gainput fork's CMake HID selection includes decoder sources and hidapi,
but does not supply the compiler definition `GAINPUT_ENABLE_HID` that guards the
manager's HID initialization and polling. The existing Linux native build's compile
rules omit that definition, so those runtime calls are compiled out. Enabling the
CMake option alone does not establish an active HID path.

Both PS4 and PS5 HID parsers emit listener deltas under Gainput's fixed
`CONTROLLER_ID` (`4`). This is a logical Gainput device ID, not a DualSense hardware
identifier. The native adapter's normal keyboard/mouse/pad creation assigns the pad
ID `2`. The parsers also do not update that pad's retained state or establish its
availability through the HID connection. These remain integration defects when the
HID runtime is enabled.

Cheryl uses the IDs returned by Gainput's device creation API and queries the pad's
availability/current state after each update to reconcile held controls and
disconnection. Repairing the feature wiring, report identity and retained pad state
belongs in the Gainput fork; Cheryl's acceptance must verify that integration.
Controller acceptance also requires disconnect clearing and one selected report
source for each controller.
HID device visibility and access also require observation in the user's desktop
session; successful Linux joystick reads do not establish HID access.
The [Gainput handoff](../../../extern/gainput/TODO.md) owns the deferred backend
correction and adaptive-trigger sequence. The
[HID integration](../../planning/long-term/README.md#deferred-hid-integration)
tracks Cheryl's remaining lifetime and observation requirements.

### HID capability and platform scope

The shared HID layer whitelists DualShock 4, DualShock 4 Slim and DualSense; it does
not provide a generic decoder for every HID controller. Its PS4/PS5 parsers contain
USB and Bluetooth report decoding, including ordinary controls, touch contacts and
motion sensors. Source presence does not establish usable Gainput pad support:

| Capability | Current integration limit |
| --- | --- |
| Buttons, sticks and triggers | Require the report identity, retained state and availability bridge described above. |
| Touch and motion | Use the same unfinished bridge. Both parsers send each contact's X and Y to the same axis ID; touch routing needs correction before acceptance. |
| Conventional rumble and lights | HID output packet code exists, but the normal pad API's HID forwarding is commented out. Device selection and rumble duration handling need correction. |
| Battery and adaptive triggers | DualSense battery information remains private parser data; adaptive-trigger output is an unimplemented disabled stub. Neither is an exposed feature contract. |

The platform fallbacks also depend on this integration. Windows DirectInput removes
whitelisted Sony controllers on the assumption that HID handles them. The macOS
native pad implementation maps selected Xbox controllers and leaves PlayStation
controllers to HID, while the Apple CMake branch omits the shared HID runtime and
parser sources. These are source-level gaps; Windows and macOS controller behavior
requires platform acceptance. The Linux joystick correction does not establish
coverage on either platform.

For direct Bluetooth reports on Linux, select hidapi's hidraw backend; its libusb
backend supports USB only, as described in the
[hidapi documentation](https://github.com/libusb/hidapi#about). The working joystick
route already uses kernel-decoded Bluetooth input. Direct HID is also not the only
route to advanced Linux features: the
[Sony kernel driver](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-playstation.c)
exposes motion/touch input, conventional rumble, battery and light controls through
Linux interfaces that this joystick adapter does not consume.

Cheryl's portable input interface currently exposes control input without rumble,
light or battery APIs. Select the desired feature scope before extending that
consumer contract; repairing Gainput alone does not expose those features to games.

## Controller diagnostics

`InputSystem::set_gamepad_diagnostics(true)` opts in on the platform owner before
startup or between polls. TRACE must be compiled and admitted by the OS-platform
logger and a destination. The demo's `--input-diagnostics` configures its file/logger
gates before creating the context and enables the adapter's trace. Normal mapping,
device IDs and state reconciliation are unchanged.

Records go to `logs/os-platform.log`, relative to the launch directory:

| Operation | Evidence |
| --- | --- |
| `gainput_init` | DEBUG entry/return around `InputManager::Init`; no HID readiness assertion. |
| `gamepad_setup` | DEBUG assigned pad ID, the public header's HID report ID when available, and trace selection. |
| `gainput_delta` | Opt-in TRACE callback device/control IDs and old/new values for pads or unresolved device IDs; known keyboard/mouse devices are omitted. |
| `gamepad_sample` | Opt-in TRACE pad values that change during the adapter's full-state reconciliation. |
| `gamepad_poll` | Opt-in TRACE availability, device state, A-button validity and sampled value, at most once per second. |

Callback records use the mapper's diagnostic domain; setup/sample records use the
adapter's domain. Match device IDs across these stages. An unresolved callback ID
matching `hid_report_device` identifies a routing mismatch candidate; it does not
identify a physical controller or independently establish HID report delivery.
Axis callbacks alone do not establish button delivery. Cross/A needs a button
delta and sampled transition for `PadButtonA` on the assigned pad, followed by the
demo counter. An available pad or valid button ID alone does not establish decoding.

Inspect the compiler command for Gainput's own `GainputInputManager.cpp`, not the
adapter's compile definitions, when checking the HID guard. A returned initialization
call or continuing poll heartbeat cannot establish enumeration, device-open success
or notification registration. Those observations still need hooks in Gainput.
The [Desktop and controller checks](../development/native-desktop-checks.md#linux-controller-automation)
supplies build inspection and Bluetooth capture procedures; HID observations
remain blocked in the [testing queue](../../testing-requests.md).

## Linux shutdown leak reports

Native GLFW's diagnostics owner supplies a weak `__lsan_default_suppressions`
hook on Linux GCC/Clang builds. LeakSanitizer uses it to filter known external
Fontconfig/GTK desktop-module stacks and the once-only EGL allocation matched by
`_glfwInitEGL`. These defaults address dependency reports after window teardown;
they do not free allocations or establish that application leaks are absent.
Inspect the report's allocation stack before attributing another leak to them.

An application can supply its own strong hook to replace those defaults. To
restore unfiltered reporting, define this once in an application `.cpp` file:

```cpp
extern "C" const char* __lsan_default_suppressions() { return ""; }
```

Use this on the Linux LeakSanitizer path; other platforms/toolchains have no such
module default. The [diagnostics implementation](../../../projects/modules/platform/native-glfw/src/core/display/glfw-diagnostics.cpp)
owns the exact suppression patterns.

## Checks

| Target | Output / kind | Selection / coverage |
| --- | --- | --- |
| `tests-native-glfw` | `tests-native-glfw` | `CHERYL_BUILD_TESTS`: input ownership policy, mapping and callback diagnostics. |
| `tests-native-joystick` | `tests-native-joystick` | Linux GNU/Clang, owned static Gainput, HID disabled: synthetic kernel maps, button actions, sticks/hats, disconnect/reconnect and legacy dialects. |
| `all-native-glfw` | `tests-all-native-glfw` | Ordinary owner GoogleTests; `CHERYL_BUILD_ALL_TESTS` adds it to the default build/CTest. |
| `consumer-module-native-glfw` | `cheryl-native-glfw-consumer` | `CHERYL_BUILD_CONSUMER_TESTS`: independent link/implementation consumer. |
| `consumer-module-headers-native-glfw` | Object library | Consumer's first-include header probes; built with the consumer. |

The joystick runner uses private syscall wrapping and stays separate from owner
aggregates. It requires no physical joystick/display and does not accept HID
startup, device permissions or physical reconnect behavior.

Native graphics runtime checks belong to the OpenGL owner's opt-in acceptance
suite. Use the
[composition procedures](../development/architecture-validation.md)
when changing module boundaries.
