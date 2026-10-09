# Deferred platform acceptance

HID acceptance on Linux and Windows, plus Windows, macOS, Wayland and other
platform acceptance, are long-term work. These procedures preserve unresolved
coverage; they are outside the active
[testing queue](../../testing-requests.md). Resume them only when platform testing is
scheduled, reconcile the current source/targets/configuration first, then restore
ready requests to that queue. Linux/X11 is the current short-term platform.

## Scope and prerequisites

Windows acceptance includes native input ownership and consumer/header probes,
typed-event and resize delivery, desktop resize, controller reports and HID
notification lifetime. TR3–TR5 below retain their request IDs for later activation.
They are **deferred**, and Windows dependency/toolchain configuration remains
unaccepted. A configuration/build failure is an unresolved prerequisite, not an
accepted or skipped case.

Commands locate the checkout root with Git and preserve the caller's starting
directory. Use a Windows Developer PowerShell with Ninja, CMake 3.28 or newer,
a C++23 toolchain and initialized pinned submodules; native builds also need OpenGL
and Python with Jinja2. Engine text also requires FreeType, HarfBuzz and ICU uc/i18n;
the demo now has a builtin font fallback. Reuse a matching
build directory when its compiler/configuration agree. Builds and CTest use normal
priority and available parallelism. Stop on failures and require nonempty test
selection without skips. Record revision, configuration, platform and coverage;
automation does not establish separate desktop or physical-controller observations.

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
  if ($LASTEXITCODE -ne 0) { throw "Cheryl command failed; stop before dependent checks." }
  cmake --build build/testing-native-windows --parallel ([Environment]::ProcessorCount) --target `
    consumer-cengine consumer-module-native-glfw tests-engine acceptance-engine `
    tests-native-glfw acceptance-opengl demo
  if ($LASTEXITCODE -ne 0) { throw "Cheryl command failed; stop before dependent checks." }
  ./build/testing-native-windows/cheryl-consumer.exe
  if ($LASTEXITCODE -ne 0) { throw "Cheryl command failed; stop before dependent checks." }
  ./build/testing-native-windows/cheryl-native-glfw-consumer.exe
  if ($LASTEXITCODE -ne 0) { throw "Cheryl command failed; stop before dependent checks." }
  ctest --test-dir build/testing-native-windows --parallel ([Environment]::ProcessorCount) --output-on-failure `
    --no-tests=error -R '^tests-engine\.(tile_animation|typed_events)\.'
  if ($LASTEXITCODE -ne 0) { throw "Cheryl command failed; stop before dependent checks." }
  ctest --test-dir build/testing-native-windows --parallel ([Environment]::ProcessorCount) --output-on-failure `
    --no-tests=error -R '^acceptance-engine\.(typed_events\.|event_delivery\.runtime_owner_threads$)'
  if ($LASTEXITCODE -ne 0) { throw "Cheryl command failed; stop before dependent checks." }
  ctest --test-dir build/testing-native-windows --parallel ([Environment]::ProcessorCount) --output-on-failure `
    --no-tests=error -R '^tests-native-glfw\.input_lifetime\.'
  if ($LASTEXITCODE -ne 0) { throw "Cheryl command failed; stop before dependent checks." }
  $env:CHERYL_NATIVE_GL_TESTS = '1'
  ctest --test-dir build/testing-native-windows --parallel ([Environment]::ProcessorCount) --output-on-failure `
    --no-tests=error `
    -R '^acceptance-opengl\.native_opengl\.(input_owner|input_window|input_reattach|resize_events|typed_resize_failure|resize_callback_failure)$'
  if ($LASTEXITCODE -ne 0) { throw "Cheryl command failed; stop before dependent checks." }
  Remove-Item Env:CHERYL_NATIVE_GL_TESTS
} finally {
  Pop-Location
}
```

