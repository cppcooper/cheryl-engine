# Testing requests

This is the pending user-run acceptance queue for implemented work. Requests combine
coverage from multiple development cycles. Remove accepted requests and retain only
their unresolved platforms or cases; the [roadmap](planning/develop-review-and-development-plan.md)
owns development progress, while subject guides retain reusable procedures and
meaningful coverage limits.

| Request | Type | Platform | Status |
| --- | --- | --- | --- |
| [TR6](#tr6-qa-hid-lifecycle-and-notification-observations) | QA | Linux/X11 | Deferred; blocked on backend work and an observation harness |
| [TR9](#tr9-qa-unicode-text-rendering) | QA | Linux/X11 | Ready; reuse the accepted native build; requires a display |
| [TR11](#tr11-qa-native-audio-and-streaming) | QA | Linux | Ready; reuse the accepted audio build; requires audible stereo output |
| [TR12](#tr12-qa-demo-tiles-and-sprites) | QA | Linux/X11 | Ready; reuse the accepted native build; artwork checks require the three sample images |

Each command block locates the checkout root with Git and runs there, so it can
be launched from `docs/` or any other directory inside this checkout. It restores
the starting directory afterward. The runtime QA requests reuse executables from
the accepted Linux Release selections. Reusable configuration and regression
procedures remain in the
[Engine guide](development/architecture-validation.md#engine-asset-and-text-regressions),
[controller guide](development/native-desktop-checks.md#linux-controller-automation)
and [audio guide](../projects/modules/audio/miniaudio/README.md#repeating-root-assembly-validation).
Builtin demo text has an embedded fallback and needs no installed font. Display,
audio output, artwork and Python requirements are specified by each QA request.
Windows, macOS, Wayland and other platform acceptance are deferred to the
[long-term platform plan](planning/platform-acceptance.md); TR3–TR5 and the Windows
portion of TR6 are retained there, outside this active queue.

Reuse these build directories when the source, compiler and configuration match.
Stop on a command failure. A skipped or unavailable observation leaves that
coverage pending. Report the request ID, tested revision, platform, failures and skips;
successful automation does not establish the separate QA observations.

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
[deferred platform plan](planning/platform-acceptance.md#deferred-hid-lifecycle-and-windows-notifications).

## TR9: QA Unicode text rendering

**Platform/prerequisites:** Linux/GLFW/X11/OpenGL, a usable display/driver, and the
accepted `build/testing-native-linux/demo` with both UI adapters and HID disabled.
The checked-in shaders/font fixture and embedded font are available; no controller,
installed font, full image tree, clipboard or IME service is required. Close each run
before the next. Reuse that matching build.
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

**Platform:** Linux/GLFW/X11/OpenGL. **Readiness:** ready with the accepted native
demo build; reuse it with both UI adapters and HID disabled. **Prerequisites:** a
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
The temporary roots are removed after the launches; no rebuild is needed.

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
  [sampling follow-on](planning/long-term-plan.md#other-engine-extensions).
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
[platform plan](planning/platform-acceptance.md).
