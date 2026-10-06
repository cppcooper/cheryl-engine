# Cheryl Engine UI integration strategy

## Purpose

Cheryl should support player-facing and developer UI libraries without coupling
`Cheryl::Engine` to any widget toolkit. A game selects an optional adapter; the
adapter translates the toolkit's rendering, resources, input and platform needs into
Cheryl contracts.

The intended dependency direction is:

```text
game -> UI adapter -> Cheryl::Engine
```

Never:

```text
Cheryl::Engine -> TGUI / RmlUi / ImGui / another widget library
```

TGUI 1.13.0 is the first selected player-facing adapter; its C++ authoring model
fits current development needs. Its [requirements and decisions](tgui-adapter-requirements.md)
define the custom backend, font ownership and initial scope. RmlUi is the leading
second-adapter candidate; Dear ImGui is primarily a developer/debug tooling candidate.

## Abstraction boundary

Cheryl abstracts facilities consumed by UI libraries, not widgets themselves.

Engine-level responsibilities:

- backend-neutral render submission;
- resource creation/publication/lifetime;
- physical input records and committed text;
- focus/routing infrastructure;
- platform/window services and capability reporting;
- lifecycle/threading rules.

Toolkit responsibilities:

- widget hierarchy and layout;
- styling and animation semantics;
- toolkit event model;
- toolkit-specific public APIs.

Do not introduce `Cheryl::Button`, `Cheryl::Panel`, a universal widget hierarchy, or
a common `iUiSystem` merely because adapters have similarly named internal tasks. A
shared runtime abstraction requires evidence from actual implementations.

## Rendering and resources

A UI adapter records neutral rendering data; it does not issue OpenGL/Vulkan calls.
The graphics/backend owner remains the only code that performs backend API work.

Use [the render contract](../rendering/pipelines-and-materials.md) and
[the frame/lifecycle contract](../runtime/runtime-frame-boundary.md) as the current
foundation. Rectangular clipping and explicit neutral vertex color/layout use that
contract. The first probe must establish:

- rectangular clipping/scissor;
- explicit vertex color/layout;
- logical-to-framebuffer clipping coordinates, including resize/content scale;
- world/HUD/overlay/developer ordering without changing authored draw order.

Expanded triangles suffice for the neutral probe. Require indexing only if the
selected toolkit demonstrates a need; UI integration does not imply batching or
arbitrary packet sorting.

Stencil/mask clipping, render targets, filters and custom effects are not baseline
requirements. Add them only when a real adapter requires them.

UI-generated images/font atlases use the engine resource boundary. The selected
baseline is one provider/cache domain with owned CPU preparation and immutable
replacement handles. Mutable texture updates, multiple resource domains and dynamic
atlas policy require explicit lifetime/publication design before exposure. Decide
whether the toolkit or Cheryl rasterizes fonts before designing its bridge; toolkit
rasterization still publishes textures through Cheryl. See
[consumer-resource-contract.md](../resources/consumer-resource-contract.md).

## Input, routing and platform services

Adapters consume Cheryl input, never GLFW/Gainput directly. Preserve the distinction
between state-oriented input, ordered physical events and semantic committed text.
The physical record remains immutable; routing decides which consumer may react.
Capture leases, ordered device/modifier/repeat records, focus epochs and poll-latched
delivery are specified in [input-state-model.md](../runtime/input-state-model.md).

Current keyboard focus is a foundation, not the entire UI routing model. Add pointer
capture, modal priority/propagation and controller-navigation ownership only as the
selected adapter requires them. Multiple UI systems must not need direct knowledge
of each other. Consumer priority/focus must change without reconfiguring physical
collection. Acceptance cases include focused text while controller gameplay
continues, modal gameplay suppression and an overlay capturing only while active.

Potential platform services include clipboard, cursor shape/visibility, window and
framebuffer dimensions, per-window content scale, monotonic time and text-input/IME
lifecycle. Generic APIs must report unavailable capabilities explicitly and must not
expose GLFW handles.