A configuration/build failure is an unresolved prerequisite; report it before
attempting the dependent QA. Passing device-free polls does not accept the HID
lifecycle observations below.

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
  if ($LASTEXITCODE -ne 0) { throw "Cheryl command failed; stop before dependent checks." }
  ./build/testing-native-windows/demo.exe --concurrent
  if ($LASTEXITCODE -ne 0) { throw "Cheryl command failed; stop before dependent checks." }
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
The reusable [desktop checks](../../development/native-desktop-checks.md) retain the
Linux resize procedure.

## TR5: QA controller reports and reconnection

Verify actual controller reports through the demo rather than relying on a
device-free native poll. Use the Windows demo built by TR3, with a controller
supported by the selected Gainput backend and permission to access its device.
Launch both runtime modes with the block in [TR4](#tr4-qa-desktop-resize).

- Launch the demo normally, then repeat with `--concurrent`. Record OS, controller
  model and wired/wireless connection type.
- Press/release gamepad A (Cross on a DualSense) several times. The HUD's `Gamepad A`
  press counter advances once per press. Holding it for two seconds adds one press;
  releasing it and waiting adds none.
- Disconnect while holding Cross or a stick off center, then reconnect while the
  demo runs. Rendering and keyboard/mouse input stay responsive, and subsequent A
  presses are observed again.
- Close and relaunch the demo with the controller attached. Reports continue and
  shutdown does not hang or crash. Also launch with it disconnected, then connect
  it while the demo runs; A presses become observable.

Report unsupported or unreadable devices as unavailable coverage. This is a report
and reconnection smoke check: the HUD cannot distinguish HID from another backend
or establish the Windows notification route.

## Deferred HID lifecycle and Windows notifications

**TR6 · QA — Linux/X11 and Windows; deferred and blocked on backend work and an
observation harness.** Preserve the
[single initialized native-owner contract](../../../projects/modules/platform/native-glfw/README.md#input-lifetime-and-mapping).
The [HID plan](README.md#deferred-hid-integration) owns backend corrections.
Successful Gainput initialization does not prove HID readiness: HID can be compiled
out, and the dependency discards its initialization return code when enabled.
The demo counter and opt-in controller trace observe Gainput callbacks and sampled
pad state, not backend readiness or enumeration/open results.

### Linux USB and Bluetooth

The eventual Linux request requires:

- A supported physical HID controller and explicit evidence of HID report delivery
  over USB and Bluetooth, independently of keyboard/mouse or joydev input.
- Report continuity after a second owner is rejected, after detach/same-window
  reattachment, and after destruction permits a replacement owner in one process.
- Initialization outcome, enumeration/open, selected report source and
  disconnect/reconnect observations; successful initialization alone is insufficient.

Expose these observations without changing the single-owner lifetime contract.
Supply runnable setup/launch instructions before reactivating the request.

### Windows notifications

Windows needs a supported physical HID controller and a harness that observes:

- HID initialization, enumeration/open and actual HID report delivery independently
  of XInput or device-free native polls.
- Report continuity after rejecting a second owner, detach/same-window reattachment,
  and destruction followed by a replacement owner in one process.
- Registration on the original live HWND, removal/arrival notifications, rejected
  replacement-window attachment and cleanup at destruction. Fallback reconnect
  polling does not establish the notification route.

Supply runnable setup/launch instructions before reactivating Windows QA.
The Gainput handoff's shared identity and lifetime contract remains applicable;
Windows-specific dependency,
DirectInput fallback and notification corrections accompany future platform work.

## Other platform configurations

macOS needs source/dependency selection and native/HID fallback ownership resolved
before desktop and USB/Bluetooth controller acceptance. The current Apple source
selection omits the shared HID runtime/parsers. Preserve the
[native module's capability limits](../../../projects/modules/platform/native-glfw/README.md#hid-capability-and-platform-scope)
until those configurations are implemented and accepted.

Full Wayland and other operating systems need their own runtime acceptance for
input, rendering, UI coexistence, resizing and shutdown in sequential/concurrent
modes and normal/sandbox configurations. Establish machines, fixtures and native
observation requirements when activating each platform; do not infer acceptance
from Linux/X11 or software-driver execution. Reuse the
[architecture procedures](../../development/architecture-validation.md) and the
[desktop checks](../../development/native-desktop-checks.md) where applicable.
