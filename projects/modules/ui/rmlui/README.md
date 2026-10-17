# RmlUi UI integration

Use `Cheryl::UI::RmlUi` to author native RML documents and RCSS styles, receive
Engine input and publish retained scenes. The application owns its session and
chooses the materials and pass order. RmlUi owns document layout and editing;
Engine does not acquire a common widget API or a toolkit dependency.

## Selection and dependency

Set `CHERYL_BUILD_UI_RMLUI=ON` and reload CMake to expose `module_ui_rmlui`,
`tests-ui-rmlui` and `all-ui-rmlui` when tests are selected. The root option defaults
to `ON`; TGUI and RmlUi can be selected separately or together. The module library's
output name is `cheryl-module-ui-rmlui`.

```cmake
target_link_libraries(game PRIVATE Cheryl::UI::RmlUi)
```

## Create an editable document

Start from the game lifecycle in First Game. This example adds an editable RML
field, with the same owner boundaries used by the maintained
[demo implementation](../../../apps/demo/src/rmlui-demo.cpp). It uses OpenGL
materials, the shaders `tgui.vert`, `tgui-solid.frag` and `tgui-textured.frag` under
`graphics/shaders/`, and an explicit TrueType/OpenType font file. Copy those shader
files from the checkout's assets when extending First Game's smaller asset root.
The indexed loader does not construct these UI materials.

Include `<cheryl/backends/opengl/resource-provider.h>`,
`<cheryl/ui/rmlui/session.h>`, `<cheryl/ui/rmlui/scene.h>` and
`<RmlUi/Core/ElementDocument.h>`, plus `<filesystem>`, `<future>`, `<memory>`,
`<optional>`, `<stdexcept>` and `<utility>`. Keep the following members in your game, alongside
its context reference `engine_`, asset root `root_` and font path `font_`:

```cpp
std::optional<CE::UI::RmlUi::SceneUploader> uploader_;
CE::UI::RmlUi::Materials materials_;
std::unique_ptr<CE::UI::RmlUi::Session> ui_;
Rml::ElementDocument* document_ = nullptr;
CE::UI::RmlUi::Scene scene_;
std::future<CE::UI::RmlUi::Scene> pending_;
```

### Prepare materials during initialization

Define this application helper using the maintained demo's material construction.
Its shaders multiply vertex/image RGBA; RmlUi requires premultiplied blending:

```cpp
CE::UI::RmlUi::Materials make_rmlui_materials(
    CE::Rendering::OpenGL::ResourceProvider& native, const std::filesystem::path& root
) {
    CE::Rendering::Pipelines::Definition definition;
    definition.vertex_layout = CE::Rendering::Geometry::Layout2D::Position3UV2Color4;
    definition.topology = CE::Rendering::Geometry::Topology::Triangles;
    definition.state = {CE::Rendering::Pipelines::Blend::PremultipliedAlpha,
                        CE::Rendering::Pipelines::Depth::Disabled, false,
                        CE::Rendering::Pipelines::Cull::None};
    definition.parameters = {{"projection", CE::Rendering::Parameters::Type::Mat4, true,
                              CE::Rendering::Parameters::Semantic::Projection}};
    definition.program_sources = {root / "graphics/shaders/tgui.vert",
                                  root / "graphics/shaders/tgui-solid.frag"};
    const CE::Rendering::OpenGL::PipelineBindings solid{{{"projection", "projectionMatrix"}}};
    CE::UI::RmlUi::Materials result;
    result.solid = native.build_material({native.build_pipeline(definition, solid), {}});
    definition.program_sources[1] = root / "graphics/shaders/tgui-textured.frag";
    definition.parameters.push_back({"image", CE::Rendering::Parameters::Type::Sampler2D});
    const CE::Rendering::OpenGL::PipelineBindings textured{
        {{"projection", "projectionMatrix"}, {"image", "mytexture"}}
    };
    result.textured = native.build_material({native.build_pipeline(std::move(definition), textured), {}});
    return result;
}
```

On platform during `init()`, after renderer initialization:

