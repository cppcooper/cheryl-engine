# Testing requests

This is the pending user-run acceptance queue for implemented work. The
[status inventory](STATUS.md) owns selected work and deferred review order;
[planning catalogue](planning/README.md) indexes the owning plans. Subject guides
own contracts, reusable procedures and coverage limits.
Current requests retain real-desktop terminal checks, targeted material/artwork QA
and the first main-push SDK collection. Accepted Engine-only graph, terminal matrix,
CPU, sampling/UI and hosted build/SDK checks belong in their reusable subject
procedures, not this pending queue.

| **TR#** | **Short description / title** | **Long description (plain language)** |
| --- | --- | --- |
| [TR18](#tr18-qa-linux-debug-terminal-on-a-real-desktop) | **QA:** Real desktop terminal | Watch actual Konsole/xterm windows during normal runs, manual closure and crashes, including launches from a terminal, desktop and IDE. Confirm final output appears and closing the viewer leaves the app running. Requires a Linux desktop and matching terminal probe binaries; the Debug demo needs separate preparation. |
| [TR19](#tr19-qa-indexed-demo-materials-and-recovered-graphics) | **QA:** Demo materials and recovered assets | Run the demo with simulation/rendering together and on separate threads. Check text/images, failed shader reloads keeping the last working materials, and recovery after restoring files. Artwork checks need sample images; recovered college graphics and bitmap fonts need a showcase/font setup before visual checks can run. |
| [TR20](#tr20-automated-main-push-sdk-collection) | **Automated:** Main-push SDK collection | After an authorized main update, confirm GitHub collects the Linux and Windows precompiled SDKs only after every build/test job passes, with matching checksums and manifests. Hosted builds and SDK consumers are already accepted; this remaining collection check cannot run on a PR, develop update or manual dispatch. |

Report the request ID, tested revision, configuration, platform, failures and skips.
Keep existing IDs when requests change; never reuse retired IDs.

Reuse a build only when its source, compiler, configuration and options match;
otherwise configure it as shown. Stop on failures, require nonempty selections
and report skips separately. Skipped/unselected coverage remains unresolved.
These commands do not authorize agent execution.

## TR18: QA Linux Debug terminal on a real desktop

**QA. Platform:** Linux desktop, GNU/Clang Debug; explicit non-Debug inclusion in
Release `ON`. **Readiness:** runnable probes ready; blocked without a desktop and
Konsole/xterm. Reuse matching binaries from the accepted
[configuration matrix](site/modules/debug-terminal-linux.md#configuration-matrix),
or prepare them with that procedure. Desktop/IDE demo observations require a
matching Debug native demo too.

The raw probe covers early output, buffered exit records and file logging. `hold`
waits for Enter on launching stdin after viewer closure. `crash` deliberately uses
`_Exit(86)` and belongs outside a successful `set -e` sequence.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build/testing-terminal-Debug-ON/tests-acceptance-engine-terminal normal
  ./build/testing-terminal-Debug-ON/tests-acceptance-engine-terminal hold
  ./build/testing-terminal-Release-ON/tests-acceptance-engine-terminal normal
  ./build/testing-terminal-Release-ON/tests-acceptance-engine-terminal normal --debug-terminal
)
```

```sh
(
  cd "$(git rev-parse --show-toplevel)" || exit
  ./build/testing-terminal-Debug-ON/tests-acceptance-engine-terminal crash
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
    -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=ON -DCHERYL_DEBUG_TERMINAL_EMULATOR=auto \
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
  [desktop sequence](site/development/native-desktop-checks.md#repeatable-sequence).
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

## TR20: Automated Main-push SDK collection

**Automated. Platform:** GitHub Actions, with Ubuntu 24.04 x64 GCC 14 Release/Clang
18 Debug and Windows 2025 x64 MSVC RelWithDebInfo producer jobs.
**Readiness:** blocked until the first authorized `main` push containing the workflow.
**Prerequisites:** enabled Actions and access to the resulting run/artifacts. The
commands below need an authenticated GitHub CLI and `sha256sum`; the GitHub Actions
UI can replace run inspection and artifact download.

Hosted builds, focused tests and relocated SDK consumers are accepted within the
[CI selection and coverage limits](site/development/ci.md).
This request retains only the main-push collection gate. PR, `develop` and manual
runs intentionally skip it, so another manual dispatch cannot accept this scope.
The existing workflow configures and checks both producer matrices before collection;
no separate local build is needed for this request.

After the authorized main update, inspect its matching push run and download the
collected artifact. The block selects the latest main-push run; confirm its reported
SHA is the intended revision before accepting the result. It preserves the caller's
working directory and downloads into a fresh directory:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  sdk_run_id="$(gh run list --workflow ci.yml --branch main --event push --limit 1 \
    --json databaseId --jq '.[0].databaseId // empty')"
  if [ -z "$sdk_run_id" ]; then
    printf '%s\n' 'No main-push CI run is available yet.' >&2
    exit 1
  fi
  gh run watch "$sdk_run_id" --exit-status
  gh run view "$sdk_run_id" --json headSha,event,status,conclusion,jobs,url
  sdk_commit="$(gh run view "$sdk_run_id" --json headSha --jq '.headSha')"
  mkdir -p build
  sdk_artifacts="$(mktemp -d "$PWD/build/testing-sdk-collection.XXXXXX")"
  gh run download "$sdk_run_id" --name "cheryl-development-sdks-$sdk_commit" \
    --dir "$sdk_artifacts"
  for sdk_bundle in "$sdk_artifacts"/candidate-sdk-linux-modules "$sdk_artifacts"/candidate-sdk-windows-modules; do
    test -f "$sdk_bundle/SDK-MANIFEST.json"
    (cd "$sdk_bundle" && sha256sum --check SHA256SUMS)
  done
)
```

- Require all six matrix jobs and `Collect accepted development SDKs` to succeed.
  The collection job must wait for both complete matrices; a skipped collection is
  not acceptance.
- Confirm `cheryl-development-sdks-<commit>` contains both Linux/Windows SDK bundles,
  matching SHA256 files and SDK manifests for the intended revision/configurations.
  The workflow creates no GitHub release or demo package.
- Report the run URL, commit, configurations and collection result. Preserve the
  [hosted Windows skipped-case limits](site/development/ci.md#hosted-windows-coverage-limits)
  independently; green matrix jobs do not accept those cases or native/device QA.

## Deferred and unfinished coverage

HID backend work and TR6 remain deferred with the
[platform plan](planning/long-term/platform-acceptance.md#deferred-hid-lifecycle-and-windows-notifications),
including Windows TR3–TR5 and macOS/Wayland prerequisites. Hosted build/SDK
acceptance and TR20's remaining collection check do not activate those native/device
requests. Existing Linux audio, Unicode, controller and startup/UI acceptance limits
remain in their subject guides.

Unimplemented regressions stay in the
[font-selection plan](planning/short-term/README.md#font-selection-regressions) and
[startup/UI checklist](planning/short-term/README.md#startup-and-ui-follow-up).
Restore runnable requests when their fixtures, targets and observation paths exist.
