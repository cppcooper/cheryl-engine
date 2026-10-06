# TGUI UI integration

`Cheryl::UI::TGUI` is an optional UI owner. It translates selected Cheryl input
records and records/uploads toolkit draws as retained Cheryl scenes. A `Session`
owns the custom backend, GUI, FreeType fonts and input leases. The
[selected demo](../../../apps/demo/README.md) supplies a representative panel and
application materials. Remaining composition and native widget acceptance are
tracked in the
[U9 checklist](../../../../docs/planning/cheryl-ui-integration-plan.md#remaining-development-sequence).

The module links `Cheryl::Engine` and TGUI 1.13.0, configured with a custom backend
and FreeType font support only. It has no Native GLFW/OpenGL/Gainput link. Public
headers expose TGUI's native GUI/widget API; Engine headers never include TGUI. The
[requirements](../../../../docs/planning/tgui-adapter-requirements.md) define the
materials, font ownership, upload/lifetime and platform decisions for the adapter.
The [adapter-author guide](../../../../docs/development/ui-adapters.md) describes
the engine boundaries used by independently implemented UI modules.

## Contents

- [Selection and dependency](#selection-and-dependency)
- [Session ownership](#session-ownership)
- [Typed layout](#typed-layout)
- [Input translation](#input-translation)
- [Recording and publication](#recording-and-publication)
- [Focused checks](#focused-checks)

## Selection and dependency

At the repository root, `CHERYL_BUILD_UI_TGUI=ON` is the default, independently of
native/graphics selection. Set it to `OFF` to omit the target and its toolkit/font
dependencies. Link the application to
`Cheryl::UI::TGUI`; its Engine and toolkit requirements propagate through the target.

```cmake
target_link_libraries(game PRIVATE Cheryl::UI::TGUI)
```

The module reuses an existing `TGUI::TGUI` target. Otherwise it uses an explicit
`CHERYL_TGUI_SOURCE`, an explicit package location in `TGUI_DIR`, the pinned
`extern/tgui` submodule, or that exact version's discovered CMake package, in that
order. Initialize the bundled dependency with
`git submodule update --init extern/tgui`; CMake never downloads it. Source
composition requires FreeType development files and selects only the
custom/FreeType backend and excludes upstream tools, examples and tests. Conflicting
backend settings fail explicitly; supplied targets/settings are not rewritten.
Compiled version/feature checks also reject a supplied package/target with an
unreviewed version or native toolkit backend.

Existing CMake/CLion profiles with `CHERYL_BUILD_UI_TGUI=OFF` cached must change it
to `ON` and reload CMake to expose `cheryl_ui_tgui` and its selected test targets.

For a standalone module, supply `Cheryl::Engine` or `CHERYL_ENGINE_SOURCE`. Engine
bootstrap suppresses UI/native/graphics/demo/test selection in its local scope.
For an independently configured consumer, use `tests/consumer/` as the source
directory with the same dependency arguments; it links only this module and checks
its public header as the first include.

## Session ownership

Create `Session(input, nonzero_focus_id, options)` on the simulation/UI thread,
before creating toolkit objects. A session installs TGUI's process-global backend;
an already active toolkit backend/session rejects explicitly. Use `session.gui()`
and TGUI's native widgets on that same owner. The session keeps Events capture
active, uses the toolkit's embedded default font and selects guarded FreeType
rasterization. `SessionOptions` supplies a texture bound of at least 128 and an
explicit positive font scale; this is not a hardware or monitor-scale query.

For each update, pass the tick's copied `logical_size` and `framebuffer_size` to
`set_view`, then pass simulation seconds to `update_time` and the entire immutable
record stream to `handle_input`. GUI input/layout use logical units; the recording
target uses physical ratios only for pixel rounding. Zero framebuffer dimensions
record an empty scene. Drawing does not advance a separate wall clock or run a
toolkit loop. `record()` returns owned CPU data for the uploader below.

`request_keyboard_focus` acquires routed focus and Text capture; release drops
those leases and unfocuses widgets. Keyboard/text delivery requires this session's
target and latest requested epoch. A newer lease with the same target rejects
obsolete epochs. On external preemption, already collected records drain before
the session releases its old lease; that release cannot erase the newer owner.
The complete stream updates modifier snapshots, including unselected releases.
The caller explicitly selects pointer delivery; pointer capture, modal arbitration
and controller navigation are not implied. A routed keyboard lease does not
fabricate an OS window-focus event.

`capabilities()` reports the input source's committed-text/focus support and
unavailable OS clipboard, cursor and IME services. Clipboard calls reject; standard
toolkit copy/cut/paste shortcuts are skipped before widgets can delete a selection.
Cursor requests leave the platform cursor alone. Nearest font sampling rejects
before FreeType mutates its smoothing flag or its atlas texture.

Release application-held widgets, fonts and toolkit textures before session
destruction. The input source must outlive it. After simulation has joined,
`close_after_quiescence()` permits serial final destruction on the platform thread;
no toolkit calls or references may race or survive it. GUI resources are destroyed
before TGUI's global font, theme, timers and backend. Retained CPU recordings and
uploaded Cheryl frames remain independent of the toolkit lifetime.

## Typed layout

`layout.h` defines three configuration values: `WidgetLayout`, `Offset` and
`Scalable`. Add a native TGUI widget to this session's GUI hierarchy, then call
`session.set_layout(widget, rules)` on the UI owner. The adapter installs numeric
TGUI bindings; TGUI maintains them when the window, parent size, border or padding
changes. The adapter does not parse layout strings or create a separate widget
hierarchy. Native toolkit authoring and styling remain available.

| Configuration | Contract |
|---|---|
| `WidgetLayout.anchor` | A finite point in `[0, 1]` within the parent's content area; `{1, 0}` selects top-right. The default is top-left. |
| `WidgetLayout.origin` | The normalized attachment point on the widget. Omission uses `anchor`. |
| `Offset.fixed` | Signed displacement in logical UI units. |
| `Offset.relative` | Signed fractions of parent content width/height, added to the fixed displacement. Omission defaults both parts to zero. |
| `WidgetLayout.scalable` | Optional sizing rules. Without them, a newly configured widget keeps its native sizing. |
| `Scalable.width`, `.height` | Optional finite fractions in `[0, 1]` of parent content dimensions. At least one must be supplied. Only selected axes resize. |
| `Scalable.min_width`, `.max_width`, `.min_height`, `.max_height` | Bounds in logical units for the selected axes. Minimums are finite and nonnegative; maximums cannot be below them. Defaults are zero and an unbounded positive maximum. |

The widget's attachment point is `anchor * parent_content_size + fixed_offset +
relative_offset * parent_content_size`. Origin determines which point of the widget
sits there. Resizing is a separate optional component and does not change the
meaning of offsets. Logical sizes map to framebuffer pixels through `set_view`.

```cpp
using CE::UI::TGUI::Scalable;

session.gui().add(panel);
session.set_layout(panel, {
    .anchor = {1, 0},
    .offset = {.fixed = {-8, 24}, .relative = {-0.02f, 0}},
    .scalable = Scalable{.width = 0.35f, .min_width = 320, .max_width = 600},
});
```

Changing rules replaces placement and any selected sizing bindings. An axis
previously managed by `Scalable` freezes at its current size when its fraction is
removed; other native size bindings remain in place. Omit `scalable` to release
both managed axes. A minimum can exceed a small parent's available space: the
containing UI chooses scrolling or a different arrangement. This API does not
silently shrink below the minimum. Scaling uses TGUI's native `setSize` semantics,
including each widget's own automatic-sizing behavior; text and padding follow
native styling.

Typed placement requires TGUI's `AutoLayout::Manual`. Native automatic alignment
can be used separately. Invalid rules, unparented widgets and foreign hierarchies
reject before widget mutation. Reapply after moving a widget to a different parent;
bindings refer to the parent selected by that call. Weak sizing bookkeeping does
not retain widgets, parents or toolkit resources, and expires with the session.
The call retains its target while native change signals run, so a callback may
remove the widget and release the caller's handle safely.

## Input translation

`translate_event(record)` reads portable keyboard/mouse identities and committed
text, preserving the original record for other readers. It does not choose focus,
capture or pointer routing. The application selects the eligible records and
dispatches translated events in their original order on the simulation owner.

Press and Repeat become TGUI `KeyPressed`; keyboard releases produce no toolkit
event because TGUI's event API has none. A GUI integration still updates its
modifier snapshot from the complete original stream, including releases.
Unsupported keys/devices/buttons return no event. Gamepad State remains available
to gameplay and does not become invented keyboard input.

Pointer coordinates are logical window coordinates floored to integer values;
negative fractional coordinates remain outside the window. Clicks and vertical
wheel events use their own observation-time positions and reject missing, nonfinite
or unrepresentable coordinates. Configure the GUI's input view in those units.
Fractional vertical wheel offsets are preserved; TGUI has no horizontal-wheel
event, so the original Cheryl record remains the source for another consumer.
Invalid Unicode scalars or nonfinite/unrepresentable wheel offsets reject explicitly.

## Recording and publication

`Renderer(maximum_size)` creates CPU-only `Texture` objects. The configured limit
is an application-supported bound, not a hardware query. Each texture load copies
top-to-bottom RGBA pixels into a fresh immutable snapshot, including the transient
FreeType atlas path. Old recordings retain their original snapshot. Base toolkit
image loading also retains its pixels for transparent-pixel hit testing.

The initial policy requires the application's provider to create smoothed,
clamp-to-edge RGBA images through `create_image`. Cheryl OpenGL's default satisfies
this policy, including its mipmap filtering. Nearest sampling and
`setSmooth(false)` reject before changing an adapter texture. Supporting another
policy requires a neutral provider contract; silently accepting an unsupported
toolkit setting would change appearance.

`RenderTarget` receives a view, viewport and target extent in logical window units.
Supply copied framebuffer/logical ratios through `set_pixel_scale` for toolkit
pixel rounding. The target bakes transforms and view mapping into copied colored
triangles, accepting both indexed geometry and unindexed triangle lists used for
glyphs. It flips textured V coordinates to Cheryl's image convention and retains
intersected logical clips in draw order. Clip edges map directly from the toolkit's
intersected view rectangle in double precision; an intermediate float viewport
must not add a pixel to an exact edge. Genuine fractional edges still use Cheryl's
conservative outward rounding during playback. Arbitrary rotated clipping rejects;
rectangular/right-angle clipping follows the pinned toolkit's supported algorithm.
Zero-sized views and empty clips record no draws. Changing a view/scale during
recording or finishing unmatched clip layers rejects explicitly.

Use `begin_recording`/`finish_recording` for direct drawing. `drawGui(root)` starts
a recording and discards it if a widget throws; call `finish_recording` after it
succeeds. `discard_recording` releases incomplete data and clip state. Platform
clearing remains with the application; TGUI's `mainLoop`/clear functions are not
part of this bridge.

Construct `SceneUploader(provider)` on the platform owner during initialization,
then copy its handle to simulation. The application supplies `Materials` with
colored-triangle pipelines, straight alpha, disabled depth/write/culling and
Cheryl's projection semantic. A textured material declares the named custom
`Sampler2D`; its shader multiplies sampled RGBA by vertex RGBA. Other custom values
come from material defaults or a copied pass layer accepted by each used pipeline.
Backend-specific pipeline construction stays in the application/graphics module.

`submit(platform_submission, recording, materials)` transfers owned data through
the existing dispatcher and returns a future. Uploads require the original provider
domain and platform thread. The uploader reuses a live uploaded image for the same
CPU generation; its weak cache does not retain abandoned CPU images or GPU handles.
Changed pixels always produce another image. Geometry remains separately uploaded
per draw; the adapter introduces no sorting or batching policy.

During update, `adopt_scene(future, current)` checks readiness without waiting.
It replaces `current` only with a complete successful scene; an upload error or
cancelled dispatcher future propagates while `current` remains usable. Keep at
most one outstanding replacement per GUI in the initial integration and continue
writing the current scene while it is pending. `current.write(frame_writer)` adds
a UI pass at the caller's chosen point in pass order. Packets own uploaded images,
geometry, materials, parameters and clips, independent of subsequent widget changes
or adapter destruction; native playback still requires the original live domain.

## Focused checks

The module owns `ui-tgui-tests`, the `ui-tgui-all` aggregate and
`cheryl-ui-tgui-consumer`. Its cases join `all-tests` only when this module is
selected. Input/recording checks and controlled-provider scene checks require no
window, graphics context, font file or installed toolkit-global backend. They cover
index expansion, transforms, clips, immutable texture generations, resource reuse,
retained frames, failed/cancelled adoption and owner/domain rejection. Session checks
create their own custom backend and embedded font. They cover widget text routing,
focus epochs/preemption, modifier releases, pointer selection, unsupported clipboard
shortcuts, copied view sizes, FreeType atlas growth/immutable generations and serial
teardown after the UI owner stops. The independent consumer creates and records a
real label.

`ui_tgui_layout.*` checks edge anchors, mixed fixed/relative offsets, parent
border/padding changes, bounded per-axis resizing, rule replacement, native size
bindings, owner/hierarchy rejection, weak lifetime and anchored recording. The
consumer and first-include probes also cover the typed layout header.

`ui_tgui_runtime.sequential` and `ui_tgui_runtime.concurrent` use the real
`GameRuntime`, TGUI session and platform queue with controlled window/input,
presentation, renderer and memory-resource implementations. They mutate the widget's
image after recording, publish the old and replacement generations, copy published
packets into owned retained frames and release them after toolkit/game/provider
teardown. Window reads and uploads require the platform owner; GUI recording and
frame preparation require the simulation owner. Renderer observations coordinate
replacement and shutdown, with a bounded window deadline rather than sleeps.
Both runtime cases are accepted as part of the complete module suite. They do not
establish native GPU resource retirement or compositor behavior.

Independent consumer/header composition and native pixels also require the remaining
U9 acceptance. Reuse the accepted complete module suite unless related source
changes require a rerun. A root build does not establish standalone composition.

To build the focused checks in a standalone source composition:

```sh
cmake -S projects/modules/ui/tgui -B build-ui-tgui \
  -DCHERYL_ENGINE_SOURCE=/path/to/cheryl-engine \
  -DCHERYL_TGUI_SOURCE=/path/to/TGUI-1.13.0 \
  -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON
nice -n 19 cmake --build build-ui-tgui --parallel 1 \
  --target ui-tgui-tests cheryl-ui-tgui-consumer
./build-ui-tgui/ui-tgui-tests
./build-ui-tgui/cheryl-ui-tgui-consumer
```

For the remaining U9 checks, use the
[focused batch](../rmlui/README.md#acceptance-procedure) and this module's independent
consumer. Reuse existing directories and avoid repeating the accepted TGUI suite
or neutral rendering probe.
