(rmlui-ui-integration)=
# RmlUi reference

Use `Cheryl::UI::RmlUi` to author native RML documents and RCSS styles, receive
Engine input and publish retained scenes. The application owns its session and
chooses the pass order. RmlUi owns document layout and editing;
Engine does not acquire a common widget API or a toolkit dependency.

Start with [selection](#selection-and-dependency) and the
[editable-document example](#create-an-editable-document). Use
[fonts](#fonts), [recording/publication](#recording-and-publication) and
[materials](#default-and-custom-materials) for application contracts; use
[dependency configuration](#dependency-configuration), the
[placeholder contract](#placeholder-dependency-contract) and
[focused checks](#focused-checks) when integrating a supplied toolkit.

## Selection and dependency

Set `CHERYL_BUILD_UI_RMLUI=ON` and reload CMake to expose `module_ui_rmlui`,
`tests-ui-rmlui` and `all-ui-rmlui` when tests are selected. The root option defaults
to `ON`; TGUI and RmlUi can be selected separately or together. The module library's
output name is `cheryl-module-ui-rmlui`.

```cmake
target_link_libraries(game PRIVATE Cheryl::UI::RmlUi)
```

## Create an editable document

[Create an interactive GUI with RmlUi](../getting-started/how-to/create-gui.md)
provides the full First Game extension: module selection, inherited Engine fonts,
RML and RCSS files, native event listeners,
focus routing, retained scene publication and partial-safe cleanup.

The [maintained RmlUi demo](../../../projects/apps/demo/src/rmlui-demo.cpp) also
demonstrates document loading, hit testing and recoverable upload errors.
For a C++ widget HUD using the other integration, follow
[Create a HUD with TGUI](../getting-started/create-a/hud.md).
The contracts below describe RmlUi session ownership and supported behavior.

## Native documents and input

Create `Session(engine_context, nonzero_focus_id, options)` on the simulation/UI owner and call `context()` for native RML
document loading, RCSS styling, elements and listeners. The adapter contains no
layout string parser or common widget API. The session imports the Engine font
catalogue and establishes its configured default for document inheritance. The
session owns one global Core initialization; a second session or
host-installed interface rejects instead of replacing the host's SDK state.

Its focus ID identifies the entire session as an Engine keyboard/text recipient.
Choose any nonzero value and keep independent recipients distinct; its magnitude
gives no priority. Documents/elements share that ID and use native focus within
the session. See [session routing and widget focus](../gui/index.md#session-routing-and-widget-focus).

For a manual session, initialize copied logical/framebuffer dimensions with `set_view`, then refresh
them whenever either pair changes. Repeating the call each update is also valid. Context dimensions
are logical units and its density-independent ratio is one; framebuffer scaling
belongs to Cheryl's clip/projection playback. `update_time` advances the supplied
simulation duration and updates native layout/animation. `record` updates pending
authoring/input changes before rendering without advancing the clock again.

The session holds Events capture. `request_keyboard_focus` acquires Text capture
and a keyboard routing lease for its target.
`handle_input` observes modifier snapshots but delivers keyboard/text only for its
target and requested epoch saved at call entry. Pass the complete immutable tick
record batch once: poll-latched records drain before external focus preemption
clears native editing, including when the batch is empty.

The three-argument overload calls `before_record(record)` before each delivery
and uses its return value as that record's pointer selection. Application controls
can request focus or show/hide a view in that callback without changing the entry
epoch used to select keyboard/text for the rest of the batch. Future polls observe
the new routing lease; existing records retain their original targets/epochs.
The two-argument overload keeps a constant pointer selection. Unselected pointer
delivery sends mouse-leave when necessary. Mouse buttons and fractional wheel
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

## Automatic view and time

All constructors remain manual by default. A Context-aware session can opt in
when constructed on its initialized simulation owner, normally the first game
update. Include `<cheryl/ui/rmlui/session.h>` and `<memory>`; with `engine` as
the live Context:

```cpp
auto session = std::make_unique<CE::UI::RmlUi::Session>(
    engine, 3,
    CE::UI::RmlUi::SessionOptions{.automatic_view = true, .automatic_time = true}
);
```

Retain the session between updates. `automatic_view` applies the current
`SimulationView` seed before construction returns and follows later logical/pixel
size changes, including zero drawable extents. It must not be enabled during
platform `init()` before the simulation source exists. The tutorial and demo
choose automatic view following while keeping their existing manual time/input
order.

`automatic_time` advances toolkit time on **future** `SimulationStepStarting`
phases, before the game update. A session first created inside an update does not
replay that step. While attached, every manual `update_time()` call rejects,
including zero; call `detach_time()` on the UI owner before returning to manual
advancement. Choose manual time when the application needs it after input or
other game work. Neither option delivers input, adopts scenes or chooses pass
order.

Existing sessions can call `follow_view(engine.simulation_view())` and
`follow_time(engine.services())` explicitly. Standalone input/fonts constructors
reject automatic flags because they have no Context, but can use those explicit
methods with initialized sources on the same owner. This keeps manual standalone
use independent of Runtime.

A valid public `set_view()` first detaches automatic sizing, then applies the
manual view. Invalid dimensions leave the existing attachment intact; a native
application failure after detach leaves manual mode selected. `follow_view()`
can reattach with a fresh coherent seed. Session closure acknowledges both
attachments before releasing toolkit state. Required sizing/time failures
propagate to the runtime before dependent work; optional bus observers are
[separate subscriptions](../runtime/event-delivery.md#observe-a-change-or-participate-in-an-operation).

## Fonts

Configure `engine.fonts()` before creating a context-aware session; see the
[Engine font service](../text/index.md). Documents inherit that
default unless native RCSS overrides it. Registered references are native family
names, so `font-family: handwriting` selects that registered family, while
`font-weight` and `font-style` select its actual faces. When a family supplies only
normal or only italic faces, the adapter registers those available faces for the
missing style too; weight selection still uses the closest available face.
RmlUi compares family names
without case: references differing only by case reject session import. Use simple
names such as `book-print` in RCSS, and quote names that require native RCSS quoting.

An explicit random/restored Engine font uses the same handle as direct text.
This uses the `books` group from the [catalogue configuration example](../assets/text-layout.md#configure-fonts-once):

```cpp
auto selected = engine.fonts().choose_random("books");
session.set_font(*book_element, selected);
// session.font_family(selected) returns its registered native family name.
```

The property inherits through the element's descendants; a child can override it.
Save `selected.reference()` in application data to retain the choice. The adapter
loads retained bytes with their collection face indices and styles, and keeps them
alive through Core shutdown. It does not reopen source paths. Only the configured
common glyph fallback families and embedded safety font become global native
fallbacks; registering a specialty font does not make it a fallback for other text.
RmlUi searches glyph fallback faces in registration order. That native fallback
search does not match the originating text's weight or italic style; an available
fallback glyph may therefore use a different style.

`Session(input, fonts, target, options)` injects an independent Engine font service.
The standalone `Session(input, target, options)` and explicit `load_font(file,
family)`/`load_font(bytes, family)` paths remain available. Explicit memory loading
copies its bytes; file loading uses a Core-owned buffer. Both survive through Core
shutdown. Explicit registration cannot replace a family imported from Engine, and
an Engine handle cannot replace an explicitly registered family. Service
reconfiguration does not rebind an existing session or retained font.

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

Session texture bounds are at least 1024, matching the retained font atlas's
maximum dimension; the default bound is 4096. File/generated images must fit the
configured bound. Standalone CPU `RenderTarget` checks can use smaller bounds.

Construct `SceneUploader(provider)` on the platform owner during initialization.
`submit(platform_submission, recording)` transfers owned recording data through
Cheryl's dispatcher and selects automatic materials. `upload(provider, recording)`
uses the same defaults when already on that owner. The uploader belongs to one
provider/domain and platform owner. `adopt_scene` never waits and replaces the
retained scene only after a complete successful upload. Failure or cancellation
preserves its predecessor.

## Input identities

`keyboard_key` maps portable physical keyboard identities to RmlUi keys; unknown
keys remain `KI_UNKNOWN`. Committed characters come from `TextEvent`, not from
this mapping. `modifiers` preserves Control, Shift, Alt, Super, Caps Lock and Num
Lock flags in RmlUi's modifier representation.

## Default and custom materials

The first nonempty upload creates defaults through the provider's generic shader
builder on the platform owner. OpenGL embeds the required shaders and reuses live
matching materials; applications need no UI shader files or native builder.
Unsupported providers reject the default request.

The optional third argument to `submit` or `upload` supplies explicit `Materials`
handles, including materials loaded from JSON. They require colored triangles,
`PremultipliedAlpha`, disabled depth/write/culling, a projection semantic and a
custom image sampler for textured draws. Shaders multiply image RGBA by vertex
RGBA without an additional premultiplication. Other custom values can come from
material defaults or `Materials::pass_parameters`. Passing `Materials{}` explicitly
selects an empty override; it does not request defaults for nonempty draws.
See [custom UI materials](../rendering/pipelines-and-materials.md#custom-ui-materials)
for handle selection and the separate manual construction API.

## Dependency configuration

It links shared RmlUi Core and `Cheryl::Engine`. Session installs a font engine
whose reviewed upstream provider, atlas, effects and fallback logic live in Core;
the Cheryl adapter supplies Engine rasterization through private callbacks.
Core itself does not link Engine, preserving that boundary with static Cheryl too.
The module provides native document sessions, portable input translation, CPU
recording and retained scene upload. The
[adapter-author guide](../development/ui-adapters.md) describes the
engine boundaries; RmlUi keeps its native context, RML and RCSS authoring API
exported from the shared toolkit library.

This core integration is sufficient for the selected Linux root-composition scope.
Further rendering effects are
[planned extensions](../../planning/long-term/README.md#ui-adapters),
including any required graphics-backend support. OS services remain deferred.
Dedicated regression and supplied-Core acceptance gaps remain in the
[startup/UI follow-up](../../planning/short-term/README.md#startup-and-ui-follow-up).

The module reuses an existing `RmlUi::Core`. Otherwise it uses an explicit
`CHERYL_RMLUI_SOURCE`, an explicit package location in `RmlUi_DIR`, the pinned
`extern/rmlui` submodule, or the exact package, in that order. Initialize the bundled
dependency with `git submodule update --init extern/rmlui`; CMake does not download
it. Core is configured with `RMLUI_FONT_ENGINE=none`; the module installs its
Engine-backed implementation explicitly. Supplied shared targets declare
`CHERYL_RMLUI_ENGINE_SERVICES=TRUE` and `CHERYL_RMLUI_RASTERIZER_ABI=1` after
implementing the reviewed private rasterizer bridge and preserving native API exports. Metadata does
not make a stock incompatible binary satisfy those contracts.

The adapter requires the matching reviewed font integration sources even when
Core comes from a package; supply `CHERYL_RMLUI_SOURCE` or retain the pinned
`extern/rmlui` checkout. The private callback signatures use those upstream font
headers. Session initialization binds the callback table only after acquiring
toolkit ownership, and shutdown releases font faces before clearing it. Keep Core,
the adapter and Engine loaded while their objects or callbacks remain in use.

An owned source configuration selects Core without a separate FreeType engine and excludes samples, toolkit
tests, native backends, Lua and optional plugins. Conflicting settings reject
instead of rewriting a host's choices. The module links Core directly, leaving
the upstream debugger outside the adapter dependency graph.

The standalone module and its `tests/consumer/` entry point locate their enclosing
checkout; set `CHERYL_REPOSITORY_ROOT` to select another absolute checkout path.
The module reuses a supplied
`Cheryl::Engine` or bootstraps Engine from that root, suppressing other
module/demo/test selection in its local scope. Consumer/header checks are selected
through `CHERYL_BUILD_CONSUMER_TESTS` at the root or by configuring the consumer
entry point independently.

## Placeholder dependency contract

The selected RmlUi 6.3 Core must include the empty-value correction in
`WidgetTextInput::CalculateCharacterIndex`. In the pinned source,
[`GetValue()`](../../../extern/rmlui/Source/Core/Elements/WidgetTextInput.cpp#L1008)
returns an empty string while the placeholder is shown, but
[`FormatText()`](../../../extern/rmlui/Source/Core/Elements/WidgetTextInput.cpp#L1285)
retains offsets and lengths from the displayed hint. The
[hit-test](../../../extern/rmlui/Source/Core/Elements/WidgetTextInput.cpp#L1120)
uses those offsets and lengths to form an iterator range into `GetValue()`.
That requires:

```text
0 <= line.value_offset
0 <= line.editable_length
line.value_offset + line.editable_length <= GetValue().size()
```

A nonempty hint can violate this invariant against the empty editable value.
Later cursor clamping cannot undo an invalid read. Returning zero before forming
the range preserves the only valid insertion position without treating the hint
as editable content.

When Cheryl creates Core from `CHERYL_RMLUI_SOURCE` or the bundled source, its
[helper](../../../projects/modules/ui/rmlui/cmake/RmlUi.cmake) compiles a generated copy of `WidgetTextInput.cpp` with
an early `GetValue().empty()` return. It excludes the original translation unit,
preserves its include directory and leaves the dependency checkout unchanged.
An already guarded source is reused; an unrecognized entry/source layout rejects
configuration. The helper then marks the concrete Core target with
`CHERYL_RMLUI_PLACEHOLDER_FIX=TRUE`.

Existing targets and installed packages bypass that helper. Their concrete Core
target must declare the same property after the provider or consumer verifies
that the actual library contains the correction. The declaration does not patch
a supplied binary or prove its runtime behavior. Missing declarations reject
configuration; use Cheryl-owned source or a corrected dependency with the property.

For a verified, corrected external package, place this after finding the package
and before adding Cheryl. Resolve an alias before setting the property:

```cmake
find_package(RmlUi 6.3 EXACT CONFIG REQUIRED)
get_target_property(rmlui_core_target RmlUi::Core ALIASED_TARGET)
if(NOT rmlui_core_target)
    set(rmlui_core_target RmlUi::Core)
endif()
# Declare an existing correction; this does not modify the supplied library.
set_property(TARGET "${rmlui_core_target}" PROPERTY CHERYL_RMLUI_PLACEHOLDER_FIX TRUE)
```

Shared Cheryl SDK consumers use the integrated owner binary and require no
separate exported Core dependency or manual declaration. Supplied source
compositions still verify the Core correction and Engine-service configuration.

The method-entry guard also returns before resetting
`ideal_cursor_position_to_the_right_of_cursor`. Its effect on type/clear/reclick,
double-click and textarea vertical movement remains a dedicated regression and
upstream-review requirement. The
[upstream follow-up](../../planning/short-term/README.md#upstream-placeholder-follow-up)
owns the unmodified reproduction, regression cases and submission prerequisites.

The Cheryl-owned correction has accepted placeholder hit-testing observations in
the Linux/X11/OpenGL demo; existing session tests do not directly exercise that
correction. Supplied targets/packages still require
[independent host acceptance](../../planning/short-term/README.md#supplied-rmlui-acceptance)
and correction declarations. Downstream acceptance does not establish an
unmodified-upstream reproduction or upstream acceptance.

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
`ui_rmlui_session.*` uses real documents, input elements and Engine-rasterized fonts.
Font catalogue cases cover inherited defaults, per-element families, real style
faces and collection indices, retained source bytes, and alias collision rejection.
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

## Acceptance procedure

For changes affecting RmlUi, reuse the existing native/OpenGL profile with tests
and both UI adapters selected. `demo` also requires native input. From the repository
root:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  git submodule update --init extern/rmlui
)
```

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug -DCHERYL_BUILD_UI_RMLUI=ON -G Ninja \
    -DCHERYL_BUILD_UI_TGUI=ON -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_DEMO=ON
)
```

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake --build build/debug --parallel \
    --target demo all-ui-rmlui tests-ui-coexist tests-engine tests-native-glfw
)
```

Run the focused checks:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build/debug/tests-all-ui-rmlui
  ./build/debug/tests-ui-coexist
  ./build/debug/tests-engine \
    --gtest_filter='runtime_contract.*:input_capture.portable_buttons:tick_context.unbound_stop'
  ./build/debug/tests-native-glfw --gtest_filter='glfw_bindings.portable_*'
)
```

`tests-ui-coexist` belongs to `projects/tests/`, and also joins the combined
`all-tests` runner. It checks poll-latched text delivery, focus preemption and
stale-lease release across both native toolkits, distinct alpha passes, and frames
retained through independent toolkit/provider teardown. Do not run both runners
for the same cases. Run the affected TGUI session checks as well when shared
routing/session paths change. The accepted root selections and remaining dedicated
regression limits are in the
[UI validation guide](../development/ui-adapters.md#repeating-linux-root-validation).
Native interaction is accepted in the selected Linux root composition; the
[demo procedure](../../../projects/apps/demo/README.md#startup-and-ui-validation) preserves
its scope and reusable observations.

An independently configured consumer establishes standalone source composition
and first-include headers without selecting TGUI, Native GLFW or OpenGL. Run it
after the root checks settle, so any fixes precede a separate compilation:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S projects/modules/ui/rmlui/tests/consumer -B build/rmlui-consumer -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCHERYL_REPOSITORY_ROOT="$PWD" \
    -DCHERYL_RMLUI_SOURCE="$PWD/extern/rmlui"
)
```

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake --build build/rmlui-consumer --parallel \
    --target consumer-module-ui-rmlui
)
```

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build/rmlui-consumer/cheryl-ui-rmlui-consumer
)
```

Verify that `consumer-module-ui-rmlui` depends on
`consumer-module-headers--ui-rmlui` and compiles its input, rendering, scene and
session probes. The independent graph must exclude TGUI, Native GLFW and OpenGL.
Root consumers alone do not prove isolation; supplied-target/package composition
needs its own check when used. TGUI's independent procedure stays in its
[module guide](tgui.md#focused-checks).

Follow the [demo procedure](../../../projects/apps/demo/README.md#interaction-checks) in
normal and `--concurrent` modes for native alpha/orientation, fonts, focus,
scrolling, bounded resize/DPI, image replacement and shutdown. Controlled cases
do not establish compositor behavior, physical input or GPU resource retirement.
The [demo guide](../../../projects/apps/demo/README.md#interaction-checks) retains selected
platform limits.

For a toolkit upgrade, recheck Core/Engine font-service metadata, premultiplied file
and generated images, font-byte lifetime, placeholder correction/declarations and
retained frames through teardown.
Accept changed behavior independently before extending native consumers or the
cross-toolkit coexistence suite.
