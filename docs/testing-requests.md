# Testing requests

This is the pending user-run acceptance queue for implemented work. Requests combine
coverage from multiple development cycles. Remove accepted requests and retain only
their unresolved platforms or cases; the [roadmap](planning/develop-review-and-development-plan.md)
owns development progress, while subject guides retain reusable procedures and
meaningful coverage limits.

| Request | Type | Platform | Status |
| --- | --- | --- | --- |
| [TR6](#tr6-qa-hid-lifecycle-and-notification-observations) | QA | Linux/X11 | Deferred; blocked on backend work and an observation harness |
| [TR9](#tr9-qa-unicode-text-rendering) | QA | Linux/X11 | Needs toolkit-free demo refresh; visual QA pending |
| [TR12](#tr12-qa-demo-tiles-and-sprites) | QA | Linux/X11 | Ready with demo rebuilt for the current source; artwork checks require the three sample images |
| [TR14](#tr14-qa-startup-and-ui-interaction) | QA | Linux/X11 | Ready with the accepted UI demo build; requires a display |

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
The accepted UI-enabled `build/testing-native-ui` directory is separate; rebuilding
it does not refresh the toolkit-free F2 probe used by TR9. A demo built before the
startup or asset-layout changes does not establish current native acceptance.

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
**Readiness:** refresh the toolkit-free demo through the shared preparation before
retrying. The previous executable requested `assets/shaders/shader2d.vert`; current
source and tracked shaders use `assets/graphics/shaders/shader2d.vert`. That failed
launch establishes no visual acceptance.
The checked-in shaders/font fixture and embedded font are available; no controller,
installed font, full image tree, clipboard or IME service is required. Close each run
before the next. Existing font/layout/resource checks are accepted; their reusable
procedure and remaining fixture limits are in the
[Engine guide](development/architecture-validation.md#engine-asset-and-text-regressions).
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
images = [Path('graphics/tilesets/punyworld-overworld-tileset.png'),
          Path('graphics/MiniWorldSprites/Objects/SwordShort.png'),
          Path('graphics/MiniWorldSprites/Characters/Soldiers/Melee/CyanMelee/SwordsmanCyan.png')]
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
            for manifest in ['graphics/MiniWorldSprites/atlas.json',
                             'graphics/tilesets/punyworld-overworld.json']:
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
  To observe failure retention, edit only a printed temporary root's `graphics/shaders/shader2d.frag`
  in another terminal to invalid GLSL, press F5, restore it from the checkout and
  press F5 again. Both old materials remain usable on failure and both recover;
  the HUD reports then clears the error. Close while animations and replacements
  are active without hangs, native resource errors or stale callbacks.

Report the revision, launch cases/modes, visible failures, stderr diagnostics and
blocked/skipped observations. This accepts the sample application, not a world/map
API or physical GPU retirement. Wider platform testing remains deferred in the
[platform plan](planning/long-term/platform-acceptance.md).

## TR14: QA startup and UI interaction

**Platform:** Linux/GLFW/X11/OpenGL in sequential and concurrent runtime modes.
**Readiness:** ready with the accepted Linux native/UI build. Refresh affected
targets through the [UI validation procedure](development/ui-adapters.md#repeating-linux-root-validation)
if source or configuration changes invalidate that build.
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
