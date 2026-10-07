# Testing requests

This is the pending user-run acceptance queue for implemented work. Requests combine
coverage from multiple development cycles. Remove accepted requests and retain only
their unresolved platforms or cases; the [roadmap](planning/develop-review-and-development-plan.md)
owns development progress, while subject guides retain reusable procedures and
meaningful coverage limits.

| Request | Type | Platform | Status |
| --- | --- | --- | --- |
| [TR3](#tr3-automated-native-input-and-resize-on-windows) | Automated | Windows | Ready; platform acceptance pending |
| [TR4](#tr4-qa-desktop-resize) | QA | Windows | Ready after TR3 |
| [TR5](#tr5-qa-controller-reports-and-reconnection) | QA | Linux/X11 and Windows | Linux ready after updated TR8; Windows pending |
| [TR6](#tr6-qa-hid-lifecycle-and-notification-observations) | QA | Linux and Windows | Blocked on an observation harness |
| [TR7](#tr7-automated-tile-animation-warning-correction) | Automated | Linux | Ready; reuse the existing Engine-only build |
| [TR8](#tr8-automated-controller-diagnostic-build) | Automated | Linux/X11 | Ready; configure the existing native build with HID disabled |

Each command block locates the checkout root with Git and runs there, so it can
be launched from `docs/` or any other directory inside this checkout. It restores
the starting directory afterward. These commands require CMake 3.28 or newer,
Ninja, a C++23 toolchain and initialized pinned submodules; see
[setup and dependencies](../README.md#setup). Native builds also need OpenGL and
Python with Jinja2. Linux native builds need X11 and the selected hidapi backend's
libudev/libusb development dependencies. HID configuration can fetch pinned hidapi.
The demo needs a discoverable system font.
Windows commands use a Developer PowerShell with the compiler and Ninja available.
All command-line configurations explicitly select the Ninja generator.

Reuse these build directories when the compiler and configuration match. Each block
batches its build targets and uses the available CPU count for builds and CTest
at normal priority, selecting only the requested cases. Stop
on a command failure. A zero-case selection or skipped case leaves that coverage
pending. Report the request ID, tested revision, platform, failures and skips;
successful automation does not establish the separate QA observations.

## TR3: Automated native input and resize on Windows

Accept native input integration and resize callback cases on Windows, particularly
rejection of reattachment to a different notification window and successful
reattachment to the original live window. Include tile-animation and typed-event
regressions and consumer probes on this platform.
Use a usable Windows desktop session. This is an acceptance request for the current
Windows source, not evidence that its dependency/toolchain configuration is accepted.

```powershell
$cherylRoot = git rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0) { throw "Cannot locate the checkout root." }
Push-Location -LiteralPath $cherylRoot -ErrorAction Stop
try {
  cmake -S . -B build/testing-native-windows -G Ninja `
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 `
    -DCMAKE_CXX_SCAN_FOR_MODULES=OFF `
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST `
    -DCHERYL_LOG_PROFILE=developer -DCHERYL_SANDBOX_BUILD=OFF `
    -DCHERYL_BUILD_NATIVE_GLFW=ON -DCHERYL_BUILD_OPENGL=ON `
    -DCHERYL_NATIVE_INPUT=ON -DCHERYL_NATIVE_NULL_PLATFORM=OFF `
    -DGAINPUT_ENABLE_HID=ON `
    -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF `
    -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON `
    -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=ON `
    -DCHERYL_BUILD_DEMO=ON
  cmake --build build/testing-native-windows --parallel ([Environment]::ProcessorCount) --target `
    consumer-cengine consumer-module-native-glfw tests-engine acceptance-engine `
    tests-native-glfw acceptance-opengl demo
  ./build/testing-native-windows/cheryl-consumer.exe
  ./build/testing-native-windows/cheryl-native-glfw-consumer.exe
  ctest --test-dir build/testing-native-windows --parallel ([Environment]::ProcessorCount) --output-on-failure `
    --no-tests=error -R '^tests-engine\.(tile_animation|typed_events)\.'
  ctest --test-dir build/testing-native-windows --parallel ([Environment]::ProcessorCount) --output-on-failure `
    --no-tests=error -R '^acceptance-engine\.(typed_events\.|event_delivery\.runtime_owner_threads$)'
  ctest --test-dir build/testing-native-windows --parallel ([Environment]::ProcessorCount) --output-on-failure `
    --no-tests=error -R '^tests-native-glfw\.input_lifetime\.'
  $env:CHERYL_NATIVE_GL_TESTS = '1'
  ctest --test-dir build/testing-native-windows --parallel ([Environment]::ProcessorCount) --output-on-failure `
    --no-tests=error `
    -R '^acceptance-opengl\.native_opengl\.(input_owner|input_window|input_reattach|resize_events|typed_resize_failure|resize_callback_failure)$'
  Remove-Item Env:CHERYL_NATIVE_GL_TESTS
} finally {
  Pop-Location
}
```

A configuration/build failure is an unresolved prerequisite; report it before
attempting the dependent QA. Passing device-free polls does not accept TR6.

## TR4: QA desktop resize

Verify desktop-generated resizing on Windows after the native resize bridge change.
Use the demo built by TR3, with both optional UI adapters disabled. This checks
rendering and responsiveness during actual window-manager delivery. The demo does
not display the typed/legacy callback contract; TR3 requests its automated coverage.

Launch both runtime modes. Close the first session to start the concurrent session.
Use this Windows launch block for TR5 as well.

```powershell
$cherylRoot = git rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0) { throw "Cannot locate the checkout root." }
Push-Location -LiteralPath $cherylRoot -ErrorAction Stop
try {
  ./build/testing-native-windows/demo.exe
  ./build/testing-native-windows/demo.exe --concurrent
} finally {
  Pop-Location
}
```

- Keep each session interactive by omitting `--max-updates`.
- Drag edges/corners through several larger and smaller sizes, then maximize and
  restore. The HUD and camera-target text remain placed correctly and rendering
  resumes normally after each change.
- While resizing, use WASD/R, click and scroll. Camera motion/reset and HUD mouse,
  click and wheel observations remain responsive; the session does not hang or exit
  with a deferred native error.
- Close the window while updates are active in both modes. Shutdown completes
  normally. Report the platform and mode for any failure.

This request does not establish per-window scale transitions or GPU reset recovery.
The reusable [desktop checks](development/native-desktop-checks.md) retain the
Linux resize procedure.

## TR5: QA controller reports and reconnection

Verify actual controller reports through the demo rather than relying on a
device-free native poll. Use the Linux joystick demo rebuilt by updated TR8 or the
Windows demo built by TR3, with a controller supported by the selected Gainput
backend and permission to access its device.

**Linux ready after the updated TR8 build:** captured Bluetooth traces establish
joystick axis delivery to pad `2`, but no button deltas or sampled A presses. The
local Gainput correction adds kernel button/axis translation and disconnect clearing;
verify it with the same DualSense. This acceptance selects the
[Linux joystick path](../projects/modules/platform/native-glfw/README.md#linux-controller-mapping)
with `GAINPUT_ENABLE_HID=OFF`. HID feature wiring/report integration and Windows
notifications remain separate prerequisites for TR6.

For Linux, use this launch block after TR8. For Windows,
use the launch block in [TR4](#tr4-qa-desktop-resize).

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build/testing-native-linux/demo --input-diagnostics
  cp logs/os-platform.log build/testing-native-linux/tr5-sequential.log
  ./build/testing-native-linux/demo --input-diagnostics --concurrent
  cp logs/os-platform.log build/testing-native-linux/tr5-concurrent.log
)
```

- Launch the demo normally, then repeat with `--concurrent`. Record OS, controller
  model and wired/wireless connection type.
- Press/release gamepad A (Cross on a DualSense) several times. The HUD's `Gamepad A`
  press counter advances once per press. Holding it for two seconds adds one press;
  releasing it and waiting adds none.
- On Linux, confirm Cross produces `gainput_delta kind=button` and
  `gamepad_sample kind=button` for the assigned pad and `PadButtonA`. Exercise the
  other face buttons, shoulders/triggers, sticks and d-pad; translated controls
  appear in the trace. Only Cross/A has a demo gamepad binding, so other controls
  need not move the camera or change its counter.
- Disconnect while holding Cross or a stick off center, then reconnect while the
  demo runs. Rendering and
  keyboard/mouse input stay responsive; release/zero samples clear held input and
  subsequent A presses are observed again. On Linux, retain both `tr5-*.log` files;
  close each session normally before the next launch rotates the log.
- Close and relaunch the demo with the controller attached. Reports continue and
  shutdown does not hang or crash.

Report unsupported or unreadable devices as unavailable coverage. This is a report
and reconnection smoke check: the HUD cannot distinguish HID from another backend
or establish the Windows notification route.

## TR6: QA HID lifecycle and notification observations

**Blocked:** an observation harness is needed before requesting this run. Successful
Gainput initialization does not prove HID readiness: the HID path can be compiled
out, and the dependency discards its initialization return code when enabled.
The demo's opt-in controller trace observes Gainput callbacks and sampled pad state,
not HID enumeration/open results or backend/notification registration.
The next development action is to expose those observations without changing the
[single-owner lifetime contract](../projects/modules/platform/native-glfw/README.md#input-lifetime-and-mapping).

The eventual QA request needs:

- A supported physical HID controller and explicit evidence of HID report delivery
  on Linux and Windows, independently of keyboard/mouse or Windows XInput.
- Report continuity after a second owner is rejected, after detach/same-window
  reattachment, and after destruction permits a replacement owner in one process.
- Windows notification registration on the original live HWND, observed removal/
  arrival notifications, rejected replacement-window attachment and cleanup at
  destruction. A reconnect discovered by fallback polling is insufficient.

Keep this request blocked until the harness has runnable setup/launch instructions.

## TR7: Automated tile-animation warning correction

Rebuild the changed tile-animation test source without discarded-result warnings
and run its existing exception/boundary cases. Exception assertions now explicitly
discard the returned cell with `static_cast<void>`; the public `[[nodiscard]]`
attribute and value assertions remain intact. Reuse the Engine-only build from
the completed TR1 request; its broader typed-event coverage needs no rerun for
this test-only correction.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/testing-engine -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
    -DCHERYL_LOG_PROFILE=developer -DCHERYL_SANDBOX_BUILD=OFF \
    -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_OPENGL=OFF \
    -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
    -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON \
    -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=ON \
    -DCHERYL_BUILD_DEMO=OFF
  cmake --build build/testing-engine --parallel "$(nproc)" --target \
    tests-engine
  ctest --test-dir build/testing-engine --parallel "$(nproc)" --output-on-failure \
    --no-tests=error -R '^tests-engine\.tile_animation\.'
)
```

Acceptance: the changed test source compiles without ignored-result diagnostics,
and all selected `tile_animation.*` cases pass without skips.

## TR8: Automated controller diagnostic build

Build the Linux joystick correction, native input diagnostics and demo, then run
the mapper regression, synthetic joystick suite and native consumer. Reconfigure
the existing native build with owned static Gainput, HID disabled and both UI
adapters disabled. The synthetic suite supplies kernel mappings/events to the real
pad implementation and mapper, covering A presses/holds/releases, other controls,
reordered slots, d-pad hats, disconnect/reconnect and retained legacy mappings.
It requires no physical controller or display. Physical reports remain in TR5.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/testing-native-linux -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
    -DCHERYL_LOG_PROFILE=developer -DCHERYL_SANDBOX_BUILD=OFF \
    -DCHERYL_BUILD_NATIVE_GLFW=ON -DCHERYL_BUILD_OPENGL=ON \
    -DCHERYL_NATIVE_INPUT=ON -DCHERYL_NATIVE_NULL_PLATFORM=OFF \
    -DGAINPUT_ENABLE_HID=OFF -DGLFW_BUILD_X11=ON -DGLFW_BUILD_WAYLAND=OFF \
    -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
    -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON \
    -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=ON \
    -DCHERYL_BUILD_DEMO=ON
  cmake --build build/testing-native-linux --parallel "$(nproc)" --target \
    consumer-module-native-glfw tests-native-glfw tests-native-joystick demo
  ./build/testing-native-linux/cheryl-native-glfw-consumer
  printf 'Native GLFW consumer passed.\n'
  ctest --test-dir build/testing-native-linux --parallel "$(nproc)" --output-on-failure \
    --no-tests=error -R '^(tests-native-glfw\.input_mapper\.|tests-native-joystick\.linux_joystick\.)'
  python3 - <<'PY'
import json
from pathlib import Path
import shlex

build = Path('build/testing-native-linux')
source = Path('extern/gainput/lib/source/gainput/GainputInputManager.cpp').resolve()
entries = json.loads((build / 'compile_commands.json').read_text())
matches = [entry for entry in entries
           if (Path(entry['directory']) / entry['file']).resolve() == source]
if len(matches) != 1:
    raise SystemExit('Expected one owned Gainput manager compiler command.')
entry = matches[0]
arguments = entry.get('arguments') or shlex.split(entry['command'])
command = entry.get('command') or shlex.join(arguments)
output = build / 'tr8-gainput-command.txt'
output.write_text(command + '\n')
macro_arguments = [argument for argument in arguments if 'GAINPUT_ENABLE_HID' in argument]
print('Gainput manager HID macro arguments:', macro_arguments or '(none)')
print('Saved compiler command:', output)
PY
)
```

Acceptance: build/consumer/header checks and all selected mapper/joystick cases pass
without new warnings, failures or skips. The joystick runner is intentionally
separate from owner aggregates because its syscall wrappers apply process-wide.
The exported command belongs to the owned dependency; HID macro arguments should
be absent with this explicit OFF configuration. This accepts neither HID runtime
startup nor physical controller reports/reconnection.
