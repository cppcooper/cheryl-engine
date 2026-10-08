# Desktop smoke checks

## Repeatable sequence

Use the normal build's existing `demo` executable. Run it once with no arguments,
then repeat with `--concurrent`. Keep the run interactive by omitting
`--max-updates`; that option is only for finite automation. The automated normal
configuration uses GLFW's X11 backend with Wayland disabled.

In each mode, check these observations:

1. Text renders clearly; WASD pans the camera and R resets it. Mouse coordinates,
   click count and wheel values respond. Drag window edges/corners through larger
   and smaller sizes, then maximize and restore. The HUD and camera-target text
   remain correctly placed; rendering and WASD/R, click and wheel input remain
   responsive during resizing, without hangs or deferred native errors.
2. Press F2, type ASCII text, move the caret and use Backspace/Delete. Text focus
   prevents those keystrokes from moving the camera. Enter/Esc releases focus.
   The builtin HUD now uses the Unicode text service. Its selected multilingual,
   bidi/fallback and wrapping observations are pending separately in
   [TR9](../testing-requests.md#tr9-qa-unicode-text-rendering); this existing input
   sequence alone does not establish them. Probe editing still uses logical scalars.
3. Test shader reload using copied assets. From the repository root, run
   `cp -a assets/. /tmp/cheryl-reload-assets`, then launch
   `/path/to/demo /tmp/cheryl-reload-assets`, substituting your demo executable.
   The folder path after the executable is the positional argument: it selects the
   asset root without a flag. While the demo stays running, press Esc to leave text
   editing, then F5 with valid shaders. Reload should succeed. Append
   `this is not valid GLSL;` to the copied
   `/tmp/cheryl-reload-assets/shaders/shader2d.frag` and save. Press F5 again: the
   demo's `Reload:` status must show an error while the previous text/material
   remains visible. Remove the invalid line, save and press F5: the error clears.
   Repeat with `/path/to/demo --concurrent /tmp/cheryl-reload-assets`.
4. Close the window through its normal close control, including during repeated
   reloads. The process exits normally without hanging or printing a cleanup error.

For a failure, identify sequential/concurrent mode, the action, visible behavior,
console error and your GPU/driver if known. Screenshots are optional; a short text
report is enough to begin investigation. These observations complement the automated
context/lifetime cases in [architecture-validation.md](architecture-validation.md).

## Linux controller automation

Linux Release automation accepts the mapper and synthetic joystick regressions,
native consumer/header checks and compilation of the demo with GLFW/X11, OpenGL,
native input, owned static Gainput, HID disabled and both UI adapters disabled.
The synthetic runner supplies kernel mappings/events to the actual pad implementation,
covering controls, reordered slots, hats, disconnect/reconnect and legacy mappings.
Its syscall wrappers apply process-wide, so keep that runner separate from owner
aggregates. Automation does not establish HID readiness or physical reports.

Reuse the matching `build/testing-native-linux` directory and its demo for pending
text and artwork observations. When source or configuration requires new coverage,
this procedure batches the relevant targets and exports the owned Gainput manager's
compiler command. With HID disabled its HID macro arguments must be absent; inspect
the printed result. Physical observations use the following controller checks.

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
    -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF \
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
output = build / 'gainput-manager-command.txt'
output.write_text(command + '\n')
macro_arguments = [argument for argument in arguments if 'GAINPUT_ENABLE_HID' in argument]
print('Gainput manager HID macro arguments:', macro_arguments or '(none)')
print('Saved compiler command:', output)
PY
)
```

## Linux joystick controller checks

Use the existing `build/testing-native-linux/demo` with developer logging, native
input and GLFW's X11 backend selected, both optional UI adapters disabled, and
`GAINPUT_ENABLE_HID=OFF`. This checks the
[Linux joystick path](../../projects/modules/platform/native-glfw/README.md#linux-controller-mapping);
it does not establish direct HID report/output behavior or Windows notifications.
The physical acceptance configuration is a Bluetooth DualSense. Other controllers
and USB connections require their own observations.

Run each mode interactively and close it normally before launching the next:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build/testing-native-linux/demo --input-diagnostics
  cp logs/os-platform.log build/testing-native-linux/joystick-sequential.log
  ./build/testing-native-linux/demo --input-diagnostics --concurrent
  cp logs/os-platform.log build/testing-native-linux/joystick-concurrent.log
)
```

- Record the controller model and connection type. Exercise every face button,
  shoulder/trigger, stick and d-pad control. Translated controls appear in the trace;
  only Cross/A has a demo gameplay binding.
- Press/release Cross/A several times. The HUD counter advances once per press;
  holding it for two seconds adds one press, and releasing it adds none. Confirm
  button deltas and sampled transitions for `PadButtonA` on the assigned pad ID.
- Disconnect with a stick off center or Cross held, then reconnect while the demo
  runs. Held controls clear, keyboard/mouse input and rendering remain responsive,
  and subsequent A presses are observed again.
- Launch with the controller already connected, then launch with it disconnected
  and connect it while running. A presses work in both cases. Repeat the checks in
  the concurrent runtime and close each session without a hang or cleanup failure.

Keep the two captures when investigating regressions. Identify the mode, action and
observed behavior for a failure. Physical QA does not accept the separate automated
mapping, remapping and synthetic disconnect cases.
