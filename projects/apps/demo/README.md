# Cheryl Engine demo

The demo composes `Cheryl::Engine`, `Cheryl::NativeGLFW`, `Cheryl::OpenGL` and
`Cheryl::OpenGL::Startup` for shared command-line bootstrap. Root composition
also selects the optional [Linux Debug terminal](../../modules/platform/debug-terminal-linux/README.md)
where available; actual Debug defaults to a separate output viewer.
Selecting `Cheryl::UI::TGUI` adds a panel on the right; selecting
`Cheryl::UI::RmlUi` adds an independent native-document view on the left. Each has
live counters, a camera-reset button, an editable field, a scrolling list,
translucent panels, a replaceable image and an edge tooltip. Either or both can
be selected.

## Setup

From the repository root, with the [dependencies](../../../docs/development/building.md#system-dependencies)
initialized, configure the demo:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -G Ninja
)
```

Build its target:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake --build build/release --target demo --parallel
)
```

Run in sequential or concurrent mode:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build/release/demo
  ./build/release/demo --concurrent
)
```

The demo requires Native GLFW, OpenGL and native input. TGUI is selected by
default; enable `CHERYL_BUILD_UI_RMLUI=ON` for the left-hand view. Disable both
adapters for the toolkit-free F2 text probe. Reload CMake after changing cached
selections; the [build guide](../../../docs/development/building.md) owns option
defaults and dependency settings.

The builtin HUD selects optional system/application fonts and always retains an
embedded fallback; it can start without an installed system font. RmlUi keeps its
separate file-based service, using a selected file or the checked-in DejaVu Sans
fallback. TGUI uses its own embedded default font. Shaders and the small RmlUi proof document/images are checked in under the
asset root. The tile/sprite showcase attempts to load three optional package images
on every launch. Missing or broken package metadata/images skip the affected samples
and report once to stderr; the HUD, input and selected UI views continue running.
`--full-assets` also attempts the whole manifest/image tree and reports a failed
batch without aborting startup. Package PNGs are not tracked. The
[asset package catalog](../../../docs/assets/catalog.md) lists their download sources
and expected paths. The default asset root is the checkout's `assets/`.
A positional argument selects another root:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build/release/demo --concurrent /path/to/assets
)
```

## Controls

| Input | Behavior |
| --- | --- |
| WASD | Pans the camera while gameplay owns keyboard input. |
| R | Resets the camera while gameplay owns keyboard input. |
| F5 | Queues text/image shader reload while gameplay owns keyboard input; the previous material pair remains until both replacements succeed. |
| F7 | Hides/shows the tile/sprite samples and their labels while gameplay owns keyboard input. Hidden animations continue advancing. |
| P | Pauses/resumes all sample animations while gameplay owns keyboard input. |
| Space | Replays the sample's nonlooping Swordsman attack while gameplay owns keyboard input. If paused, it holds the first frame until resumed. |
| F2 | Toggles TGUI text focus when its panel is visible, or the text probe when both adapters are disabled. |
| F4 | Focuses the RmlUi text field when its view is visible; releases focus when that field already owns it. |
| Escape | Releases keyboard focus. |
| Q | Requests orderly runtime shutdown while gameplay owns keyboard input; closing the window also exits. |
| Enter | Releases focus in the toolkit-free text probe. |
| F3 | Hides/shows the TGUI panel; hiding releases its keyboard focus. |
| F6 | Hides/shows the RmlUi view; hiding releases its keyboard focus. |
| Left click | Acquires UI focus when a widget is hit; clicking outside releases it. Also updates the gameplay click counter. |
| Wheel | Scrolls the hovered list and updates the wheel counter. |
| Arrows, Home/End, Backspace/Delete | Edits focused text. |
| Tab | Uses the focused toolkit's native keyboard navigation. |
| Gamepad A | Updates the gameplay press counter independently of keyboard focus. |

