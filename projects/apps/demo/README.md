# Cheryl Engine demo

The demo composes `Cheryl::Engine`, `Cheryl::NativeGLFW` and `Cheryl::OpenGL`.
Selecting `Cheryl::UI::TGUI` adds a panel on the right; selecting
`Cheryl::UI::RmlUi` adds an independent native-document view on the left. Each has
live counters, a camera-reset button, an editable field, a scrolling list,
translucent panels, a replaceable image and an edge tooltip. Either or both can
be selected.

## Setup

From the repository root, with the [dependencies](../../../README.md#dependencies)
initialized, configure the demo:

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -G Ninja
```

Build its target:

```sh
cmake --build build/release --target demo --parallel
```

Run in sequential or concurrent mode:

```sh
./build/release/demo
./build/release/demo --concurrent
```

| Target | Executable | Requirements |
| --- | --- | --- |
| `demo` | `demo` | Demo selection, OpenGL and native input enabled. |

| CMake option | Root default | Effect |
| --- | --- | --- |
| `CHERYL_BUILD_DEMO` | `ON` | Selects the demo target when its requirements are enabled. |
| `CHERYL_BUILD_NATIVE_GLFW` | `ON` | Supplies display/windows and native input. |
| `CHERYL_BUILD_OPENGL` | `ON` | Supplies graphics/context/resources; requires Native GLFW. |
| `CHERYL_NATIVE_INPUT` | `ON` | Supplies the demo's Gainput-backed input implementation. |
| `CHERYL_BUILD_UI_TGUI` | `ON` | Adds the right-hand TGUI panel. |
| `CHERYL_BUILD_UI_RMLUI` | `ON` | Adds the left-hand RmlUi view. |

Reload CMake after changing a cached selection. Disabling both adapters retains
the toolkit-free F2 text-input probe. The build supplies `CHERYL_DEMO_TGUI=1` and
`CHERYL_DEMO_RMLUI=1` for selected UI targets; `CHERYL_SOURCE_DIR` supplies the
default asset root. See the root [setup guide](../../../README.md#setup) for clone,
submodule updates, common configurations, all tests and
[compile-time macro tables](../../../README.md#compile-time-options-macros).

The builtin HUD selects optional system/application fonts and always retains an
embedded fallback; it can start without an installed system font. RmlUi keeps its
separate file-based service, using a selected file or the checked-in DejaVu Sans
fallback. TGUI uses its own embedded default font. Shaders and the small RmlUi proof document/images are checked in under the
asset root. The full manifest image tree is needed only with `--full-assets`;
those PNG files are not tracked. The [asset package catalog](../../../docs/assets/catalog.md)
lists their download sources and expected paths. The default asset root is the
checkout's `assets/`.
A positional argument selects another root:

```sh
./build/release/demo --concurrent /path/to/assets
```

## Controls

| Input | Behavior |
| --- | --- |
| WASD | Pans the camera while gameplay owns keyboard input. |
| R | Resets the camera while gameplay owns keyboard input. |
| F5 | Queues shader reload while gameplay owns keyboard input; the previous material remains until replacement succeeds. |
| F2 | Toggles TGUI text focus when its panel is visible, or the text probe when both adapters are disabled. |
| F4 | Toggles RmlUi text focus when its view is visible. |
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

## Command-line options

| Argument | Effect |
| --- | --- |
| `/path/to/assets` | Replaces the default asset root. |
| `--full-assets` | Also loads the manifest/image tree. |
| `--concurrent` | Runs simulation on its separate owner thread. |
| `--max-updates=N` | Stops after N updates; zero leaves the run interactive. |
| `--fixed` | Selects fixed-step simulation. |
| `--fixed-step-ms=N` | Selects fixed simulation and sets its step in milliseconds. |
| `--variable-interval-ms=N` | Sets the variable-update interval in milliseconds. |
| `--max-fixed-updates=N` | Limits ordinary fixed updates per scheduler turn. |
| `--variable-catch-up` | Selects fixed simulation with variable catch-up recovery. |
| `--recovery-prefix=N` | Sets the fixed-update prefix before recovery. |
| `--recovery-cap-ms=N` | Caps the recovery update's simulated duration. |
| `--input-unlimited` | Removes the finite pending-input-poll admission limit. |
| `--input-capacity=N` | Selects finite input polling with the given backlog capacity. |
| `--input-spacing-ms=N` | Sets the minimum input-poll spacing in milliseconds. |
| `--input-diagnostics` | Enables controller callback/sample TRACE records in `logs/os-platform.log`; requires compiled TRACE logging. |
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
`--concurrent` and `--text-direction=rtl`. Full Linux visual/upload acceptance is
pending in [TR9](../../../docs/testing-requests.md#tr9-qa-unicode-text-rendering).
Color emoji and wider CJK acceptance remain outside this batch. The missing-glyph
example exercises replacement rather than Chinese-language coverage.

## UI ownership and uploads

`Game` passes the neutral `DemoUiStatus` model to its selected views and receives
camera-reset actions. Each view builds its OpenGL materials on platform during
initialization, then creates its session/native objects lazily on the first
simulation update. It supplies copied logical/framebuffer dimensions and simulation
time. Complete CPU recordings go through the platform dispatcher/uploader, with
at most one replacement pending per view. Upload failure preserves the current
scene and appears in the HUD. Frame preparation appends TGUI, then RmlUi, after
the world/HUD pass; it never traverses live widgets. Teardown follows simulation
join and releases application-held widgets/elements/listeners before their toolkit
globals. Both adapters remain independent; their app views share only the status
model and generic shader assets, with distinct alpha pipelines.

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

RmlUi authors its view in [demo.rml](../../../assets/ui/demo.rml) using native RCSS.
Its panel has a 24-unit top-left inset, width of 34% bounded to 360–600 units and
height of 75% bounded to 500–800 units. Flex layout keeps the header, field and
image row at native sizes while the list fills the middle. Smaller windows can
clip the minimum-sized panel. The bottom-left tooltip opens toward the window
interior. No Cheryl layout string parser is involved.

The initial RmlUi image has red/green upper quadrants and blue/yellow lower
quadrants. Blue is translucent. The replacement rotates those colors one place;
this makes orientation, premultiplied blending and immutable replacement visible.

## Interaction checks

Repeat this sequence in normal and `--concurrent` modes, without `--max-updates`:

1. Check label/font appearance, panel transparency, overlap and clipping.
2. Click each field or use F2/F4. Type, move the caret and delete text; typing WASD
   should edit text without moving the camera. Switch between adapters while
   editing and check that only the new owner receives subsequent text. Press
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

Both adapters' controlled runtime and independent consumer/header proofs, and
their coexistence proof, are accepted. Native appearance, alpha/image orientation,
fonts, focus switching, scrolling, image replacement, resizing/clipping and shutdown
are accepted from the user's manual demo report in sequential and concurrent modes.
The changed builtin text path and font bootstrap have separate pending
[Unicode acceptance](../../../docs/testing-requests.md#tr9-qa-unicode-text-rendering);
earlier toolkit reports do not establish that new coverage.
The selected native scope is Linux/GLFW/X11/OpenGL, including the existing 125%
desktop scale. It does not establish per-window scale transitions, other platforms,
IME or physical GPU resource retirement.

Reuse accepted coverage unless related source changes require a rerun. The
[focused acceptance procedure](../../modules/ui/rmlui/README.md#acceptance-procedure)
batches RmlUi and coexistence checks in one parallel build.
