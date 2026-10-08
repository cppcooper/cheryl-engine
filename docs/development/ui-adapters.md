# Writing a UI adapter

A UI adapter is an optional module that translates a toolkit into Cheryl's
existing input, render and resource contracts. The toolkit owns its widgets,
documents, styling and layout. Engine does not include toolkit headers or select
an adapter. The [TGUI](../../projects/modules/ui/tgui/README.md) and
[RmlUi](../../projects/modules/ui/rmlui/README.md) modules provide independent
examples. Their public classes are toolkit-specific, not requirements for other
adapters.

An unknown future toolkit should require an additional adapter module without
introducing its concepts into Engine. Dear ImGui remains a developer/debug tooling
candidate; it does not define player-facing widget architecture.

## Module and application ownership

Give the adapter one library target, `include/`, `src/` and owner-local `tests/`.
Link its public target to `Cheryl::Engine` and the selected toolkit. Follow the
[module guide](modules.md) for supplied targets, standalone composition and
dependency selection. Configure only an explicitly selected dependency; ordinary
CMake configuration does not download it or change a host's toolkit settings.

Keep gameplay state in ordinary application models. A toolkit-specific view owns
native UI objects, reads those models and returns application actions. The game
chooses its adapters, presentation order and input recipients. Adapters do not
depend on each other or define a common widget hierarchy or `iUiSystem`. A shared
runtime abstraction requires evidence from actual implementations.

Before implementing another adapter, select its toolkit/version and map concrete
requirements to Engine's contracts. Resolve font ownership, resource publication,
material construction, lifecycle and unavailable platform services before dependent
code grows. Any required direct graphics/native access is an Engine contract gap;
resolve the smallest demonstrated seam first.

## Bridge responsibilities

| Toolkit need | Cheryl contract | Adapter responsibility |
| --- | --- | --- |
| Draw geometry and clipping | [Render submission](../rendering/pipelines-and-materials.md) | Record ordered, owned colored triangles and logical rectangular clips. Match the toolkit's alpha convention to the supplied material. |
| Images and font atlases | [Consumer resources](../resources/consumer-resource-contract.md) | Copy CPU pixels into immutable generations; publish provider-owned resources on the platform owner. Resolve font ownership and sampling before exposing textures. |
| Keyboard, pointer and text | [Input records and focus](../runtime/input-state-model.md) | Translate portable identities and committed text without native input APIs, invented characters or modification of another reader's records. |
| Time, size and OS services | [Runtime ticks](../runtime/runtime-frame-boundary.md) and capability reporting | Use copied window/framebuffer dimensions and explicit simulation time; report unavailable clipboard, cursor and IME services. |
| Queued upload and retained playback | [Thread dispatch](../runtime/thread-dispatch.md) and [resource lifetime](../resources/resource-lifetime.md) | Send owned preparation through platform submission and retain complete published scenes through replacement and teardown. |

The application supplies backend-appropriate pipelines and materials. An adapter
validates their vertex layout, topology, alpha/depth/culling state, projection
semantic and texture parameters. It does not create OpenGL pipelines or query a
native context. Font rasterization may belong to the toolkit; its resulting atlas
still follows the same CPU-copy and resource-publication boundary.