Committed Unicode text reaches the focused toolkit; clipboard and IME services
remain unavailable. Both views read the same immutable records with distinct focus
targets. Clicking a panel or pressing its focus key preempts the previous keyboard
owner. Pointer delivery is selected for each visible view and does not suppress
gameplay mouse State. The demo does not add pointer capture or modal arbitration.
Focus changes affect subsequent polls. Already collected keyboard/text records
keep their original target and epoch while each view drains the complete batch.

## Command-line options

Startup groups help into Engine, Backend and Application options. Use `--help`
to show the selected options; help and invalid configuration return before a
native context is created. The
[Engine option reference](../../../docs/development/consuming-engine.md#command-line-startup)
owns `--concurrent`, timing, input polling and `--worker-count`. The
[GLFW/OpenGL reference](../../modules/graphics/opengl/README.md#command-line-startup)
owns window dimensions/title, swap interval and `--input-diagnostics`.
The demo's [option helper](src/demo-options.cpp) registers these Application options:

| Argument | Effect |
| --- | --- |
| `/path/to/assets` | Replaces the default asset root. |
| `--full-assets` | Attempts the whole manifest/image tree; failure reports to stderr and startup continues with available samples. |
| `--max-updates=N` | Stops after N updates; zero leaves the run interactive. |
| `--unicode-text` | Shows accented Latin, Cyrillic, mixed/pure RTL, missing-glyph and wrapping examples in the builtin HUD. |
| `--builtin-font` | Disables automatic installed-family selection. Without explicit font preferences, the builtin HUD uses only its embedded fallback. |
| `--font=/path/to/font.ttf` | Appends an application file to the ordered preferred font sources; repeat as needed. Invalid files fail startup. |
| `--font-family=NAME` | Appends an installed family to the same preferred order. Quote names containing spaces; unavailable families are skipped. |
| `--text-direction=auto`, `ltr` or `rtl` | Sets builtin paragraph direction. Latin/numbers keep their natural run direction; explicit RTL aligns HUD lines to the width's right edge. |

See [simulation timing](../../../docs/runtime/simulation-timing.md) and
[input polling](../../../docs/runtime/input-state-model.md) for policy constraints
and defaults. A finite run checks startup/updates/shutdown; it does not establish
visual or interactive behavior.

For controller investigation, use a developer logging build and `--input-diagnostics`.
The [native controller diagnostics](../../modules/platform/native-glfw/README.md#controller-diagnostics)
explain the records and their limits. The option changes the OS-platform logger's
file/logger gates to TRACE while retaining its console preset. It rejects builds
that stripped TRACE before starting the runtime.

## Native output terminal

On Linux GNU/Clang, root `AUTO` composition attaches the output terminal in actual
Debug builds. `--no-debug-terminal` disables display when included. Release and
other configurations omit it unless `CHERYL_DEBUG_TERMINAL=ON`; those included
non-Debug builds display only with `--debug-terminal`. Without an implementation
these options are not consumed by terminal support. Help/headless launches create
no viewer. Closing the viewer keeps the demo running; normal exit closes it and
abnormal process loss retains available output. See the
[module contract](../../modules/platform/debug-terminal-linux/README.md) and
[pending acceptance](../../../docs/testing-requests.md).

## Tile and sprite samples

Four labeled groups appear near the bottom of the world view at three times the
authored pixel size. WASD pans them with the camera; R restores the initial view.
Hide overlapping UI panels with F3/F6 when inspecting the artwork, or hide the
samples with F7 when inspecting a long Unicode HUD.

| Group | Samples | Optional image relative to the asset root |
| --- | --- | --- |
| Static tiles | A 3×2 Puny World patch using cells 0, 1, 2, 27, 28 and 29. | `graphics/textures/tilesets/punyworld-overworld-tileset.png` |
| Animated tiles | Puny World targets 309, 314 and 324, left to right, using their declared 400, 200 and 100 ms frame durations. | `graphics/textures/tilesets/punyworld-overworld-tileset.png` |
| Static sprites | A short sword and a frozen cyan Swordsman, both using cell 0. | `graphics/textures/MiniWorldSprites/Objects/SwordShort.png`; Swordsman image below |
| Animated sprites | Top row: south, north and east walk. Bottom row: west walk, south idle and south attack. Walking and idle loop; attack holds its last frame until Space replays it. | `graphics/textures/MiniWorldSprites/Characters/Soldiers/Melee/CyanMelee/SwordsmanCyan.png` |

The samples use `graphics/definitions/tilesets/punyworld-overworld.json` and
`graphics/definitions/MiniWorldSprites/atlas.json` for grids, pivots and authored clips.
Only these selected entries/images are loaded by default; unrelated
absent sheets do not suppress available samples. The HUD reports each selected
asset as ready or skipped. A failed MiniWorld manifest skips both its sprite samples,
while a failed individual image skips only that entry. No replacement package artwork
is generated. The checked-in bootstrap shaders and selected UI resources retain
their existing requirements.

Initial loading and label uploads run on the platform owner. Tile elapsed time and
independent sprite cursors advance using `TickContext::delta_seconds`; fractional
tile time accumulates before conversion to milliseconds. Pausing excludes that time
without creating a resume jump. Frame preparation resolves current cells into
resource-retaining packets and performs no image loading or playback mutation.
Indexed `main:images` (triangle strip) and `main:text` (triangles) materials share
one `shader2d` program and straight-alpha blending. Default startup and F5 select
`graphics/shaders/graphics-manifests.json`; other shader files are not automatically
indexed. The [loading contract](../../../docs/assets/asset-loading.md#resources-and-application-bootstrap)
describes incremental catalogue replacement and pair adoption. Images inherit the OpenGL provider's linear
magnification and generated mipmaps, so enlarged pixel edges can look softened.
Engine defaults also request maximum-supported anisotropy. Explicit immutable
[sampler bindings](../../../docs/rendering/pipelines-and-materials.md#immutable-sampling)
can select other filtering without mutating an image; these demo samples expose no
filtering control. Atlas padding or repacking remains separate future work.
F5 adopts both replacement handles together; a failed
replacement keeps the current pair. Teardown releases the application-held samples
and labels on the platform owner after simulation joins.

Linux/GLFW/X11/OpenGL native sample appearance, animation, pause/resume and
missing/partial-artwork behavior are accepted in sequential and concurrent modes.
This baseline supplies no fresh invalid-shader retention/recovery observation for
the two sample materials. Closing during an interactive desktop resize was also
not established because the close attempt was blocked until resizing ended; this
does not identify which component delayed input. Shader-failure and simultaneous
close/resize coverage remain separate from normal rendering and orderly shutdown.
These observations precede the indexed 2.0 material migration; affected reruns are
now in the [testing queue](../../../docs/testing-requests.md). Reusable procedures
are below in [native QA](#repeating-native-qa).

## Builtin Unicode text

The HUD and camera label use the neutral
[Unicode text service](../../../docs/assets/text-layout.md). The HUD wraps to the
framebuffer width minus two 24-pixel margins. The toolkit-free F2 probe now encodes
its scalar editing buffer as UTF-8, so supported accents/Cyrillic display as glyphs
and unknown graphemes use a visible replacement. Its caret still moves through
logical scalars; this does not establish grapheme/bidi editing or clipboard/IME.

After initial owner-thread upload, changed HUD text/width is prepared by an owned
CPU-worker request and uploaded by the platform dispatcher. At most one replacement
is preparing/uploading. Later changes coalesce into the next request; the last
complete text stays visible until publication. Failed candidates retain that text
and report the failure to stderr. Frame preparation reads only immutable uploaded
generations and never discovers fonts, shapes, rasterizes or uploads. Shutdown
settles/cancels context work before releasing application-held text/resources.

For an interactive preview, use `--unicode-text --builtin-font`, then repeat with
`--concurrent` and `--text-direction=rtl`. Linux native appearance, wrapping,
alignment and resource-replacement/shutdown observations are accepted. RTL selects
paragraph direction rather than reversing every string: Latin retains its natural
order and aligns right, while the Hebrew-only paragraph already resolves RTL in
automatic mode. The [text guide](../../../docs/assets/text-layout.md#native-acceptance-scope)
owns bidi proof and remaining coverage limits; [native QA](#unicode-preview-validation)
preserves the reusable launch variants.
Color emoji and wider CJK acceptance remain outside this batch. The missing-glyph
example exercises replacement rather than Chinese-language coverage.

## UI ownership and uploads

`Game` passes the neutral `DemoUiStatus` model to its selected views and receives
camera-reset actions. Each view keeps manual OpenGL material construction on platform during
initialization, then creates its session/native objects lazily on the first
simulation update. It supplies copied logical/framebuffer dimensions and simulation
time. Complete CPU recordings go through the platform dispatcher/uploader, with
at most one replacement pending per view. Upload failure preserves the current
scene and appears in the HUD. Frame preparation appends TGUI, then RmlUi, after
the world/HUD pass; it never traverses live widgets. Teardown follows simulation
join and releases application-held widgets/elements/listeners before their toolkit
globals. Both adapters remain independent; their app views share only the status
model and generic shader assets, with distinct alpha pipelines. The
[manual recipe example](../../../docs/rendering/pipelines-and-materials.md#manual-material-construction)
explains definition/binding wiring. A future indexed UI migration targets TGUI
first; RmlUi retains a separate manual example if it later migrates too. Each view delivers
the complete input batch once through its session's control callback, retaining
the entry keyboard epoch while visibility controls select pointer delivery per record.

The demo uses the adapter's [typed layout](../../modules/ui/tgui/README.md#typed-layout).
The panel anchors to the window's top-right with a 24-unit inset. Its width follows
35% of the window, bounded between 394 and 640 logical units; its height follows
75%, bounded between 500 and 800 units. For example:

| Logical window size | Panel size |
| --- | --- |
| 1280 × 720 | 448 × 540 |
| 1600 × 900 | 560 × 675 |
| 1920 × 1080 | 640 × 800 |

The edit field, list viewport, list rows and image region follow their parent's
content width with fixed side margins. The image region anchors to the panel's
bottom, and the list fills the space between the fixed header and image region.
The list computes its content extent from its rows, so resizing can show or hide
the scrollbar without a fixed content width clipping wider rows. Fonts and control
heights keep their native sizes; a window narrower than 418 or shorter than 524
units can clip the panel.

The TGUI badge anchors inside the panel beside the title using a fixed inset plus
a relative inset, and its text stays centered. The tooltip button anchors to the
window's bottom-right. Panel children are clipped to the parent's content area;
scrolling uses that same normal clipping behavior.

RmlUi authors its view in [demo.rml](../../../assets/graphics/ui/demo.rml) using native RCSS.
Its panel has a 24-unit top-left inset, width of 34% bounded to 360–600 units and
height of 75% bounded to 500–800 units. Flex layout keeps the header, field and
image row at native sizes while the list fills the middle. List items explicitly
use block layout, placing each row on its own line in the vertical scroll region.
The initially empty field displays native placeholder help; clearing an edited
value restores that help. Smaller windows can clip the minimum-sized panel.
The bottom-left tooltip opens toward the window
interior. No Cheryl layout string parser is involved.

The initial RmlUi image has red/green upper quadrants and blue/yellow lower
quadrants. Blue is translucent. The replacement rotates those colors one place;
this makes orientation, premultiplied blending and immutable replacement visible.

## Interaction checks

Repeat this sequence in normal and `--concurrent` modes, without `--max-updates`:

1. Check label/font appearance, panel transparency, overlap and clipping.
2. Click each field or use F2/F4. Type, move the caret and delete text; typing WASD
   should edit text without moving the camera. Switch between adapters while
   editing and check that subsequent polls route text to the new owner. Press
   Escape, then check camera input and Q shutdown. Gamepad A should still update
   its gameplay counter while a field owns keyboard focus.
3. Scroll the list, reset the camera and change the image. Confirm counters and
   retained UI content continue updating.
4. Hover each `?` tooltip, resize the window and, where available, move
   it between displays with different content scales. Resize in both directions
   and check the panel's width/height bounds, the badge and tooltip anchors, side
   margins around the field/list/image, list growth above the image row, scrollbar
   visibility, input hit positions and clip edges.
5. Hide/show the views with F3/F6, including while editing. Hiding releases text
   focus. Inspect the common HUD with the left view hidden if it overlaps.
6. Close the window while UI updates/uploads are active. Check orderly shutdown
   in both modes.

For simultaneous keyboard/pointer checks, use a separate mouse or disable the
desktop's touchpad "Disable while typing" setting. That setting can suppress
touchpad motion after key presses; see
[libinput's behavior](https://wayland.freedesktop.org/libinput/doc/latest/palm-detection.html#disable-while-typing).

The selected native scope is Linux/GLFW/X11/OpenGL, including the existing 125%
desktop scale. Per-window scale transitions, other platforms, IME and physical GPU resource
retirement need separate observations. Native startup, field/placeholder focus,
list layout and UI interaction are accepted in the selected Linux root composition.
Dedicated parser/factory, support-header, callback/batch and placeholder regressions
remain in the [owning plan](../../../docs/planning/develop-review-and-development-plan.md#startup-and-ui-follow-up).
Builtin Unicode keeps its separate [coverage scope](../../../docs/assets/text-layout.md#native-acceptance-scope).

Reuse accepted coverage unless related source changes require a rerun. The
[focused acceptance procedure](../../modules/ui/rmlui/README.md#acceptance-procedure)
batches RmlUi and coexistence checks in one parallel build.

## Repeating native QA

Reuse accepted coverage unless related source or configuration changes invalidate
it. Select the affected observations below; shader failure injection is a targeted
check for changes to compilation, material replacement or retention. Linux/X11
results do not establish Wayland, other platforms or physical GPU retirement.

### Native QA preparation

The Unicode and sample procedures use `build/testing-native-linux`: Linux Release,
developer logging, GLFW/X11/OpenGL, native input, HID off and both UI adapters off.
Use the [native guide's configuration](../../../docs/development/native-desktop-checks.md#linux-controller-automation)
if that directory is absent or differs; user-run CMake configuration selects Ninja.
For a matching build, refresh only the demo once for both procedures:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake --build build/testing-native-linux --target demo --parallel
)
```

The startup/UI procedure uses the separate `build/testing-native-ui` directory.
Refresh affected targets with the [UI procedure](../../../docs/development/ui-adapters.md#repeating-linux-root-validation)
when needed. Rebuilding one directory does not refresh the other. Each launch
selects `assets` as the root; the demo resolves `graphics/shaders/` beneath it.
Temporary sample roots preserve that layout. Close each interactive launch before
the next. The builtin Unicode preview needs no installed font or package artwork.

### Unicode preview validation

The sequential and concurrent variants should render the same text. Font-selection
variants can also look alike when they select the same face or embedded fallback.
Explicit RTL sets paragraph direction: Latin and digits keep their natural order,
while Latin paragraphs align right. The Hebrew-only paragraph already resolves RTL
in automatic mode, so its order need not change between automatic and explicit RTL.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build/testing-native-linux/demo --unicode-text --builtin-font assets
  ./build/testing-native-linux/demo --unicode-text --builtin-font --concurrent assets
  ./build/testing-native-linux/demo --unicode-text --builtin-font --text-direction=rtl assets
  ./build/testing-native-linux/demo --unicode-text --builtin-font --text-direction=rtl --concurrent assets
  ./build/testing-native-linux/demo --unicode-text assets
  ./build/testing-native-linux/demo --unicode-text --concurrent assets
  ./build/testing-native-linux/demo --unicode-text --builtin-font --font=assets/fonts/DejaVuSans.ttf assets
  env XDG_DATA_HOME="$PWD/assets" ./build/testing-native-linux/demo \
    --unicode-text --builtin-font '--font-family=DejaVu Sans' assets
)
```

The `XDG_DATA_HOME` launch supplies a controlled family-discovery root containing
the repository font. Installed DejaVu Sans candidates can precede it; either is a
valid explicit family selection. Automatic discovery can use only the embedded
fallback when preferred families are absent. Successful fallback alone does not
prove discovery; controlled CPU cases establish its metadata and ordering. Numeric
font-weight limits still need the [dedicated fixtures](../../../docs/planning/short-term/unicode-text.md#progress).

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
  part of this preview scope. Static samples establish display independently of typing.
- Close during rapid resizing/text/counter changes in sequential and concurrent
  modes. Worker preparation and platform uploads settle/cancel without stale
  callbacks, deadlocks or cleanup errors. Failure-injection/retained-generation
  behavior is covered separately by accepted `text_resources.replacement` CPU checks.

### Tile and sprite validation

The procedure stages tracked definitions/shaders/fonts from the assets submodule
and the three images from the sample table in
temporary roots, preserving checkout files. It covers sequential variable timing
and concurrent 16 ms fixed timing, missing metadata/images, selected/partial
artwork and a corrupt weapon image. Artwork cases are blocked when their source
images are absent; place them according to the [catalog](../../../docs/assets/catalog.md).
Missing-artwork startup does not count as rendered-artwork acceptance.

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
images = [Path('graphics/textures/tilesets/punyworld-overworld-tileset.png'),
          Path('graphics/textures/MiniWorldSprites/Objects/SwordShort.png'),
          Path('graphics/textures/MiniWorldSprites/Characters/Soldiers/Melee/CyanMelee/SwordsmanCyan.png')]
tracked = subprocess.check_output(['git', '-C', str(assets), 'ls-files', '-z']).decode().split('\0')
runtime_extensions = {'.json', '.vert', '.frag', '.ttf', '.otf', '.ttc', '.otc', '.rml', '.rcss'}
tracked = [Path(file) for file in tracked
           if file and Path(file).suffix.lower() in runtime_extensions]
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
        for relative in tracked:
            destination = root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(assets / relative, destination)
        for image in selected:
            destination = root / image
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(assets / image, destination)
        if name == 'missing-manifests':
            for manifest in ['graphics/definitions/MiniWorldSprites/atlas.json',
                             'graphics/definitions/tilesets/punyworld-overworld.json']:
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
  [sampling contract](../../../docs/rendering/pipelines-and-materials.md#immutable-sampling);
  explicit sampling does not create padded atlas cells.
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

For changes affecting shader replacement, use the
[targeted reload sequence](../../../docs/development/native-desktop-checks.md#repeatable-sequence)
with the printed temporary root's `graphics/shaders/shader2d.frag`. Both old
materials must remain usable on failure and recover after restoration; the HUD
reports then clears the error. This is separate from normal sample regression QA.

### Startup and UI validation

Use the UI-enabled build with the tracked documents/images/font and a usable
display. Package artwork and controllers are optional. The help launch requires
no display; the interactive variants check normal and concurrent fixed operation,
and the final launch exits after five updates.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  env -u DISPLAY -u WAYLAND_DISPLAY ./build/testing-native-ui/demo --help
  ./build/testing-native-ui/demo --window-width=1440 --window-height=900 \
    --window-title='Cheryl startup QA' --swap-interval=0 assets
  ./build/testing-native-ui/demo --concurrent --fixed-step-ms=16 --worker-count=2 assets
  ./build/testing-native-ui/demo --concurrent --max-updates=5 --input-unlimited assets
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
- Follow the [demo interaction sequence](#interaction-checks)
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
