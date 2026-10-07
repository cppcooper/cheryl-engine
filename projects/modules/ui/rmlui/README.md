# RmlUi UI integration

`Cheryl::UI::RmlUi` is the optional, independently selected RmlUi 6.3 owner.
It links `Cheryl::Engine` and `RmlUi::Core` with the stock FreeType font engine.
The module provides native document sessions, portable input translation, CPU
recording and retained scene upload. Independent consumer/header composition,
controlled sequential/concurrent runtime behavior and assembly coexistence are
accepted. The [demo guide](../../../apps/demo/README.md#interaction-checks) records
the native proof's coverage limits.

The [requirements](../../../../docs/planning/rmlui-adapter-requirements.md) define
the selected rendering/font/lifetime scope. The
[adapter-author guide](../../../../docs/development/ui-adapters.md) describes the
engine boundaries; RmlUi keeps its native context, RML and RCSS authoring API.

## Selection and dependency

Set `CHERYL_BUILD_UI_RMLUI=ON` and reload CMake to expose `module_ui_rmlui`,
`tests-ui-rmlui` and `all-ui-rmlui` when tests are selected. The root option defaults
to `ON`; TGUI and RmlUi can be selected separately or together. The module library's
output name is `cheryl-module-ui-rmlui`.

```cmake
target_link_libraries(game PRIVATE Cheryl::UI::RmlUi)
```

The module reuses an existing `RmlUi::Core`. Otherwise it uses an explicit
`CHERYL_RMLUI_SOURCE`, an explicit package location in `RmlUi_DIR`, the pinned
`extern/rmlui` submodule, or the exact package, in that order. Initialize the bundled
dependency with `git submodule update --init extern/rmlui`; CMake does not download
it. Supplied targets must declare `RMLUI_FONT_ENGINE=freetype`; ordinary source and
package configurations provide that metadata.

An owned source configuration selects FreeType/Core and excludes samples, toolkit
tests, native backends, Lua and optional plugins. Conflicting settings reject
instead of rewriting a host's choices. The module links Core directly, leaving
the upstream debugger outside the adapter dependency graph.

For a standalone module or its `tests/consumer/` entry point, set
`CHERYL_REPOSITORY_ROOT` to the Cheryl checkout. The module reuses a supplied
`Cheryl::Engine` or bootstraps Engine from that root, suppressing other
module/demo/test selection in its local scope. Consumer/header checks are selected
through `CHERYL_BUILD_CONSUMER_TESTS` at the root or by configuring the consumer
entry point independently.

## Input identities

`keyboard_key` maps portable physical keyboard identities to RmlUi keys; unknown
keys remain `KI_UNKNOWN`. Committed characters come from `TextEvent`, not from
this mapping. `modifiers` preserves Control, Shift, Alt, Super, Caps Lock and Num
Lock flags in RmlUi's modifier representation.

## Focused checks

| Target | Output / kind | Selection / coverage |
| --- | --- | --- |
| `tests-ui-rmlui` | `tests-ui-rmlui` | `CHERYL_BUILD_TESTS`: input, recording, sessions and controlled runtime cases. |
| `all-ui-rmlui` | `tests-all-ui-rmlui` | All owner GoogleTests; `CHERYL_BUILD_ALL_TESTS` adds it to the default build/CTest. |
| `consumer-module-ui-rmlui` | `cheryl-ui-rmlui-consumer` | `CHERYL_BUILD_CONSUMER_TESTS`: native document editing and retained CPU draws. |
| `consumer-module-headers--ui-rmlui` | Object library | Consumer's first-include header probes; built with the consumer. |

`ui_rmlui_input.*` covers representative keys and combined modifiers.
`ui_rmlui_recording.*`, `ui_rmlui_texture.*` and `ui_rmlui_scene.*` cover owned
geometry/pixels, alpha/orientation, clipping, retained frames, provider ownership,
material validation and failure-preserving adoption. A checked-in two-row image
distinguishes file premultiplication and row order from generated texture handling.
`ui_rmlui_session.*` uses real documents, input elements and FreeType fonts.
`ui_rmlui_runtime.sequential` and `.concurrent` exercise queued old/new image
publication, real runtime playback, owner threads and retained frames through
toolkit/provider teardown. They use controlled engine adapters without a display.
Their in-memory document has a source URL in the fixtures directory; relative image
names resolve against it, avoiding Core's removal of a leading slash from image URLs.
The independent consumer loads a document, edits its field and retains CPU draws
after session destruction; header probes use public headers as the first include.

Checks use the selected SDK's `Samples/assets/LatoLatin-Regular.ttf` fixture or
an explicit `CHERYL_RMLUI_TEST_FONT` file. Missing fonts reject configuration;
these cases do not silently skip font coverage or search the host's system fonts.

## Recording and publication

`RenderTarget` implements the Core render interface without graphics calls. It
copies indexed triangles during compilation and translated draws during recording.
Texture handles retain immutable decoded generations; releasing a toolkit handle
does not invalidate a completed recording. Generated premultiplied pixels remain
unchanged; decoded file RGB is multiplied by alpha once. Texture V coordinates
map RmlUi's downward-positive convention to Cheryl's upward-positive image UVs.

Rectangular scissor edges and logical dimensions travel with every draw. Zero
dimensions or empty clips produce no draws. Masks, nonidentity transforms, layers,
filters, custom shaders and repeating texture coordinates reject publication.
The failure stays latched for that renderer because the toolkit may cache an
unsupported compiled property without offering it again. Recreate the session
after removing such a property. SDK callbacks allow render-stack cleanup to finish
before the recording reports the error.

Session texture bounds are at least 1024, matching the stock FreeType atlas's
maximum dimension; the default bound is 4096. File/generated images must fit the
configured bound. Standalone CPU `RenderTarget` checks can use smaller bounds.

The application supplies `Materials` with colored triangle pipelines,
`PremultipliedAlpha`, disabled depth/write/culling, a projection semantic and a
custom image sampler for textured draws. Shaders multiply image RGBA by vertex
RGBA without an additional premultiplication. `SceneUploader` belongs to one
provider/domain and platform owner; `submit` transfers owned recording data through
Cheryl's dispatcher. `adopt_scene` never waits and replaces the retained scene only
after a complete successful upload. Failure or cancellation preserves its predecessor.

## Native documents and input

Create `Session` on the simulation/UI owner and call `context()` for native RML
document loading, RCSS styling, elements and listeners. The adapter contains no
layout string parser or common widget API. `load_font` registers a named family
from an owned file buffer or copied memory bytes. Both remain valid through Core
shutdown. The session owns one global Core initialization; a second session or
host-installed interface rejects instead of replacing the host's SDK state.

Supply copied logical/framebuffer dimensions with `set_view`. Context dimensions
are logical units and its density-independent ratio is one; framebuffer scaling
belongs to Cheryl's clip/projection playback. `update_time` advances the supplied
simulation duration and updates native layout/animation. `record` updates pending
authoring/input changes before rendering without advancing the clock again.

The session holds Events capture. `request_keyboard_focus` acquires Text capture
and a keyboard routing lease for its target.
`handle_input` observes modifier snapshots but delivers keyboard/text only for its
requested epoch. Poll-latched records drain before focus preemption clears native
editing. Pointer delivery is caller-selected; mouse buttons and fractional wheel
input apply their observation-time coordinates before delivery. Physical keys,
repeats and releases stay independent of committed Unicode text.

Capabilities report keyboard/text support and rectangular clipping. Clipboard,
cursor changes, IME and advanced effects are unavailable. Clipboard shortcuts are
suppressed before they change a selection; explicit clipboard interface calls
reject. The adapter never runs a native toolkit backend or window event loop.

Release external element, event-listener and toolkit-resource references before
session destruction. After the runtime joins simulation, `close_after_quiescence`
permits final platform-owner teardown. Native contexts and font globals shut down
while interface/font storage is still live. CPU recordings and uploaded Cheryl
frames retain their own geometry/pixels/resources independently.

## Acceptance procedure

For changes affecting RmlUi, reuse the existing native/OpenGL profile with tests
and both UI adapters selected. `demo` also requires native input. From the repository
root:

```sh
git submodule update --init extern/rmlui
```

```sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug -DCHERYL_BUILD_UI_RMLUI=ON \
  -DCHERYL_BUILD_UI_TGUI=ON -DCHERYL_BUILD_TESTS=ON
```

```sh
nice -n 19 cmake --build build/debug --parallel 1 \
  --target demo all-ui-rmlui tests-ui-coexist tests-engine tests-native-glfw
```

After the build, run these focused checks when the machine has cooled:

```sh
./build/debug/tests-all-ui-rmlui
./build/debug/tests-ui-coexist
./build/debug/tests-engine \
  --gtest_filter='runtime_contract.*:input_capture.portable_buttons:tick_context.unbound_stop'
./build/debug/tests-native-glfw --gtest_filter='glfw_bindings.portable_*'
```

`tests-ui-coexist` belongs to `projects/tests/`, and also joins the combined
`all-tests` runner. It checks poll-latched text delivery, focus preemption and
stale-lease release across both native toolkits, distinct alpha passes, and frames
retained through independent toolkit/provider teardown. Do not run both runners
for the same cases. Reuse the accepted TGUI suite unless changes affect its paths.

An independently configured consumer establishes standalone source composition
and first-include headers without selecting TGUI, Native GLFW or OpenGL. Run it
after the root checks settle, so any fixes precede a separate compilation:

```sh
cmake -S projects/modules/ui/rmlui/tests/consumer -B build/rmlui-consumer \
  -DCMAKE_BUILD_TYPE=Release -DCHERYL_REPOSITORY_ROOT="$PWD" \
  -DCHERYL_RMLUI_SOURCE="$PWD/extern/rmlui"
```

```sh
nice -n 19 cmake --build build/rmlui-consumer --parallel 1 \
  --target consumer-module-ui-rmlui
```

```sh
./build/rmlui-consumer/cheryl-ui-rmlui-consumer
```

Verify that `consumer-module-ui-rmlui` depends on
`consumer-module-headers--ui-rmlui` and compiles its input, rendering, scene and
session probes. The independent graph must exclude TGUI, Native GLFW and OpenGL.
Root consumers alone do not prove isolation; supplied-target/package composition
needs its own check when used. TGUI's independent procedure stays in its
[module guide](../tgui/README.md#focused-checks).

Follow the [demo procedure](../../../apps/demo/README.md#interaction-checks) in
normal and `--concurrent` modes for native alpha/orientation, fonts, focus,
scrolling, bounded resize/DPI, image replacement and shutdown. Controlled cases
do not establish compositor behavior, physical input or GPU resource retirement.
Report skipped/unselected checks separately. The selected native demo checks are
accepted from the user's manual report in both runtime modes; the
[demo guide](../../../apps/demo/README.md#interaction-checks) retains their limits.