Expanded triangles suffice for the current adapters. Indexing, batching and draw
sorting require their own consumer need; preserve authored world/HUD/overlay order.
The initial resource scope is one window/provider domain with immutable replacement.
Mutable texture updates, multiple domains and dynamic atlas policies need explicit
publication/lifetime contracts before exposure. The
[display/window scale contract](../runtime/display-and-window-contract.md#scale-and-ui-capabilities)
uses copied dimensions and explicit font scaling
within each adapter's supported scope.

The independent implementations differ where their toolkits differ:

| Concern | TGUI | RmlUi |
| --- | --- | --- |
| Authoring | Native GUI/widgets; optional typed numeric placement and resizing. | Native context/documents; RML and RCSS layout, including percentage bounds and flex. |
| Alpha | Straight vertex/image RGBA and `StraightAlpha` materials. | Premultiplied output and `PremultipliedAlpha` materials; file images convert once. |
| Fonts | Guarded FreeType backend and embedded default font. | Stock Core FreeType engine and explicitly registered owned font files/bytes. |
| Toolkit lifetime | One guarded process-global backend. | One guarded Core initialization with contexts and interfaces. |

Existing Engine contracts cover both initial scopes. Their CPU recordings and
uploaders remain toolkit-owned implementations; matching names do not justify a
common widget or session API. The application chooses compatible materials and
presentation order for each view.

Unsupported renderer features need an explicit scope and failure/capability
contract. Rectangular clipping does not establish arbitrary masks, filters,
offscreen targets or mutable texture updates. Add a generic engine facility only
when an implemented consumer demonstrates the requirement.

## Update, publication and shutdown

1. On platform, prepare application materials and the resource upload endpoint.
2. On simulation/UI, create the toolkit session and native presentation objects.
   Keep all toolkit calls on that owner, including font and layout changes.
3. Each update supplies copied logical/framebuffer sizes, simulation time and the
   immutable input stream. Logical UI coordinates and framebuffer pixels remain
   distinct; zero drawable dimensions produce an empty recording.
4. Record owned CPU draw data, then submit it and retained materials for platform
   upload. Toolkit vertices, pixel pointers, widgets and documents do not cross
   that queue boundary.
5. Adopt a replacement only when complete. A pending or failed replacement leaves
   the current scene usable. Bound outstanding replacements instead of queuing a
   new upload every tick.
6. Frame preparation appends retained draw packets in application order and does
   not read live toolkit objects. Resource handles remain valid while an older
   frame is retained.
7. Stop/join the UI owner, release application-held toolkit objects, then destroy
   its context and global interfaces. A final transfer to platform is serial and
   quiescent. Retained Cheryl frames own resources independently of toolkit state.

## Routing and unavailable services

Use Events capture for ordered physical input and acquire Text capture while a
consumer requests text focus. Focus targets and epochs select keyboard/text
delivery; a later lease can preempt an earlier one. Drain already collected
poll-latched records before releasing a preempted lease. Observe modifier releases
even when the corresponding keyboard record is not selected for widget delivery.

Give each session the complete tick batch once and hold its entry focus epoch
through delivery. Use the session's `before_record` callback for application focus
and visibility controls before each native event; its return selects pointer
delivery for that record. New leases affect future polls, while collected records
retain their earlier targets/epochs. A sequence of one-record calls changes the
preemption boundary and can discard an earlier owner's still-pending text.

Use the pointer coordinates recorded with each button or scroll event. A current
cursor sample must not relocate an earlier click. Pointer delivery is explicitly
selected by the application; keyboard focus alone does not imply pointer capture,
modal arbitration or controller navigation. Gameplay controller input can continue
while an adapter owns keyboard focus.

Unavailable clipboard support must not cause cut/paste shortcuts to delete text.
Committed Unicode characters do not establish IME composition, shaping or
grapheme-aware editing. Keep those capabilities explicit rather than synthesizing
them from physical key presses.

## Acceptance

Keep implementation checks with their module. Use controlled input/window/render
and memory-resource implementations to exercise recording, immutable replacement,
failure, routing and queued sequential/concurrent lifetime behavior. Engine tests
exercise its contracts with cheap dummy implementations.

Configure an independent consumer and public-header probes separately; a root
build cannot establish standalone composition or dependency isolation. Check
native appearance, resizing/DPI, hit positions, editing and shutdown with the
selected renderer as a separate acceptance step. Passing controlled checks does
not establish physical driver or compositor behavior.

For two adapters, give them different focus targets, feed the same immutable
record stream and select pointer recipients explicitly. Verify preemption without
record consumption or stale-lease interference, ordered retained passes, and
resources surviving either toolkit's teardown. Keep shared functionality in
Engine only when these independent implementations demonstrate its purpose.

The repository's [coexistence suite](../../projects/tests/ui-coexist/CMakeLists.txt)
owns that cross-project proof. The [demo](../../projects/apps/demo/README.md)
selects either adapter or both and supplies separate focus controls. Each module's
standalone consumer and implementation checks remain independent.

The [demo interaction guide](../../projects/apps/demo/README.md#interaction-checks)
owns native observations and selected-platform limits. A new backend, platform or
supplied package needs its own applicable proof.
