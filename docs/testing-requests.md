# Testing requests

This is the pending user-run acceptance queue for implemented work. Requests combine
coverage from multiple development cycles. Remove accepted requests and retain only
their unresolved platforms or cases; the [roadmap](planning/develop-review-and-development-plan.md)
owns development progress, while subject guides retain reusable procedures and
meaningful coverage limits.

| Request | Type | Platform | Status |
| --- | --- | --- | --- |
| [TR6](#tr6-qa-hid-lifecycle-and-notification-observations) | QA | Linux/X11 | Blocked on an observation harness |
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
All command-line configurations explicitly select the Ninja generator.
Windows, macOS, Wayland and other platform acceptance are deferred to the
[long-term platform plan](planning/platform-acceptance.md); TR3–TR5 and the Windows
portion of TR6 are retained there, outside this active queue.

Reuse these build directories when the compiler and configuration match. Each block
batches its build targets and uses the available CPU count for builds and CTest
at normal priority, selecting only the requested cases. Stop
on a command failure. A zero-case selection or skipped case leaves that coverage
pending. Report the request ID, tested revision, platform, failures and skips;
successful automation does not establish the separate QA observations.

## TR6: QA HID lifecycle and notification observations

**Blocked:** an observation harness is needed before requesting this run. Successful
Gainput initialization does not prove HID readiness: the HID path can be compiled
out, and the dependency discards its initialization return code when enabled.
The demo's opt-in controller trace observes Gainput callbacks and sampled pad state,
not HID enumeration/open results or backend readiness.
The next development action is to expose those observations without changing the
[single-owner lifetime contract](../projects/modules/platform/native-glfw/README.md#input-lifetime-and-mapping).

The eventual QA request needs:

- A supported physical HID controller and explicit evidence of HID report delivery
  on Linux over USB and Bluetooth, independently of keyboard/mouse or joydev input.
- Report continuity after a second owner is rejected, after detach/same-window
  reattachment, and after destruction permits a replacement owner in one process.
- Initialization outcome, enumeration/open, selected report source and
  disconnect/reconnect observations; successful initialization alone is insufficient.

Keep this Linux request blocked until the harness has runnable setup/launch
instructions. Windows notification observations are in the
[deferred platform plan](planning/platform-acceptance.md#deferred-hid-lifecycle-and-windows-notifications).

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

Build the Linux joystick correction, HID lifecycle foundation, native input diagnostics
and demo, then run the mapper regression, synthetic joystick/HID suites and native
consumer. Reconfigure the existing native build with owned static Gainput, HID
disabled and both UI adapters disabled. The joystick suite supplies kernel mappings/events to the real
pad implementation and mapper, covering A presses/holds/releases, other controls,
reordered slots, d-pad hats, disconnect/reconnect and retained legacy mappings.
The HID runner compiles the real tracker, whitelist and PS4/PS5 parsers against
synthetic hidapi, udev and time fixtures. It covers exact/distinct paths, empty lists,
failed opens/initial reads, later read failure/reconnection, capacity/slot reuse,
invalid paths, initialization failure/restart, partial notification cleanup and
periodic rechecks with or without notifications. It supplies no successful input
reports and cannot accept decoding, pad association/state or physical HID behavior.
Both suites require no physical controller or display. The reusable
[Linux joystick smoke procedure](development/native-desktop-checks.md#linux-joystick-controller-checks)
covers physical reports separately.

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
    consumer-module-native-glfw tests-native-glfw tests-native-joystick tests-native-hid demo
  ./build/testing-native-linux/cheryl-native-glfw-consumer
  printf 'Native GLFW consumer passed.\n'
  ctest --test-dir build/testing-native-linux --parallel "$(nproc)" --output-on-failure \
    --no-tests=error -R '^(tests-native-glfw\.input_mapper\.|tests-native-joystick\.linux_joystick\.|tests-native-hid\.HidLifecycle\.)'
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

Acceptance: build/consumer/header checks and all selected mapper/joystick/HID lifecycle
cases pass without new warnings, failures or skips. The joystick and HID runners are
intentionally separate from owner aggregates because their syscall/transport fixtures
apply process-wide. The HID fixture declarations do not establish real hidapi/udev
ABI or platform linkage acceptance; keep that coverage with the later HID-ON work.
The exported command belongs to the owned dependency; HID macro arguments should
be absent with this explicit OFF configuration. This accepts neither HID runtime
startup nor physical controller reports/reconnection.
