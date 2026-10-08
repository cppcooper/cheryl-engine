# Testing requests

This is the pending user-run acceptance queue for implemented work. Requests combine
coverage from multiple development cycles. Remove accepted requests and retain only
their unresolved platforms or cases; the [roadmap](planning/develop-review-and-development-plan.md)
owns development progress, while subject guides retain reusable procedures and
meaningful coverage limits.

| Request | Type | Platform | Status |
| --- | --- | --- | --- |
| [TR6](#tr6-qa-hid-lifecycle-and-notification-observations) | QA | Linux/X11 | Deferred; blocked on backend work and an observation harness |
| [TR9](#tr9-qa-unicode-text-rendering) | QA | Linux/X11 | Ready with demo rebuilt for the current source; requires a display |
| [TR11](#tr11-qa-native-audio-and-streaming) | QA | Linux | Ready; reuse the accepted audio build; requires audible stereo output |
| [TR12](#tr12-qa-demo-tiles-and-sprites) | QA | Linux/X11 | Ready with demo rebuilt for the current source; artwork checks require the three sample images |
| [TR13](#tr13-automated-startup-compilation-and-existing-regressions) | Automated | Linux | Ready with Cheryl-owned dependencies; new targeted regression cases remain unimplemented |
| [TR14](#tr14-qa-startup-and-ui-interaction) | QA | Linux/X11 | Ready after TR13 builds the UI demo; requires a display |

Each command block locates the checkout root with Git and runs there, so it can
be launched from `docs/` or any other directory inside this checkout. It restores
the starting directory afterward. Reuse the Linux Release build selections, but
refresh affected executables for the current source before using earlier acceptance
as evidence. Reusable configuration and regression procedures remain in the
[Engine guide](development/architecture-validation.md#engine-asset-and-text-regressions),
[controller guide](development/native-desktop-checks.md#linux-controller-automation)
and [audio guide](../projects/modules/audio/miniaudio/README.md#repeating-root-assembly-validation).
Builtin demo text has an embedded fallback and needs no installed font. Display,
audio output, artwork and Python requirements are specified by each QA request.
Windows, macOS, Wayland and other platform acceptance are deferred to the
[long-term platform plan](planning/long-term/platform-acceptance.md); TR3–TR5 and the Windows
portion of TR6 are retained there, outside this active queue.

Reuse these build directories when the source, compiler and configuration match.
Stop on a command failure. A skipped or unavailable observation leaves that
coverage pending. Report the request ID, tested revision, platform, failures and skips;
successful automation does not establish the separate QA observations.

## Shared native demo preparation

TR9 and TR12 use the toolkit-free `build/testing-native-linux` configuration in
the [native guide](development/native-desktop-checks.md#linux-controller-automation):
Linux Release, developer logging, GLFW/X11/OpenGL, native input, HID off and both
UI adapters off. If that matching configuration already exists, update only the
demo once for both requests:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake --build build/testing-native-linux --target demo --parallel
)
```

If the directory is absent or its options differ, use the native guide's explicit
CMake configuration first; select only `demo` when preparing these QA runs.
Existing controller regression results need no rerun solely for startup/UI changes.
TR13 uses a separate UI-enabled directory so the toolkit-free F2 probe remains
available for TR9. A demo built before the startup changes does not establish
acceptance of the current implementation.

## TR6: QA HID lifecycle and notification observations

**Deferred and blocked:** Gainput backend work remains deferred at the pinned
baseline; an observation harness is needed before requesting this run. Successful
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
[deferred platform plan](planning/long-term/platform-acceptance.md#deferred-hid-lifecycle-and-windows-notifications).

## TR9: QA Unicode text rendering

**Platform/prerequisites:** Linux/GLFW/X11/OpenGL, a usable display/driver, and the
`build/testing-native-linux/demo` rebuilt for the current source, with both UI
adapters and HID disabled.
The checked-in shaders/font fixture and embedded font are available; no controller,
installed font, full image tree, clipboard or IME service is required. Close each run
before the next. Use the shared native preparation above and the font regressions
in TR13; reuse the matching refreshed build.
Press F7 to hide the new tile/sprite samples when they overlap the long text preview;
their appearance and animation belong to TR12.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build/testing-native-linux/demo --unicode-text --builtin-font
  ./build/testing-native-linux/demo --unicode-text --builtin-font --concurrent
  ./build/testing-native-linux/demo --unicode-text --builtin-font --text-direction=rtl
  ./build/testing-native-linux/demo --unicode-text --builtin-font --text-direction=rtl --concurrent
  ./build/testing-native-linux/demo --unicode-text
  ./build/testing-native-linux/demo --unicode-text --concurrent
  ./build/testing-native-linux/demo --unicode-text --builtin-font --font=assets/fonts/DejaVuSans.ttf
  env XDG_DATA_HOME="$PWD/assets" ./build/testing-native-linux/demo \
    --unicode-text --builtin-font '--font-family=DejaVu Sans'
)
```

The last run supplies a controlled family-discovery root containing the repository
fixture. Existing installed DejaVu Sans candidates can precede it; both are valid
explicit family selections. Common-family automatic runs can legitimately select
only the embedded fallback when none are installed. This establishes graceful
absence, while isolated `font_selection.families` cases establish discovery metadata
and ordering. Missing font preferences are not proof of successfully loading them.
Automatic discovery must avoid bold-flagged and heavier-than-Medium defaults;
explicit family/file preferences remain unrestricted. Report unexpectedly heavy
automatic HUD text with the selected font inventory. Isolated weight-selection
regressions still need fixtures/cases as identified in the
[Unicode plan](planning/short-term/unicode-text.md#progress).

- Inspect English, French accents and `e`+combining acute, German umlauts/`ß`, and
  Russian glyphs. Accents stay with their base and glyph masks are upright, with
  no per-byte replacements, solid rectangles or atlas bleed. In builtin-only runs,
  the Chinese missing-glyph example is one visible replacement; color/CJK rendering
  is outside acceptance.
- In automatic direction, the Hebrew-only line begins at the right of the available
  width; its letters read right to left while `123` remains left to right. The mixed
  Hebrew/English line preserves both run directions. Explicit RTL aligns the Latin
  paragraphs to the right without reversing their letters or numbers.
- Resize narrower/wider, maximize and restore while moving the mouse or entering
  text. The wrap sample and HUD follow the framebuffer width with 24-pixel margins;
  accents/whole graphemes remain together, hard breaks remain, and updates settle
  without blank text, corrupted old frames, hangs or native errors. Ordinary glyph
  overhang is separate from measured line advance; a too-wide indivisible grapheme
  can overflow and is not silently split.
- Use F2 and committed text input for supported accents/Cyrillic where keyboard
  layouts allow, then Enter/Esc to release focus. Text focus still prevents WASD
  from panning; the unfocused camera/input controls and F5 shader replacement remain
  usable. The probe edits logical scalars, so grapheme/bidi caret behavior is not
  part of this request. Static samples establish display independently of typing.
- Close during rapid resizing/text/counter changes in sequential and concurrent
  modes. Worker preparation and platform uploads settle/cancel without stale
  callbacks, deadlocks or cleanup errors. Failure-injection/retained-generation
  behavior is covered separately by accepted `text_resources.replacement` CPU checks.

Report the tested revision, launch variants, visible failures, console errors and
skips/unavailable input layouts. Finite runs and successful CPU checks do not establish
these visual observations. If using an existing toolkit-enabled build as well,
check the builtin HUD with overlapping views hidden and verify normal view startup;
toolkit text shaping/fallback remains its own service, outside this builtin scope.

## TR11: QA native audio and streaming

**Platform:** Linux, independently of X11/Wayland and graphical runtime modes.
**Readiness:** ready with the accepted audio consumer build. **Prerequisites:** a
working native audio backend/server/device and audible stereo headphones or
speakers. The consumer supplies its own 20-second WAV, longer than miniaudio's
two one-second stream pages; no recorded media or external encoder is needed.
If stereo output or a usable device is unavailable, this request is blocked for
that environment; the next action is to provide it. Offline success does not
replace these observations.

Reuse the matching `build/testing-audio` build. Run each launch, complete the checks,
then type `q` before starting the next. Device selection must name a real backend
and fail explicitly when none can initialize; `offline`/Null is not native acceptance.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build/testing-audio/cheryl-audio-miniaudio-consumer --device
  ./build/testing-audio/cheryl-audio-miniaudio-consumer --device --producers
)
```

- Listen past the initial two seconds. The generated stream alternates left/right
  once per second and cycles through 220/440/660/880 Hz; it continues for 20 seconds
  without dropouts or becoming silent when terminal input is idle. Check left/right
  with stereo output, rather than a mono speaker configuration.
- Enter `p`, wait, then `r`: the stream pauses and continues its previous sequence.
  In the producer launch, its short two-channel effect continues every two seconds
  while music is paused. Enter `e` repeatedly: three distinct overlapping effects
  finish after their handles are discarded, without cutting off music.
- Enter `v 0`, then `v 0.5`: music alone mutes/restores. Enter `m 0`, then `m 0.5`:
  all output mutes/restores while playback time continues. An invalid gain such as
  `v -1` reports a command error and preserves the previous gain/playback.
- Enter `s`, then `r`: stopped music remains stopped. Enter `x`: it restarts at
  the initial low left-channel tone rather than replaying an old cached segment.
  Leave looping off for a full 20-second playback; `t` reports `Finished` and `r`
  leaves it finished. Enter `l`, then `x`, and listen through the 20-second boundary:
  playback loops and remains `Playing` without a multi-second gap or stale segment.
- In both launches, close with `q` while music/effects are active, then relaunch.
  Shutdown joins the producer, releases the device without hanging or trailing
  playback, and reports the surviving music handle as `Closed` after destruction.
  Relaunch obtains usable output again. Audible artifact/latency observations are
  separate from the sample-level offline regressions.

Report the revision, backend, device/server, launch variants, missing observations
and errors. This baseline covers native output and sustained WAV streaming;
whole-clip FLAC/MP3 decoding is accepted in the root assembly. Native compressed-file
streaming/seek/loop, standalone and supplied-SDK variants, device hotplug and other platforms retain
their separate coverage limits in the [owner guide](../projects/modules/audio/miniaudio/README.md).

## TR12: QA demo tiles and sprites

**Platform:** Linux/GLFW/X11/OpenGL. **Readiness:** ready after the shared native
demo preparation; use it with both UI adapters and HID disabled. **Prerequisites:** a
usable display/driver and the tracked bootstrap assets. Present/partial-artwork
observations also need the three images in the
[demo sample table](../projects/apps/demo/README.md#tile-and-sprite-samples), placed
according to the [package catalog](assets/catalog.md). If those images are absent,
that portion is blocked: place them and rerun it. Missing-artwork startup remains
ready and never counts as successful rendering.

The block stages tracked assets and selected images in temporary roots, preserving
the checkout's files. Each launch is interactive: complete the relevant observations
and close it before the next. It runs sequential variable timing and concurrent
16 ms fixed timing, and exercises empty package trees both normally and with
`--full-assets`. It prints blocked artwork cases when source images are unavailable.
The temporary roots are removed after the launches; no additional rebuild is needed
after the shared preparation.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  python3 - <<'PY'
from pathlib import Path
import shutil
import subprocess
import tempfile

checkout = Path.cwd()
demo = checkout / 'build/testing-native-linux/demo'
assets = checkout / 'assets'
images = [Path('tilesets/punyworld-overworld-tileset.png'),
          Path('MiniWorldSprites/Objects/SwordShort.png'),
          Path('MiniWorldSprites/Characters/Soldiers/Melee/CyanMelee/SwordsmanCyan.png')]
tracked = subprocess.check_output(['git', 'ls-files', '-z', '--', 'assets/']).decode().split('\0')
cases = [('missing-images', []), ('missing-manifests', [])]
missing = [str(path) for path in images if not (assets / path).is_file()]
if missing:
    print('BLOCKED: present/partial/broken-image observations need:', ', '.join(missing), flush=True)
else:
    cases += [('selected-images', images), ('tiles-only', images[:1]),
              ('sprites-only', images[1:]), ('broken-weapon', images)]

with tempfile.TemporaryDirectory(prefix='cheryl-demo-assets-') as workspace:
    for name, selected in cases:
        root = Path(workspace) / name
        for file in filter(None, tracked):
            relative = Path(file).relative_to('assets')
            destination = root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(checkout / file, destination)
        for image in selected:
            destination = root / image
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(assets / image, destination)
        if name == 'missing-manifests':
            for manifest in ['atlas.json', 'punyworld-overworld.json']:
                (root / manifest).unlink()
        if name == 'broken-weapon':
            (root / images[1]).write_bytes(b'invalid PNG fixture')
        variants = [[], ['--full-assets']] if name == 'missing-images' else [[]]
        for options in variants:
            for mode in [[], ['--concurrent', '--fixed-step-ms=16']]:
                arguments = [str(demo), '--builtin-font', *mode, *options, str(root)]
                print('QA:', name, ' '.join(arguments), flush=True)
                subprocess.run(arguments, check=True)
PY
)
```

- In `selected-images`, all three asset statuses are ready despite unrelated
  package sheets being absent. Inspect four upright, labeled groups at three times
  source size, intact colors/transparent backgrounds and no joined adjacent-cell
  geometry. The current provider uses linear magnification; softened pixel edges
  reflect that policy. Report neighboring-cell color leakage separately for the
  [sampling follow-on](planning/long-term/README.md#other-engine-extensions).
  Static grass, sword and frozen Swordsman do not animate.
- Watch the three animated tiles use their respective 400/200/100 ms frame durations
  and loop. The sprite top row walks south/north/east; the bottom row walks west,
  idles south and attacks south. Walk/idle loop; attack reaches its final frame and
  holds until Space restarts it. Observe for several cycles in both runtime variants.
- Press P: all changing frames freeze. Wait, then resume: playback continues without
  jumping over the paused time. While paused, Space resets attack to its first frame
  and holds it until resume. F7 hides/shows all samples and labels while hidden
  playback keeps advancing; WASD/R moves/restores the world samples while the HUD
  stays fixed. Text focus suppresses P/Space/F7 gameplay actions.
- In missing-image and missing-manifest roots, the HUD reports skipped samples,
  diagnostics report each failed optional load without repeating every frame, and
  text/input/camera/F5/shutdown still work. `--full-assets` failure also preserves
  startup. In `tiles-only` and `sprites-only`, available groups render and animate
  while absent groups skip. In `broken-weapon`, only the weapon skips; tiles and
  Swordsman remain functional. These are separate observations from missing-image
  acceptance.
- Use F5 in both runtime variants: text and samples keep working after replacement.
  To observe failure retention, edit only a printed temporary root's `shaders/shader2d.frag`
  in another terminal to invalid GLSL, press F5, restore it from the checkout and
  press F5 again. Both old materials remain usable on failure and both recover;
  the HUD reports then clears the error. Close while animations and replacements
  are active without hangs, native resource errors or stale callbacks.

Report the revision, launch cases/modes, visible failures, stderr diagnostics and
blocked/skipped observations. This accepts the sample application, not a world/map
API or physical GPU retirement. Wider platform testing remains deferred in the
[platform plan](planning/long-term/platform-acceptance.md).

## TR13: Automated startup compilation and existing regressions

**Platform:** Linux. **Readiness:** ready to compile the current source and run existing
checks with Cheryl-owned dependencies. **Prerequisites:** the compiler/system
dependencies in the [build guide](development/building.md), initialized pinned
submodules including CLI11, TGUI and RmlUi, and the SDK sample font or an explicit
`CHERYL_RMLUI_TEST_FONT`. No display, controller, installed font or audio device is
needed for the executed consumers/CPU cases. Native QA follows in TR14.

The first selection preserves Engine-only isolation and explicitly compiles common
startup support. The second compiles the updated native demo, both sessions and the
generated RmlUi correction, then runs each focused owner/coexistence case once.
Reuse either matching directory; build the listed targets together rather than
running the same cases through owner and assembly aggregates.

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
    -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
    -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF -DCHERYL_BUILD_DEMO=OFF \
    -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON \
    -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=ON
  cmake --build build/testing-engine --parallel --target \
    cengine_startup tests-engine consumer-cengine
  ./build/testing-engine/cheryl-consumer
  ctest --test-dir build/testing-engine --parallel "$(nproc)" --output-on-failure \
    --no-tests=error -R '^tests-engine\.(font_selection|text_layout|text_resources)\.'

  cmake -S . -B build/testing-native-ui -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
    -DCHERYL_LOG_PROFILE=developer -DCHERYL_SANDBOX_BUILD=OFF \
    -DCHERYL_BUILD_NATIVE_GLFW=ON -DCHERYL_BUILD_OPENGL=ON \
    -DCHERYL_NATIVE_INPUT=ON -DCHERYL_NATIVE_NULL_PLATFORM=OFF \
    -DGAINPUT_ENABLE_HID=OFF -DGLFW_BUILD_X11=ON -DGLFW_BUILD_WAYLAND=OFF \
    -DCHERYL_BUILD_UI_TGUI=ON -DCHERYL_BUILD_UI_RMLUI=ON \
    -DCHERYL_TGUI_SOURCE="$PWD/extern/tgui" \
    -DCHERYL_RMLUI_SOURCE="$PWD/extern/rmlui" \
    -DCHERYL_RMLUI_PLACEHOLDER_FIX_VERIFIED=OFF \
    -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF -DCHERYL_BUILD_DEMO=ON \
    -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON \
    -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=OFF
  cmake --build build/testing-native-ui --parallel --target \
    demo tests-ui-tgui tests-ui-rmlui tests-ui-coexist \
    consumer-module-ui-tgui consumer-module-ui-rmlui
  ./build/testing-native-ui/cheryl-ui-tgui-consumer
  ./build/testing-native-ui/cheryl-ui-rmlui-consumer
  ctest --test-dir build/testing-native-ui --parallel "$(nproc)" --output-on-failure \
    --no-tests=error -R '^(tests-ui-(tgui|rmlui)\.|tests-ui-coexist\.)'
)
```

Confirm the Engine-only final links exclude native/graphics/toolkit owners and
plain Engine consumers do not inherit CLI11. In the native/UI compile inventory,
Core must compile the generated `rmlui-fixes/WidgetTextInput.cpp` rather than its
unguarded original. The startup libraries compile through their explicit target
and the demo; current consumer header probes do not cover their public headers.

Existing cases cover baseline font discovery/layout/resources and session routing,
editing, lifetime and coexistence. They do not directly exercise Startup's parser/
backend factory, numeric font-weight limits, placeholder hit-testing, or the new
`before_record` overload. Dedicated regressions remain unimplemented in the
[startup/UI checklist](planning/develop-review-and-development-plan.md#startup-and-ui-follow-up)
and [Unicode plan](planning/short-term/unicode-text.md#progress); a passing run does
not close those gaps. Supplied RmlUi target/package acceptance is blocked until a
corrected dependency and consuming host are available. The next action is to
provide them and check correction declarations under the
[module contract](../projects/modules/ui/rmlui/README.md#placeholder-dependency-contract),
including rejection without a declaration. Owned-source success does not establish
those supplied paths or standalone composition.

Report revision, configuration, selections, compile/link failures, case failures
and skips. Preserve TR9/TR12/TR14 native observations separately.

## TR14: QA startup and UI interaction

**Platform:** Linux/GLFW/X11/OpenGL in sequential and concurrent runtime modes.
**Readiness:** ready after TR13 builds the updated UI demo.
**Prerequisites:** a usable display/driver and both adapters with tracked demo
documents/images/font available. No package artwork or controller is required.
Use a separate mouse or disable touchpad suppression while typing when checking
simultaneous keyboard/pointer behavior. Reuse `build/testing-native-ui` and close
each interactive launch before the next.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  env -u DISPLAY -u WAYLAND_DISPLAY ./build/testing-native-ui/demo --help
  ./build/testing-native-ui/demo --window-width=1440 --window-height=900 \
    --window-title='Cheryl startup QA' --swap-interval=0 assets
  ./build/testing-native-ui/demo --concurrent --fixed-step-ms=16 --worker-count=2
  ./build/testing-native-ui/demo --concurrent --max-updates=5 --input-unlimited
)
```

- Help succeeds with no display and lists Engine, Backend and Application options,
  including window settings, workers, positional assets and font preferences.
  Independently repeat the executable with an unknown option, a missing numeric
  value, `--fixed-step-ms=0`, `--max-fixed-updates=0`, `--input-capacity=0`,
  `--worker-count=0`, `--window-width=0` and `--text-direction=invalid`. Each must
  return nonzero with a parsing/validation diagnostic before attempting GLFW.
  An excessive `--recovery-prefix` must also reject. These intentionally failing
  launches are separate from the successful `set -e` block.
- The configured title/dimensions appear, the positional asset root loads tracked
  resources, and normal and concurrent fixed runs retain input/rendering/shutdown.
  The finite launch reports five completed updates and exits normally. Help and
  finite-run success do not prove all parser precedence/runtime-policy combinations;
  dedicated Startup regressions remain pending.
- The RmlUi field begins empty with a muted placeholder. Click its left, middle
  and right, type text, clear it and click again: no crash or invalid caret/selection,
  and the hint never becomes editable content. F4 focuses the actual field even
  after using another RmlUi control; pressing it again releases the field's focus.
  List items occupy separate vertical rows, with working scrolling and clipping.
- Follow the [demo interaction sequence](../projects/apps/demo/README.md#interaction-checks)
  in both modes. Switch repeatedly between F2/F4 and pointer focus while typing,
  then use Escape and F3/F6 hide/show controls. Pending old-owner records retain
  their poll routing; subsequent polls reach the new owner. Check for lost or
  duplicated text, stale native focus, leaked WASD camera actions while editing,
  and pointer delivery to hidden views. Rapid physical input supplements rather
  than deterministically covers the missing same-batch regression cases.
- Close during active UI uploads/replacements and relaunch without cleanup errors
  or hangs. Native alpha/image orientation, resize/hit positions and retained scenes
  must still satisfy the demo sequence. These observations do not reproduce the
  unmodified upstream defect or replace the investigation's sanitizer evidence.

Report revision, launches, actions, diagnostics and unavailable observations.
Builtin Unicode appearance and package artwork retain TR9 and TR12 acceptance.
