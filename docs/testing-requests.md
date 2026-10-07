# Testing requests

This is the pending user-run acceptance queue for implemented work. Requests combine
coverage from multiple development cycles. Remove accepted requests and retain only
their unresolved platforms or cases; the [roadmap](planning/develop-review-and-development-plan.md)
owns development progress, while subject guides retain reusable procedures and
meaningful coverage limits.

| Request | Type | Platform | Status |
| --- | --- | --- | --- |
| [TR3](#tr3-automated-native-input-and-resize-on-windows) | Automated | Windows | Ready; platform acceptance pending |
| [TR4](#tr4-qa-desktop-resize) | QA | Linux/X11 and Windows | Ready with the Linux demo; Windows after TR3 |
| [TR5](#tr5-qa-controller-reports-and-reconnection) | QA | Linux/X11 and Windows | Ready with a supported controller |
| [TR6](#tr6-qa-hid-lifecycle-and-notification-observations) | QA | Linux and Windows | Blocked on an observation harness |
| [TR7](#tr7-automated-tile-animation-warning-correction) | Automated | Linux | Ready; reuse the existing Engine-only build |

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

Verify desktop-generated resizing after the native resize bridge change. Use the
existing Linux native demo or the Windows demo built by TR3, with both optional UI
adapters disabled. This checks rendering and responsiveness during actual
window-manager delivery. The demo does not display the typed/legacy callback
contract; its Linux automation is accepted and its Windows coverage remains in TR3.

Launch both runtime modes with the block for your platform. Close the first
session to start the concurrent session. Use these same launch blocks for TR5.

Linux:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build/testing-native-linux/demo
  ./build/testing-native-linux/demo --concurrent
)
```

Windows:

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

The existing accepted Linux UI appearance/focus checks are not requested again.
This request does not establish per-window scale transitions, Wayland or GPU reset
recovery.

## TR5: QA controller reports and reconnection

Verify actual controller reports through the demo rather than relying on a
device-free native poll. Use the same HID-enabled native build as TR4 and a controller
supported by the selected Gainput backend, with permission to access its device.

- Launch the demo normally, then repeat with `--concurrent`. Record OS, controller
  model and wired/wireless connection type.
- Press/release gamepad A several times. The HUD's `Gamepad A` press counter advances
  once per press and stops advancing when released.
- Disconnect and reconnect the controller while the demo runs. Rendering and
  keyboard/mouse input stay responsive; subsequent A presses are observed again.
- Close and relaunch the demo with the controller attached. Reports continue and
  shutdown does not hang or crash.

Report unsupported or unreadable devices as unavailable coverage. This is a report
and reconnection smoke check: the HUD cannot distinguish HID from another backend
or establish the Windows notification route.

## TR6: QA HID lifecycle and notification observations

**Blocked:** an observation harness is needed before requesting this run. Successful
Gainput initialization does not prove HID readiness because the dependency discards
the HID initialization return code. The demo provides no backend/notification trace.
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