```cpp
auto* native = dynamic_cast<CE::Rendering::OpenGL::ResourceProvider*>(&engine_.resources());
if (!native)
    throw std::runtime_error("This material builder requires OpenGL");
materials_ = make_rmlui_materials(*native, root_);
uploader_.emplace(engine_.resources());
```

Another graphics backend supplies compatible materials through its own builder.
The [rendering guide](../../../../docs/rendering/pipelines-and-materials.md#manual-material-construction)
defines parameter contracts and native bindings.

### Create the session on the first update

A concurrent game's UI owner starts with `update()`, after platform-owned `init()`.
Run this block once there. Choose a distinct nonzero routing target for each UI;
this example uses `7`:

```cpp
if (!ui_) {
    ui_ = std::make_unique<CE::UI::RmlUi::Session>(engine_.input(), 7);
    if (!ui_->load_font(font_, "tutorial"))
        throw std::runtime_error("Cannot load the UI font");
    ui_->set_view(tick.logical_size, tick.framebuffer_size);
    document_ = ui_->context().LoadDocumentFromMemory(R"(
        <rml><head><style>
            body { width: 100%; height: 100%; margin: 0;
                   font-family: tutorial; font-size: 18px; }
            input { position: absolute; left: 16px; top: 48px;
                    width: 240px; height: 32px;
                    background-color: #eeeeee; color: #111111; }
        </style></head><body><div>Player name</div>
            <input id="name" type="text" />
        </body></rml>
    )");
    if (!document_)
        throw std::runtime_error("Cannot load the UI document");
    document_->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    auto* field = document_->GetElementById("name");
    if (!field || !field->Focus())
        throw std::runtime_error("Cannot focus the UI field");
    ui_->request_keyboard_focus();
}
```

The document owns its elements; `document_` borrows the session's context. Font
bytes remain valid through Core shutdown. Routing/capture affect later polls, so
text already captured under another target does not move to the new field.

### Process input and publish a scene

After creation, each `update()` supplies copied sizes, simulation time and the
complete tick batch once:

```cpp
ui_->set_view(tick.logical_size, tick.framebuffer_size);
ui_->update_time(tick.delta_seconds);
ui_->handle_input(tick.input.records(), true);
static_cast<void>(CE::UI::RmlUi::adopt_scene(pending_, scene_));
if (!pending_.valid())
    pending_ = uploader_->submit(engine_.platform_dispatcher().submission(), ui_->record(), materials_);
```

`true` explicitly assigns pointer input to this interface. Adopt only a ready,
complete upload; the first scene can be empty while the request is pending.
An upload/cancellation error propagates without replacing the previous scene.
The demo catches that error to report it and continue; propagation here selects
normal runtime failure cleanup. Keeping one pending replacement bounds the queue.

During `prepare_render_frame()`, after world/HUD passes:

```cpp
scene_.write(frame);
```

The frame contains retained packets, never live document operations. To end
editing on the UI owner, call `ui_->release_keyboard_focus()`; native window close
remains available while the field owns keyboard gameplay input. Application focus
shortcuts use the session's ordered `before_record` overload, as in the demo.

### Close after UI quiescence

During `deinit()`, after simulation joins and accepted dependencies settle:

```cpp
document_ = nullptr;
if (ui_) {
    ui_->close_after_quiescence();
    ui_.reset();
}
scene_ = CE::UI::RmlUi::Scene{};
uploader_.reset();
materials_ = CE::UI::RmlUi::Materials{};
```

Detach and release any listeners or toolkit resources you add before closing the
session. Keep the rest of game cleanup tolerant of partial initialization.
[Ownership and Cleanup](../../../../docs/architecture/ownership-cleanup.md)
connects these steps to the runtime's shutdown dependencies.

The field should appear below the label, accept committed text and editing keys,
retain its placement during resize, and close cleanly in both runtime modes. This
minimal interface uses supported rectangular drawing. Clipboard, cursor services,
IME, transforms and advanced effects retain the capability limits below.

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

The application supplies `Materials` with colored triangle pipelines,
`PremultipliedAlpha`, disabled depth/write/culling, a projection semantic and a
custom image sampler for textured draws. Shaders multiply image RGBA by vertex
RGBA without an additional premultiplication. `SceneUploader` belongs to one
provider/domain and platform owner; `submit` transfers owned recording data through
Cheryl's dispatcher. `adopt_scene` never waits and replaces the retained scene only
after a complete successful upload. Failure or cancellation preserves its predecessor.

## Input identities

`keyboard_key` maps portable physical keyboard identities to RmlUi keys; unknown
keys remain `KI_UNKNOWN`. Committed characters come from `TextEvent`, not from
this mapping. `modifiers` preserves Control, Shift, Alt, Super, Caps Lock and Num
Lock flags in RmlUi's modifier representation.

## Material construction

The demo retains manual pipeline/material construction on the graphics owner;
the adapter receives compatible premultiplied materials from its caller. The
[manual example](../../../../docs/rendering/pipelines-and-materials.md#manual-material-construction)
shows typed definitions and explicit uniform wiring. Indexed loading migrates
only main text/image materials. A later UI migration targets TGUI first; migrating
RmlUi also requires preserving a separately usable manual example. Core support
and planned effects remain independently scoped.

## Dependency configuration

It links shared RmlUi Core and `Cheryl::Engine`. Session installs a font engine
whose reviewed upstream provider, atlas, effects and fallback logic live in Core;
the Cheryl adapter supplies Engine rasterization through private callbacks.
Core itself does not link Engine, preserving that boundary with static Cheryl too.
The module provides native document sessions, portable input translation, CPU
recording and retained scene upload. The
[adapter-author guide](../../../../docs/development/ui-adapters.md) describes the
engine boundaries; RmlUi keeps its native context, RML and RCSS authoring API
exported from the shared toolkit library.

This core integration is sufficient for the selected Linux root-composition scope.
Further rendering effects are
[planned extensions](../../../../docs/planning/long-term/README.md#ui-adapters),
including any required graphics-backend support. OS services remain deferred.
Dedicated regression and supplied-Core acceptance gaps remain in the
[startup/UI follow-up](../../../../docs/planning/develop-review-and-development-plan.md#startup-and-ui-follow-up).

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
`WidgetTextInput::CalculateCharacterIndex`. Uncorrected code forms a range into
the empty editable value using the displayed placeholder's line length.

When Cheryl creates Core from `CHERYL_RMLUI_SOURCE` or the bundled source, its
[helper](./cmake/RmlUi.cmake) compiles a generated copy of `WidgetTextInput.cpp` with
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

The [placeholder investigation](../../../../docs/external-work/rmlui-placeholder-issue.md)
preserves the source diagnosis, unmodified-upstream reproduction requirements and
contribution guidance. The Cheryl-owned correction has accepted placeholder
hit-testing observations in the Linux/X11/OpenGL demo; existing session tests do
not directly exercise that correction. Supplied targets/packages still require
independent host acceptance and correction declarations.

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
[UI validation guide](../../../../docs/development/ui-adapters.md#repeating-linux-root-validation).
Native interaction is accepted in the selected Linux root composition; the
[demo procedure](../../../apps/demo/README.md#startup-and-ui-validation) preserves
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
[module guide](../tgui/README.md#focused-checks).

Follow the [demo procedure](../../../apps/demo/README.md#interaction-checks) in
normal and `--concurrent` modes for native alpha/orientation, fonts, focus,
scrolling, bounded resize/DPI, image replacement and shutdown. Controlled cases
do not establish compositor behavior, physical input or GPU resource retirement.
The [demo guide](../../../apps/demo/README.md#interaction-checks) retains selected
platform limits.

For a toolkit upgrade, recheck Core/Engine font-service metadata, premultiplied file
and generated images, font-byte lifetime, placeholder correction/declarations and
retained frames through teardown.
Accept changed behavior independently before extending native consumers or the
cross-toolkit coexistence suite.
