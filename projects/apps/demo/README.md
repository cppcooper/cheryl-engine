# Cheryl Engine demo

The demo composes `Cheryl::Engine`, `Cheryl::NativeGLFW` and `Cheryl::OpenGL`.
Selecting `Cheryl::UI::TGUI` adds a toolkit panel with live counters, a camera-reset
button, an editable field, a scrolling list, translucent panels, a replaceable image
and an edge tooltip.

## Build and launch

From the repository root, with the [dependencies](../../../README.md#dependencies)
initialized:

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --target demo --parallel 1
./build/release/demo
./build/release/demo --concurrent
```

`demo` exists when OpenGL and native input are enabled. TGUI is enabled by default;
set `CHERYL_BUILD_UI_TGUI=ON` and reload CMake if an existing profile caches it as
`OFF`. Disabling TGUI retains the toolkit-free F2 text-input probe.

The HUD uses a discoverable system font and shaders from the asset root. TGUI uses
its embedded default font. The full image tree is needed only with `--full-assets`;
its PNG files are not tracked. The default asset root is the checkout's `assets/`.
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
| F2 | Toggles text focus; with TGUI, selects the edit box when the panel is visible. |
| Escape | Releases keyboard focus. Close the window to end the demo. |
| Enter | Releases focus in the toolkit-free text probe. |
| F3 | Hides/shows the TGUI panel; hiding releases its keyboard focus. |
| Left click | Acquires UI focus when a widget is hit; clicking outside releases it. Also updates the gameplay click counter. |
| Wheel | Scrolls the hovered list and updates the wheel counter. |
| Arrows, Home/End, Backspace/Delete | Edits focused text. |
| Tab | Navigates TGUI widgets when UI keyboard focus is held. |
| Gamepad A | Updates the gameplay press counter independently of keyboard focus. |

Committed Unicode text reaches TGUI's embedded font; clipboard and IME services
remain unavailable. Pointer delivery is selected while the panel is visible and
does not suppress gameplay mouse State. Escape means release focus throughout the
on-screen instructions.

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

See [simulation timing](../../../docs/runtime/simulation-timing.md) and
[input polling](../../../docs/runtime/input-state-model.md) for policy constraints
and defaults. A finite run checks startup/updates/shutdown; it does not establish
visual or interactive behavior.

## UI ownership and uploads

`Game` passes a neutral status model to `DemoUi` and receives a camera-reset action.
`DemoUi` builds its OpenGL materials on platform during initialization, then creates
the session/widgets lazily on the first simulation update. It feeds copied logical
and framebuffer dimensions and simulation time to the session. Complete CPU
recordings go through the existing platform dispatcher/uploader, with at most one
replacement pending. Upload failure preserves the current scene and appears in the
HUD. Frame preparation appends that retained UI scene after the world/HUD pass;
it never traverses live widgets. Teardown follows the runtime's simulation join and
releases application-held widgets before the global toolkit backend.

The demo uses the adapter's [typed layout](../../modules/ui/tgui/README.md#typed-layout).
The panel anchors to the window's top-right with a 24-unit inset. Its width follows
35% of the window, bounded between 394 and 426 logical units. The edit field,
scrolling list and image region follow the panel's content width with fixed side
margins. Fonts, control heights and the panel's 500-unit height keep their native
sizes; a window narrower than 418 or shorter than 524 units can clip the panel.

The TGUI badge anchors inside the panel beside the title using a fixed inset plus
a relative inset, and its text stays centered. The tooltip button anchors to the
window's bottom-right. Panel children are clipped to the parent's content area;
scrolling uses that same normal clipping behavior.

## Interaction checks

Repeat this sequence in normal and `--concurrent` modes, without `--max-updates`:

1. Check label/font appearance, panel transparency, overlap and clipping.
2. Click the field or press F2. Type, move the caret and delete text; typing WASD
   should edit text without moving the camera. Press Escape, then check camera input.
3. Scroll the list, reset the camera and change the image. Confirm counters and
   retained UI content continue updating.
4. Hover the bottom-right `?` tooltip, resize the window and, where available, move
   it between displays with different content scales. Check the panel's width
   bounds, the badge and tooltip anchors, side margins around the field/list/image,
   input hit positions and clip edges.
5. Hide/show the panel with F3, including while editing. Hiding releases text focus.
6. Close the window while UI updates/uploads are active. Check orderly shutdown
   in both modes.

For simultaneous keyboard/pointer checks, use a separate mouse or disable the
desktop's touchpad "Disable while typing" setting. That setting can suppress
touchpad motion after key presses; see
[libinput's behavior](https://wayland.freedesktop.org/libinput/doc/latest/palm-detection.html#disable-while-typing).

The controlled TGUI input/recording/scene/session checks are accepted. The new
layout and controlled runtime cases, independent composition and native
resize/widget/lifetime acceptance remain in
[U9](../../../docs/planning/cheryl-ui-integration-plan.md#remaining-development-sequence).
Reuse those accepted module results unless related source changes require a rerun;
batch any needed demo/consumer builds with one low-priority job.