## Module contract

UI integrations follow the optional-owner model documented in
[modules.md](../development/modules.md). Representative targets are:

```text
Cheryl::UI::TGUI
Cheryl::UI::RmlUi
Cheryl::UI::ImGui
```

Each adapter owns its dependency graph and can be selected independently. Building
or consuming the engine without a UI adapter must not discover or link UI toolkit
dependencies. A game may deliberately select more than one adapter.
The first adapter must establish its owner/selection target and an independent
downstream consumer; installed package/export support remains separate work.

An adapter will normally contain toolkit-specific render/resource/input/platform and
lifecycle bridges. Similar responsibilities do not require identical public APIs.
Users should continue to use each toolkit's native widget API.

## Application-side separation

Gameplay/domain state should not store concrete toolkit widgets. Keep presentation
state in toolkit-neutral models such as `HudState`, `InventoryModel`,
`DialogueState` or `SettingsModel`, then bind a toolkit-specific view/controller to
those models. This makes a future UI migration a presentation-layer replacement
rather than a gameplay rewrite.

## Remaining development sequence

The [main development plan's U9](develop-review-and-development-plan.md#u9--prove-generic-ui-facing-facilities-with-a-real-adapter)
owns prerequisites and ordering with the module acceptance work. The
[neutral probe](../development/ui-probe.md) establishes the rendering, resource and
input baseline. The [TGUI module](../../projects/modules/ui/tgui/README.md) owns the
adapter's current source contracts; session and executable acceptance remain below.

Follow these steps in order and update their status while U9 remains active.

- [x] **1. Establish the neutral consumer probe.** The
  [recording and native probes](../development/ui-probe.md) define its controlled
  scene, clipping/color contracts, resource replacement, focused input and coverage
  limits. Reuse that focused acceptance as the adapter's foundation.
- [x] **2. Select the toolkit and resolve requirements.** Select the concrete
  toolkit/version and build a requirements matrix against Cheryl's
  render/resource/input/routing/platform contracts before adapter-specific changes.
  Set font ownership and identify any new lifetime, routing or platform prerequisite.
  Resolve pipeline/atlas bootstrap before adapter code depends on it: the provider
  uploads resources, while current typed OpenGL pipeline construction belongs to
  that module. Choose application-supplied materials or the smallest neutral
  construction seam; the recording pipeline and synthetic atlas do not prove a
  toolkit's rendering/font implementation.
  Unavailable clipboard/cursor/scale/IME services must receive honest capability
  responses; monitor scale does not establish per-window scale changes.
  - [x] Review a pinned TGUI release's custom backend and record its requirements
    against Cheryl. Resolve font ownership, atlas replacement, global toolkit
    lifetime and the supported initial widget scope before implementation.
  - [x] Define application-supplied materials and a platform upload handoff so the
    adapter records retained neutral draws without constructing backend pipelines
    or uploading resources from simulation.
  - [x] Establish portable input identifiers before adapter event translation.
    - [x] Add keyboard/mouse identities to button records without changing State
      binding IDs, native diagnostics or existing aggregate initialization.
    - [x] Populate identities in the native module, including unknown controls,
      and add mapping and capture-to-tick preservation checks.
    - [x] Retain observation-time coordinates on pointer buttons and scrolling.
      The first click after capture starts cannot depend on an earlier movement
      record, and a later cursor sample must not relocate an earlier click.
    - [x] Document the contract and reconcile source versus executable acceptance.
      Portable identifier/position checks are source-complete; their execution
      joins the focused adapter acceptance rather than repeating the probe suites.
    Leave optional platform services and broader routing at roadmap scope in the
    [requirements](tgui-adapter-requirements.md).
  - [x] Define the optional owner, dependency source/version contract and module
    acceptance. Keep Engine-only selection free of toolkit discovery and avoid
    downloading dependencies during ordinary configuration.
- [ ] **3. Implement the first adapter.** Resolve the smallest required generic seam
  before dependent adapter code grows, then implement the toolkit-specific bridges
  as an optional owner. Every proposed OpenGL/GLFW/Gainput bypass is an engine-boundary
  finding. Generalize only reusable requirements, not toolkit-specific concepts.
  - [x] Add the optional TGUI owner, pinned dependency contract, module tests and
    independent consumer. Reuse a supplied toolkit target or package without
    changing host dependency settings; suppress UI selection during Engine bootstrap.
  - [x] Translate selected immutable input records through portable identities,
    preserving repeat/text semantics and observation-time pointer positions. Do
    not synthesize characters or consume another reader's records.
  - [x] Complete repository composition: pin the toolkit under `extern/tgui`,
    select the UI owner in the root's normal assembly and retain explicit opt-out
    and supplied-target/source/package composition. Do not fetch during CMake.
  - [x] Implement owned CPU texture generations and indexed-triangle recording,
    followed by complete platform upload and retained scene adoption. Resolve
    sampling support before textures are exposed.
    - [x] Fix the initial sampling/texture-bound contract; copy RGBA generations
      before toolkit storage changes and reject unsupported smoothing changes.
    - [x] Expand indexed draws into owned colored triangles with transformed
      positions, corrected UV orientation and copied logical clips/view mapping.
    - [x] Upload a complete recording through the platform submission endpoint;
      retain the previous scene until a new result is ready, and preserve it on
      upload failure/cancellation. Add owner-local recording/resource checks.
    Controlled module checks establish recording/resource behavior and FreeType
    glyph growth. Actual queued runtime execution and native appearance remain
    acceptance work.
  - [x] Implement the simulation-owned toolkit session and GUI lifecycle, focus
    leases, modifier snapshots, explicit timing/view updates and capabilities.
    Configure toolkit view/input in logical units and supply copied framebuffer
    ratios for pixel rounding. Enforce the smoothed-only policy before FreeType's
    `setSmooth` changes its own state; its base implementation mutates before
    delegating to a texture. Limit each GUI to one outstanding scene replacement.
    - [x] Copy logical window dimensions into the runtime tick beside framebuffer
      dimensions, with controlled-window checks for sequential/concurrent delivery.
    - [x] Add one explicit session with the custom GUI/backend, guarded FreeType
      sampling, routed input/modifier state and capture/focus leases. Toolkit calls
      stay on one UI owner; final destruction may transfer to the platform owner
      only after simulation has stopped and external widget references are released.
    - [x] Wire a small TGUI panel into the selected demo, using application-built
      materials and the existing retained-scene upload/adoption path. Preserve the
      toolkit-free demo when the optional module is disabled.
    - [x] Accept the module-local input/recording/scene/session checks, including
      unindexed glyphs, exact/fractional clips, atlas growth, focus and serial
      toolkit teardown with retained CPU recordings.
  - [ ] Accept the independent consumer and first-include header probes in a
    separate composition. The consumer checks are authored. The
    [demo procedure](../../projects/apps/demo/README.md) covers the selected
    panel, sequential/concurrent runs, interactions and visual coverage limits.
  - [ ] Accept the selected Engine window-snapshot and portable/native input
    checks. Reuse the accepted module suite and neutral rendering baseline.
- [ ] **4. Prove representative widget behavior.** Exercise a menu with
  nested/overlapping translucent panels, dynamic labels, a tooltip near viewport
  edges, a text field, scrolling and an image. Verify focus/routing and resize/DPI
  behavior as well as appearance.
  The demo provides these widgets. Keep visual inspection separate from native
  input/focus, resize/DPI and teardown acceptance.
  - [x] Clarify the demo's Escape help as releasing keyboard focus and document
    the window close control as the way to end the demo.
  - [x] Accept the demo's visual appearance, including the top TGUI badge fully
    inside its parent panel's content clip. Retain parent clipping for child
    widgets and scrolling.
  - [ ] Add typed placement and optional bounded resizing to the TGUI adapter.
    Keep native widgets and styling; layout configuration belongs to this module.
    - [x] Add `WidgetLayout`, `Offset`, `Scalable` and `Session::set_layout`.
      The [typed layout contract](../../projects/modules/ui/tgui/README.md#typed-layout)
      owns numeric binding, validation, sizing replacement and lifetime semantics.
    - [x] Author controlled layout/resize/lifetime checks and extend independent
      consumer and first-include header coverage.
    - [ ] Refactor demo placement and width-dependent regions to the typed API
      and native numeric bindings. Exercise fixed sizing, relative offsets and
      bounded resizing while keeping text and controls readable.
    - [ ] Accept the focused layout cases and inspect the changed demo's anchors,
      bounds, clipping and input positions across resize/DPI in both runtime modes.
      Reuse the accepted rendering/widget baseline for unaffected behavior.
  - [ ] Verify simultaneous keyboard/pointer input with a separate mouse or with
    the desktop's touchpad suppression disabled, following the
    [demo procedure](../../projects/apps/demo/README.md).
- [ ] **5. Establish lifetime acceptance and document the adapter contract.**
  Establish retained/concurrent frame and resource-teardown acceptance, then write
  the adapter-author guide in current-state documentation. Cover lifecycle/affinity,
  resources, rendering, routed input, platform capabilities and module selection.
  - [x] Author module-owned sequential/concurrent runtime checks with a real TGUI
    session and platform queue, controlled engine adapters, immutable image
    replacement and frames retained through toolkit teardown.
  - [x] Expose `tick.request_stop()` through runtime-owned stop state and author
    Engine contract checks for sequential/concurrent game shutdown, repeated
    requests, stop before run, saved-source lifetime and unbound manual ticks.
  - [ ] Accept those controlled runtime checks alongside the remaining consumer
    and copied-window/input/stop checks in a focused batch. Keep native interaction,
    resource playback and shutdown coverage separate from controlled acceptance.
- [ ] **6. Prove a second independent player-facing adapter.** Select its toolkit
  and requirements, then prove independent composition and the same retained
  render/resource/input/lifetime boundaries without depending on the TGUI adapter.
  Include a small coexistence/focus proof. RmlUi remains the leading candidate;
  choose its version and detailed scope when this phase becomes active.

TGUI acceptance is an intermediate checkpoint. The second adapter may remain
incomplete while other independent work proceeds, but U9 stays open until both
adapter proofs are accepted. It need not be implemented alongside TGUI.

The probe's ASCII atlas does not dictate the toolkit's font implementation.
Composition/preedit, Unicode shaping, grapheme-aware editing, stencil/filter effects,
offscreen targets, mutable updates and multiple domains remain separate requirements;
reopen the relevant resource/text contract before a consumer depends on them.

**Acceptance:** the toolkit uses only Cheryl facilities, with no OpenGL/GLFW types in
its public interface or native calls from simulation/UI code. Physical input is
collected once and routed without destructive mutation. Published frames own their
data/resources through replacement, concurrent playback and teardown. Replacing the
render backend must not require redesigning the adapter's public architecture.

The portable recording probe establishes contract behavior, not native driver or
toolkit conformance. The initial native proof is Linux/GLFW/X11/OpenGL; other
platforms, IME and device coverage need separately scoped acceptance.

## Independent adapter proof

The second adapter is required to complete U9, rather than an optional extension
after it. It implements its own Cheryl bridges; similar responsibilities are not
grounds for a shared widget abstraction or a dependency on TGUI. Coexistence
validates routing between heterogeneous consumers. Preserve each toolkit's native
authoring API and generalize only engine contract gaps demonstrated by both.

Dear ImGui can later reuse the same neutral facilities for diagnostics/tooling while
remaining independently selectable and without defining player-facing architecture.

The core architectural test remains simple: an unknown future UI library should be
integratable by adding an adapter module, not by introducing that library's concepts
or types into `Cheryl::Engine`.
