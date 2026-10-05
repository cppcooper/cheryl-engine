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

TGUI remains the likely first player-facing adapter because its C++ authoring model
fits current development needs. RmlUi remains attractive for a later highly styled
production UI. Dear ImGui is primarily a developer/debug tooling candidate. These are
roles, not dependencies selected by this plan.

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
owns prerequisites and ordering with the module acceptance work. Neither the neutral
probe nor a concrete toolkit adapter is implemented yet.

Follow these steps in order and update their status while U9 remains active.

- [ ] **1. Establish the neutral consumer probe.** Extend the portable recording
  graph with the library-neutral consumer selected for resource work: overlapping
  translucent panels, a rectangularly clipped scroll region, an image, an ASCII
  label and a focused editing target while controller gameplay continues. Reuse
  current render/input/resource contracts, add the missing neutral clipping/color
  facilities and verify alpha blending. This probe needs one provider/window,
  immutable image replacement and committed text, without IME or shaping.
  - [x] Settle clipping and color contracts before dependent probe code: copied
    top-left logical clip edges and extent, finite/ordered validation, nested
    intersection, framebuffer clamping and explicit pixel rounding. Keep the
    existing position/UV layout and add a separate float RGBA vertex layout/upload;
    providers without that layout reject it explicitly.
  - [x] Implement neutral contracts and OpenGL support as one coherent unit.
    Retain authored order, validate before native state changes, reset scissor for
    unclipped draws/full clears, and add checks for scaling, empty clips, layout validation,
    shader attribute mapping and native color/alpha/scissor behavior.
  - [ ] Extend the existing portable runtime recording graph with the probe in both
    execution modes. Exercise owned vertex/image/ASCII-atlas data, clipped panels,
    focused text with controller State, immutable image replacement and retained
    old packets without introducing a widget API or another engine target.
  - [ ] Reconcile current contracts and run the focused Engine/OpenGL acceptance
    after explicit authorization. Reuse existing build directories, batch affected
    targets with one low-priority job, and avoid repeating the full aggregate/demo.
- [ ] **2. Select the toolkit and resolve requirements.** Select the concrete
  toolkit/version and build a requirements matrix against Cheryl's
  render/resource/input/routing/platform contracts before adapter-specific changes.
  Set font ownership and identify any new lifetime, routing or platform prerequisite.
  Unavailable clipboard/cursor/scale/IME services must receive honest capability
  responses; monitor scale does not establish per-window scale changes.
- [ ] **3. Implement the first adapter.** Resolve the smallest required generic seam
  before dependent adapter code grows, then implement the toolkit-specific bridges
  as an optional owner. Every proposed OpenGL/GLFW/Gainput bypass is an engine-boundary
  finding. Generalize only reusable requirements, not toolkit-specific concepts.
- [ ] **4. Prove representative widget behavior.** Exercise a menu with
  nested/overlapping translucent panels, dynamic labels, a tooltip near viewport
  edges, a text field, scrolling and an image. Verify focus/routing and resize/DPI
  behavior as well as appearance.
- [ ] **5. Establish lifetime acceptance and document the adapter contract.**
  Establish retained/concurrent frame and resource-teardown acceptance, then write
  the adapter-author guide in current-state documentation. Cover lifecycle/affinity,
  resources, rendering, routed input, platform capabilities and module selection.

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

## Later proofs

A second independent player-facing adapter, likely RmlUi if useful, is the strongest
proof that the boundary is genuinely multi-library. It must implement its own Cheryl
bridges rather than depend on the first adapter. Coexistence then validates routing
between heterogeneous consumers.

Dear ImGui can later reuse the same neutral facilities for diagnostics/tooling while
remaining independently selectable and without defining player-facing architecture.

The core architectural test remains simple: an unknown future UI library should be
integratable by adding an adapter module, not by introducing that library's concepts
or types into `Cheryl::Engine`.
