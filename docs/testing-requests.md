# Testing requests

This is the pending user-run acceptance queue for implemented work. The
[roadmap](planning/develop-review-and-development-plan.md) owns development
sequencing; subject guides own contracts, reusable procedures and coverage limits.
These requests cover sampling/TGUI, graphics manifests 2.0, indexed materials and
the optional terminal since the accepted Linux baseline. They do not reopen
unrelated accepted audio, controller or startup/UI QA.

| TR# | Short description / title | Long description (plain language) |
| --- | --- | --- |
| [TR15](#tr15-automated-engine-only-graphics-and-resource-contracts) | Engine-only graphics and assets | Check that asset files and shader definitions load correctly, texture filtering settings are preserved, and reloading assets leaves data already in use valid. This checks Engine independently of window/rendering modules on Linux without a display; it does not show the recovered artwork. |
| [TR16](#tr16-automated-opengl-sampling-owned-shaders-and-ui-composition) | OpenGL sampling and UI | Check texture sharpness/smoothing, shader creation and the TGUI/RmlUi interface integrations, including scenes kept while images or fonts are replaced. Most checks need no desktop; the final pixel comparison needs an X11 display. Hardware without anisotropy, which improves texture filtering on angled surfaces, verifies only the fallback. |
| [TR17](#tr17-automated-linux-terminal-configuration-matrix) | Terminal configuration and output | Use fake terminal launchers to check which builds include the terminal, when it opens, and how application output and test reports survive shutdown, failed launches, manual closure and crashes. Covers Debug/Release and AUTO/ON/OFF settings without a desktop; actual windows need TR18. |
| [TR18](#tr18-qa-linux-debug-terminal-on-a-real-desktop) | Real desktop terminal | Watch actual Konsole/xterm windows during normal runs, manual closure and crashes, including launches from a terminal, desktop and IDE. Confirm final output appears and closing the viewer leaves the app running. Requires a Linux desktop and TR17's binaries; the Debug demo needs separate preparation. |
| [TR19](#tr19-qa-indexed-demo-materials-and-recovered-graphics) | Demo materials and recovered assets | Run the demo with simulation/rendering together and on separate threads. Check text/images, failed shader reloads keeping the last working materials, and recovery after restoring files. Artwork checks need sample images; recovered college graphics and bitmap fonts need a showcase/font setup before visual checks can run. |

Report the request ID, tested revision, configuration, platform, failures and skips.
Keep existing IDs when requests change; accepted and deferred historical requests
retain their original numbers.

No builds or tests were executed for this reconciliation. Reuse a build only when
its source, compiler, configuration and options match; otherwise configure it as
shown. Stop on failures, require nonempty selections and report skips separately.
Skipped/unselected coverage remains unresolved. These commands do not authorize
agent execution.

## TR15: Automated Engine-only graphics and resource contracts

**Platform:** Linux Release. **Readiness:** source/targets ready; execution pending.
**Prerequisites:** initialized assets/dependency submodules, C++23, Ninja, FreeType,
HarfBuzz and ICU. No display, native module or audio device is needed.

Accept neutral public headers, sampler values, indexed parsing/source ownership,
asset shader selection, catalogue generations/retry/failure behavior and checked-in
manifest conversions. The focused suite also preserves text/grid/submission coverage.
Inspect the generated graph/compile inventory for Engine-only isolation; successful
root composition alone cannot prove it.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/testing-engine -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
    -DCHERYL_LOG_PROFILE=developer -DCHERYL_SANDBOX_BUILD=OFF \
    -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_OPENGL=OFF \
    -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=OFF \
    -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
    -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF -DCHERYL_BUILD_DEMO=OFF \
    -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON \
    -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=OFF
  cmake --build build/testing-engine --parallel --target \
    cengine_startup tests-engine consumer-cengine
  ./build/testing-engine/cheryl-consumer
  ctest --test-dir build/testing-engine --parallel "$(nproc)" --output-on-failure \
    --no-tests=error -R '^tests-engine\.'
)
```

This does not establish recovered FFont artwork appearance. Dedicated font-weight
fixtures and Startup/batch/placeholder regressions remain unfinished in their plans.

## TR16: Automated OpenGL sampling, owned shaders and UI composition

**Platform:** Linux Release, GLFW/X11 dependencies, HID disabled.
**Readiness:** source/targets ready; execution pending. **Prerequisites:** the
[native/toolkit dependencies](development/building.md#system-dependencies), owned
pinned TGUI/RmlUi with its placeholder correction and the bundled font. Controlled
suites need no desktop; the final native case requires an X11 display/driver and
is blocked when unavailable.

Accept sampler policy/domain/mipmap/reset/retirement, owned shader construction and
cleanup, TGUI texture/font smoothing snapshots, shared pixel generations and retained
scenes. Include RmlUi/manual-material and coexistence consumers because shared
resource contracts changed. Run focused owners once, without duplicate aggregates.
The same build prepares the demo for targeted QA.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/testing-graphics -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
    -DCHERYL_LOG_PROFILE=developer -DCHERYL_SANDBOX_BUILD=OFF \
    -DCHERYL_BUILD_NATIVE_GLFW=ON -DCHERYL_BUILD_OPENGL=ON \
    -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=ON -DCHERYL_DEBUG_TERMINAL=AUTO \
    -DCHERYL_NATIVE_INPUT=ON -DCHERYL_NATIVE_NULL_PLATFORM=OFF \
    -DGAINPUT_ENABLE_HID=OFF -DGLFW_BUILD_X11=ON -DGLFW_BUILD_WAYLAND=OFF \
    -DCHERYL_BUILD_UI_TGUI=ON -DCHERYL_BUILD_UI_RMLUI=ON \
    -DCHERYL_RMLUI_TEST_FONT="$PWD/assets/fonts/DejaVuSans.ttf" \
    -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF -DCHERYL_BUILD_DEMO=ON \
    -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON \
    -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=ON
  cmake --build build/testing-graphics --parallel --target \
    tests-opengl tests-ui-tgui tests-ui-rmlui tests-ui-coexist \
    consumer-module-opengl consumer-module-ui-tgui consumer-module-ui-rmlui \
    acceptance-opengl demo
  ./build/testing-graphics/cheryl-opengl-consumer
  ./build/testing-graphics/cheryl-ui-tgui-consumer
  ./build/testing-graphics/cheryl-ui-rmlui-consumer
  ctest --test-dir build/testing-graphics --parallel "$(nproc)" --output-on-failure \
    --no-tests=error -R '^(tests-opengl|tests-ui-tgui|tests-ui-rmlui|tests-ui-coexist)\.'
  CHERYL_NATIVE_GL_TESTS=1 ctest --test-dir build/testing-graphics \
    --output-on-failure --no-tests=error \
    -R '^acceptance-opengl\.native_opengl\.texture_sampling$'
)
```

The native case compares nearest/linear minification and magnification pixels,
retained sampler state and defaults. Unsupported-anisotropy hardware establishes
fallback only. The demo has no interactive TGUI smoothing control; controlled
texture/font cases do not establish a new native nearest-font observation.

## TR17: Automated Linux terminal configuration matrix

**Platform:** Linux GNU/Clang with the POSIX/GNU APIs used by the module.
**Readiness:** raw/report probes and driver ready; execution pending.
**Prerequisites:** Engine text dependencies, owned pinned GoogleTest and Python 3
without optimization. Fake emulator launchers remove the desktop/emulator
requirement; this is controlled transport/lifetime coverage.

Accept Debug automatic inclusion/display, default Release omission, explicit
Release inclusion with runtime enable, and Debug `OFF`. Included runs cover raw
streams, C/C++/native/worker/logger output, final draining, startup rollback and
launcher reaping, manual closure, abnormal process loss, runner reporting,
help/discovery, diagnostics, captures, both death-test styles and XML. Omitted runs
establish omission/passthrough only.

Standalone composition avoids GLFW/OpenGL/UI dependencies. Reuse each matching
directory independently. The driver selects deliberate failure/crash scenarios;
those probes must not be run as ordinary standalone successful tests.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  for selection in Debug:AUTO Release:AUTO Release:ON Debug:OFF; do
    terminal_configuration="${selection%%:*}"
    terminal_inclusion="${selection#*:}"
    terminal_build="build/testing-terminal-${terminal_configuration}-${terminal_inclusion}"
    cmake -S projects/modules/platform/debug-terminal-linux -B "$terminal_build" -G Ninja \
      -DCMAKE_BUILD_TYPE="$terminal_configuration" \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
      -DCHERYL_REPOSITORY_ROOT="$PWD" -DCHERYL_LOG_PROFILE=developer \
      -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_ACCEPTANCE_TESTS=OFF \
      -DCHERYL_DEBUG_TERMINAL="$terminal_inclusion" \
      -DCHERYL_DEBUG_TERMINAL_EMULATOR=auto
    cmake --build "$terminal_build" --parallel --target \
      tests-acceptance-engine-terminal tests-acceptance-engine-terminal-reports
    python3 projects/modules/platform/debug-terminal-linux/tests/acceptance/terminal.py \
      "$terminal_build" --configuration "$terminal_configuration" \
      --inclusion "$terminal_inclusion"
  done
)
```

Inspect omitted consumers' final link commands: the implementation archive, entry
wrapper and viewer dependency must be absent. Fake launchers do not accept actual
emulator window behavior.

## TR18: QA Linux Debug terminal on a real desktop

**Platform:** Linux desktop, GNU/Clang Debug; explicit non-Debug inclusion in
Release `ON`. **Readiness:** runnable probes ready; blocked without a desktop and
Konsole/xterm. Complete the matrix first and reuse its binaries. Desktop/IDE demo
observations require a matching Debug native demo too.

The raw probe covers early output, buffered exit records and file logging. `hold`
waits for Enter on launching stdin after viewer closure. `crash` deliberately uses
`_Exit(86)` and belongs outside a successful `set -e` sequence.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build/testing-terminal-Debug-AUTO/tests-acceptance-engine-terminal normal
  ./build/testing-terminal-Debug-AUTO/tests-acceptance-engine-terminal hold
  ./build/testing-terminal-Release-ON/tests-acceptance-engine-terminal normal
  ./build/testing-terminal-Release-ON/tests-acceptance-engine-terminal normal --debug-terminal
)
```

```sh
(
  cd "$(git rev-parse --show-toplevel)" || exit
  ./build/testing-terminal-Debug-AUTO/tests-acceptance-engine-terminal crash
  terminal_status=$?
  test "$terminal_status" -eq 86
)
```

For root application composition, build once and reuse:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/testing-terminal-native-Debug -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_CXX_SCAN_FOR_MODULES=OFF -DCHERYL_LOG_PROFILE=developer \
    -DCHERYL_BUILD_NATIVE_GLFW=ON -DCHERYL_BUILD_OPENGL=ON \
    -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=ON -DCHERYL_DEBUG_TERMINAL=AUTO \
    -DCHERYL_NATIVE_INPUT=ON -DCHERYL_NATIVE_NULL_PLATFORM=OFF \
    -DGAINPUT_ENABLE_HID=OFF -DGLFW_BUILD_X11=ON -DGLFW_BUILD_WAYLAND=OFF \
    -DCHERYL_BUILD_UI_TGUI=ON -DCHERYL_BUILD_UI_RMLUI=OFF \
    -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF -DCHERYL_BUILD_TESTS=OFF \
    -DCHERYL_BUILD_CONSUMER_TESTS=OFF -DCHERYL_BUILD_DEMO=ON
  cmake --build build/testing-terminal-native-Debug --parallel --target demo
  ./build/testing-terminal-native-Debug/demo --builtin-font assets
  ./build/testing-terminal-native-Debug/demo --builtin-font --concurrent assets
)
```

- Show C stdio, C++ stream, native, worker and logger markers live; normal exit
  drains buffered/exit output and closes the viewer.
- Close during `hold`: the process stays active and inherited stderr reports the
  capture path. Enter allows shutdown; the file log contains `after-close` and exit
  records, with no hang or broken-pipe termination.
- `crash` retains available output until manual closure. Framework reports must
  finish on the original runner channel while an abnormal viewer remains open;
  controlled capture/death/XML checks remain separate.
- Release `ON` without an enable argument uses inherited streams; enabling it
  displays the viewer. Repeat Debug suppression and unavailable-emulator fallback.
- Repeat the Debug demo from a terminal, desktop and IDE, plus redirection of each
  inherited stream. Check early failure and both runtime modes. The standalone
  probe alone does not accept application composition or desktop/IDE launch.

## TR19: QA Indexed demo materials and recovered graphics

**Platform:** Linux/GLFW/X11/OpenGL, sequential and concurrent.
**Readiness:** main-material QA ready after the graphics build. Package rendering
is blocked without matching images. Recovered-college and FFont appearance portions
are blocked because the demo has no corresponding showcase/initialization harness.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build/testing-graphics/demo --builtin-font assets
  ./build/testing-graphics/demo --builtin-font --concurrent assets
  ./build/testing-graphics/demo --builtin-font --full-assets assets
  ./build/testing-graphics/demo --builtin-font --full-assets --concurrent assets
)
```

- Check main text/image orientation, transparency, topology and input in both modes.
  TGUI/RmlUi manual materials retain complete scenes through image/font replacement
  and shutdown.
- Use a copied temporary root for F5 as in the
  [desktop sequence](development/native-desktop-checks.md#repeatable-sequence).
  Successful reload, invalid/missing source or main definition must preserve the
  previous text/image pair on failure, report the error and recover after restoration.
  Earlier normal-rendering QA did not establish this migration/failure scope.
- Repeat missing/partial artwork with the repaired
  [sample procedure](../projects/apps/demo/README.md#tile-and-sprite-validation).
  A reported full-batch failure with successful startup does not accept full upload.
- Next action for recovered appearance: select a consumer/harness submitting
  Invaders/Rover/HyperMaze/Tileset and initializing FFont with cached `whitefont.png`.
  Check inferred pivots, Invaders' caller scale and font rows/banks. Parsing or byte
  preservation alone does not establish those images.

## Deferred and unfinished coverage

HID backend work and TR6 remain deferred with the
[platform plan](planning/long-term/platform-acceptance.md#deferred-hid-lifecycle-and-windows-notifications),
including Windows TR3–TR5 and macOS/Wayland prerequisites. No new platform request
is activated. Existing Linux audio, Unicode, controller and startup/UI acceptance
limits remain in their subject guides.

Unimplemented regressions stay in the
[Unicode plan](planning/short-term/unicode-text.md#progress) and
[startup/UI checklist](planning/develop-review-and-development-plan.md#startup-and-ui-follow-up).
Restore runnable requests when their fixtures, targets and observation paths exist.
