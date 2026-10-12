# Testing requests

This is the pending user-run acceptance queue for implemented work. The
[roadmap](planning/develop-review-and-development-plan.md) owns development
sequencing; subject guides own contracts, reusable procedures and coverage limits.
Current requests retain independent Engine-only graph verification, the optional
terminal matrix/desktop checks, targeted material/artwork QA and the first hosted
CI/SDK run. Accepted CPU, sampling/UI and native pixel checks belong in their
reusable subject procedures, not this pending queue.

Recorded Linux CI/SDK acceptance uses source `b49ffd9` and pinned assets `12f06eb`.
The subsequent assets update in `8989c3c` changes that fixture baseline; its
manifest-count mismatch is a prerequisite in TR20, not a newly accepted result.

| **TR#** | **Short description / title** | **Long description (plain language)** |
| --- | --- | --- |
| [TR15](#tr15-automated-engine-only-composition-graph) | **Automated:** Engine-only composition graph | Build an independent Engine-only Release consumer and check its compiled sources and final links. This verifies that using Engine does not pull in windowing, graphics, UI, audio or hwloc libraries. Behavior tests are already accepted; this remaining check needs Linux development dependencies but no display or device. |
| [TR17](#tr17-automated-linux-terminal-configuration-matrix) | **Automated:** Terminal configuration and output | Use fake terminal launchers to check which builds include the terminal, when it opens, and how application output and test reports survive shutdown, failed launches, manual closure and crashes. Covers Debug/Release and AUTO/ON/OFF settings without a desktop; actual windows need TR18. |
| [TR18](#tr18-qa-linux-debug-terminal-on-a-real-desktop) | **QA:** Real desktop terminal | Watch actual Konsole/xterm windows during normal runs, manual closure and crashes, including launches from a terminal, desktop and IDE. Confirm final output appears and closing the viewer leaves the app running. Requires a Linux desktop and TR17's binaries; the Debug demo needs separate preparation. |
| [TR19](#tr19-qa-indexed-demo-materials-and-recovered-graphics) | **QA:** Demo materials and recovered assets | Run the demo with simulation/rendering together and on separate threads. Check text/images, failed shader reloads keeping the last working materials, and recovery after restoring files. Artwork checks need sample images; recovered college graphics and bitmap fonts need a showcase/font setup before visual checks can run. |
| [TR20](#tr20-automated-hosted-ci-and-sdk-artifacts) | **Automated:** Hosted Linux/Windows builds and SDKs | Run the GitHub workflow on its actual Ubuntu and Windows machines, checking builds, tests and applications using the precompiled SDK. First reconcile the count test with the newer asset catalog, then check SDK collection after a successful main update. Local Linux results do not establish Windows or hosted-toolchain coverage; the workflow must first be available on GitHub. |

Report the request ID, tested revision, configuration, platform, failures and skips.
Keep existing IDs when requests change; never reuse retired IDs.

Reuse a build only when its source, compiler, configuration and options match;
otherwise configure it as shown. Stop on failures, require nonempty selections
and report skips separately. Skipped/unselected coverage remains unresolved.
These commands do not authorize agent execution.

## TR15: Automated Engine-only composition graph

**Automated. Platform:** Linux Release. **Readiness:** consumer/header targets ready;
independent graph verification pending. **Prerequisites:** initialized pinned
submodules, C++23, Ninja, FreeType, HarfBuzz and ICU. No display/device is needed.

CPU resource, graphics-manifest, shader-selection, sampling and positioned-text
behavior is accepted in the selected Linux runs. Preserve only the independent
Engine-only isolation check here: inspect the compilation inventory, target graph
and final consumer link to confirm absence of Cheryl native/OpenGL/GLAD/GLFW/Gainput,
UI/audio and hwloc dependencies. Include common Startup compilation and public
first-include probes. This consumer does not accept appearance or device operation.

Reuse a matching Engine-only Release build if its graph and options establish this
scope. Otherwise prepare one without selecting optional topology helpers:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/testing-engine-isolation -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_OPENGL=OFF \
    -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=OFF \
    -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
    -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF -DCHERYL_BUILD_DEMO=OFF \
    -DCHERYL_BUILD_WORKER_TOPOLOGY_HWLOC=OFF -DCHERYL_BUILD_WORKER_TOPOLOGY_JSON=OFF \
    -DCHERYL_BUILD_SDK=OFF -DCHERYL_BUILD_TESTS=OFF \
    -DCHERYL_BUILD_CONSUMER_TESTS=ON
  cmake --build build/testing-engine-isolation --parallel --target \
    cengine_startup consumer-cengine
  ./build/testing-engine-isolation/cheryl-consumer
  ninja -C build/testing-engine-isolation -t commands consumer-cengine
)
```

Inspect the consumer's final link command and `compile_commands.json` for the
listed exclusions; successful consumption in a full-module root cannot prove this
independent graph. Broader behavioral reruns are not part of this remaining request.

## TR17: Automated Linux terminal configuration matrix

**Automated. Platform:** Linux GNU/Clang with the POSIX/GNU APIs used by the module.
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

**QA. Platform:** Linux desktop, GNU/Clang Debug; explicit non-Debug inclusion in
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

**QA. Platform:** Linux/GLFW/X11/OpenGL, sequential and concurrent.
**Readiness:** main-material QA ready after preparing the demo below. Package rendering
is blocked without matching images. Recovered-college and FFont appearance portions
are blocked because the demo has no corresponding showcase/initialization harness.

Reuse an accepted matching `build/testing-graphics` composition; otherwise build
only the demo with both UI owners. This is preparation for QA, not a repeat of
accepted unit/native suites:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/testing-graphics -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCHERYL_LOG_PROFILE=developer \
    -DCHERYL_BUILD_NATIVE_GLFW=ON -DCHERYL_BUILD_OPENGL=ON \
    -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=OFF \
    -DCHERYL_NATIVE_INPUT=ON -DCHERYL_NATIVE_NULL_PLATFORM=OFF \
    -DGAINPUT_ENABLE_HID=OFF -DGLFW_BUILD_WAYLAND=OFF \
    -DCHERYL_BUILD_UI_TGUI=ON -DCHERYL_BUILD_UI_RMLUI=ON \
    -DCHERYL_RMLUI_TEST_FONT="$PWD/assets/fonts/DejaVuSans.ttf" \
    -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF -DCHERYL_BUILD_DEMO=ON \
    -DCHERYL_BUILD_TESTS=OFF
  cmake --build build/testing-graphics --target demo --parallel
)
```

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

## TR20: Automated Hosted CI and SDK artifacts

**Automated. Platform:** Ubuntu 24.04 x64 GCC 14 Release/Clang 18 Debug and Windows
2025 x64 MSVC RelWithDebInfo. **Readiness:** workflow/jobs implemented; hosted
execution pending and blocked by the asset fixture mismatch below.
**Prerequisites:** a pinned revision containing `ci.yml` on
GitHub, enabled Actions and permission to trigger it. The command block also needs
an authenticated GitHub CLI; its
[dispatch](https://cli.github.com/manual/gh_workflow_run) and
[run listing](https://cli.github.com/manual/gh_run_list) commands can be replaced by
the GitHub Actions UI. Manual dispatch requires the
workflow on the default branch. Main artifact collection additionally requires an
authorized main push; PR/develop/manual runs do not trigger that collection.

Root commit `8989c3c` pins assets `5603c19`, whose graphics index contains twelve
asset definitions. `asset_manifest.checked_in_manifests` still expects eleven in
[manifest.cpp](../projects/engine/tests/all-tests/src/assets/manifest.cpp).
This is a source-inspection finding, not an executed failure. Reconcile the test
with the intended catalog and accept its focused manifest selection before hosted
validation; all CI profiles include that case. Earlier Linux results used the old
pin and do not accept the new catalog.

Run all six headless/modules matrix entries from the
[CI matrix](development/building.md#github-ci-and-sdk-artifacts). The workflow's
[runner](../.github/ci/run.py) supplies Ninja CMake configuration, explicit build
targets, owner-prefix CTest selection and font fixtures; module jobs additionally
package/extract SDKs and build their installed consumers. Native OpenGL/hwloc
hardware, terminal and physical-device acceptance are outside this selection.

Once the workflow is installed and the intended revision is on `develop`, this
starts a validation run and lists its IDs/revisions for inspection. Use the GitHub
run page or `gh run watch <run-id> --exit-status` for the matching SHA:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  gh workflow run ci.yml --ref develop
  gh run list --workflow ci.yml --branch develop --limit 5 \
    --json databaseId,headSha,event,status,conclusion
)
```

- Require all selected jobs and installed SDK consumers/probes to succeed; inspect
  JUnit/skips rather than treating a green job as missing fixture/platform acceptance.
  Linux requires its supplied TrueType/CFF fixtures; absent Windows CFF coverage
  remains unaccepted and does not invalidate unrelated consumer checks.
- Confirm the Windows consumer runs with DLLs from the relocated SDK's bundled
  `dependencies/bin`, and import files contain no producer source/build paths.
- Inspect archives for compiled libraries, public/inline/template headers, CMake
  integration and dependency licenses. Windows includes native dependencies;
  Linux uses compatible system packages. No demo, executables or C/C++ translation
  units may be included.
- On the first successful main-push run, confirm the final
  `cheryl-development-sdks-<commit>` artifact appears only after both complete
  matrices pass, contains Linux/Windows bundles and matching SHA256/manifests,
  and creates no GitHub release. A successful manual run does not accept this gate.

Record run URL, commit/configuration and unresolved skips/failures. Local Linux
producer/consumer results do not replace these exact hosted jobs. Windows desktop,
controller/HID and GPU runtime behavior remain separate deferred requests.

## Deferred and unfinished coverage

HID backend work and TR6 remain deferred with the
[platform plan](planning/long-term/platform-acceptance.md#deferred-hid-lifecycle-and-windows-notifications),
including Windows TR3–TR5 and macOS/Wayland prerequisites. TR20 activates hosted
build/SDK acceptance only; it does not activate those native/device requests. Existing Linux audio, Unicode, controller and startup/UI acceptance
limits remain in their subject guides.

Unimplemented regressions stay in the
[Unicode plan](planning/short-term/unicode-text.md#progress) and
[startup/UI checklist](planning/develop-review-and-development-plan.md#startup-and-ui-follow-up).
Restore runnable requests when their fixtures, targets and observation paths exist.
