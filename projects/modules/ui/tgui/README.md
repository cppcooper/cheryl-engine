# TGUI UI integration

`Cheryl::UI::TGUI` is an optional UI owner. It translates selected Cheryl input
records and records/uploads toolkit draws as retained Cheryl scenes. The
toolkit-session/GUI lifecycle and real-widget acceptance remain in the
[U9 checklist](../../../../docs/planning/cheryl-ui-integration-plan.md#remaining-development-sequence).
The bridge is source-complete; executable acceptance remains pending. A complete
widget integration still needs the session/GUI owner.

The module links `Cheryl::Engine` and TGUI 1.13.0, configured with a custom backend
and FreeType font support only. It has no Native GLFW/OpenGL/Gainput link. Public
headers expose TGUI's native event API; Engine headers never include TGUI. The
[requirements](../../../../docs/planning/tgui-adapter-requirements.md) define the
materials, font ownership, upload/lifetime and platform decisions for the adapter.

## Selection and dependency

At the repository root, `CHERYL_BUILD_UI_TGUI=ON` is the default, independently of
native/graphics selection. Set it to `OFF` to omit the target and its toolkit/font
dependencies. Link the application to
`Cheryl::UI::TGUI`; its Engine and toolkit requirements propagate through the target.

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
triangles, flips textured V coordinates to Cheryl's image convention, and retains
intersected logical clips in draw order. Arbitrary rotated clipping rejects;
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
retained frames, failed/cancelled adoption and owner/domain rejection. The independent
consumer exercises event translation and direct rectangle recording. Actual
FreeType glyph growth, widgets, queued runtime/concurrent handoff and native pixels
belong to the remaining U9 acceptance; promise-based adoption checks do not prove
those paths.

After explicit build/test authorization, a standalone source composition is:

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

Batch the changed Engine/native input cases with this validation when their
executable acceptance is needed. Reuse existing directories and avoid repeating
the accepted neutral rendering probe or aggregate suites.
