# Cheryl Engine — Status and Architecture Review

**Review baseline:** [`65be712f`](https://github.com/cppcooper/cheryl-engine/commit/65be712f03019e50f0c86524f4e64c131926756d) on branch [`engine-foundations-and-consumer-facilities`](https://github.com/cppcooper/cheryl-engine/tree/engine-foundations-and-consumer-facilities) · **Reviewed:** 2026-10-08

> **Original review scope:** Source-code inspection of the engine, backend modules, build configuration, demo, and test sources. No compilation, tests, hardware QA, or deployment checks were performed as part of that review. The inventory starts at the reviewed commit, with the focused reconciliation below; classifications are **not automatically updated release guarantees**.

**Documentation reconciliation:** 2026-10-12 — Reconciles cache-aware workers,
C++/JSON configuration, opt-in positioned text runs, Linux/Windows CI and prebuilt
SDKs through source baseline `b49ffd9`, alongside the existing sampling/material,
terminal and asset contracts. Selected local/hosted acceptance and pending artifact,
native and QA coverage remain distinct. The original review baseline and qualitative estimate
remain historical; this pass does not claim a fresh review of every subsystem or
new build/test/native results.

Independent Engine-only graph inspection and the Linux terminal configuration
matrix are accepted. Hosted Linux/Windows builds and relocated SDK consumers are
accepted within the [CI coverage limits](site/development/ci.md).
The first main-push SDK collection remains
[TR20](testing-requests.md#tr20-automated-main-push-sdk-collection); terminal desktop
and demo material/artwork observations remain pending. These acceptance updates do
not extend the original implementation review baseline.

**Planning update:** 2026-10-12 — Selects three dependency reviews, RmlUi effects,
reorder-safe sorting, color emoji, an alternative audio SDK and the in-game console.
Deferred work includes absent capabilities and Windows worker affinity in phase 4;
this scheduling update does not change the implementation review baseline above.

**Planning consolidation:** 2026-10-18 — This inventory owns selected work and
deferred review order. The [planning catalogue](planning/README.md) routes to
subject prerequisites and unfinished checklists. This consolidation does not
extend implementation or executable acceptance.

**Event integration planning:** 2026-10-18 — Adds the
[engine/module event plan](planning/short-term/engine-events.md), beginning with
coherent window/view integration. Additional native capabilities remain deferred;
this selection does not change the implementation review baseline.

**Classification:** **Partial** = implemented functionality with a selected integration gap; **Planned** = selected work awaiting investigation, design or implementation; **Deferred** = deliberately postponed, unselected, or blocked work, including capabilities with no complete implementation; **Implemented** = substantive source implementation exists, but does not imply a verified build or universal platform coverage.

The following **four classification sections** comprise 147 distinct entries. Links are relative to `docs/STATUS.md`, so they follow the branch in which the document is viewed.

## 1. Partial

No current entries in this classification.

*0 entries.*

## 2. Planned

These tasks are selected for investigation and design. Each feature needs settled
scope and acceptance before implementation. Selection does not promote deferred
general batching or other integrations, authorize implementation or establish
executable acceptance. Subject plans own the prerequisites and design checkpoints.

| Feature / integration | Status | Description / scope | Investigate |
| --- | --- | --- | --- |
| GoogleTest submodule review | Planned | Review upstream history since the pinned revision, assess test/CMake/toolchain compatibility and decide whether to update the submodule; validate affected test owners if an update is selected. | [dependency reviews](planning/README.md#dependency-reviews) |
| nlohmann/json submodule review | Planned | Review upstream history since the pinned revision and assess parsing, serialization, configuration and package compatibility before deciding whether to update the JSON submodule. | [dependency reviews](planning/README.md#dependency-reviews) |
| spdlog submodule review | Planned | Review upstream history since the pinned revision and assess logging/sink behavior, formatting dependencies, exported targets and toolchain compatibility before deciding whether to update the submodule. | [dependency reviews](planning/README.md#dependency-reviews) |
| RmlUi rendering effects | Planned | Core document/font/input/clipping and retained-upload integration exists. Plan masks, transforms, layers, filters, custom shaders and repeating texture coordinates, including any neutral rendering-contract and graphics-backend changes needed to support them. | [plan](planning/long-term/README.md#ui-adapters) |
| Reorder-safe render sorting | Planned | Sorting is not performed; identify explicitly reorderable regions and preserve required authored draw order, clipping and retained generations before implementing a measured policy. | [sorting scope](planning/mid-term/README.md#reorder-safe-render-sorting) |
| Color emoji glyphs | Planned | Specify accepted color-font formats, RGBA page handling, material semantics and multi-codepoint fallback beyond the accepted initial grayscale scope. | [emoji scope](planning/mid-term/README.md#color-emoji) |
| Alternative audio SDK (e.g., FMOD) | Planned · SDK selection pending | Select and assess an alternative SDK/module against the existing audio contracts, including licensing, deployment, coexistence and any additional authoring requirements. | [plan](planning/long-term/README.md#alternative-audio-sdk) |
| In-game developer console | Planned | Define and implement an engine-owned graphical command/output panel with history, scrolling and input routing, independently of the optional native terminal. | [plan](planning/long-term/README.md#graphical-developer-console) |
| Engine and module event integration | Planned | Implement 18 justified event contract families, beginning with committed window/view integration and explicit automatic UI sizing; follow with lifecycle/shutdown, module phases and resource/media observations. Preserve legacy resize and defer new native capabilities. | [event plan](planning/short-term/engine-events.md) |

*9 entries.*

## 3. Deferred

Phases describe future review order within this section, not release milestones,
delivery dates or authorization to implement. Rows retain their existing order
within each phase.

- **Phase 1:** General render batching, the Gainput HID backend and manual collision checks for object clipping.
- **Phase 2:** Save-game serialization, physics, tilemap/world and entity-component systems, alongside other deferred engine/module extensions and Windows native acceptance.
- **Phase 3:** Cache-related optimization, residency and ownership work, plus the monitor hotplug event service.
- **Phase 4:** Wayland and macOS acceptance, Windows worker affinity, Steam API services, OS/UI input services and spatial audio.
- **Phase 5:** Other platform modules, 3D mesh/model rendering, networking and automatic asset hot reload.

| Feature / integration | Status | Description / scope | Investigate |
| --- | --- | --- | --- |
| General render batching | Deferred · phase 1 · unselected extension | Arbitrary contiguous packet merging requires a separate measured design; opt-in positioned text runs are implemented without sorting or changing UI packet policy. | [batching scope](planning/mid-term/README.md#general-render-batching), [text runs](site/assets/text-layout.md#positioned-text-runs) |
| Gainput HID backend | Deferred · phase 1 · blocked TR6 | Backend initialization, device identity, retained reports and lifecycle notifications remain unfinished. HID acceptance is deferred with backend work and the observation harness. | [HID plan](planning/long-term/README.md#deferred-hid-integration) |
| Manual collision checks (clipping) | Deferred · phase 1 | No engine-level manual overlap/intersection queries for detecting object clipping; specify shape, coordinate and boundary rules independently of physics simulation. | [plan](planning/long-term/README.md#game-systems) |
| Multiple active rendering windows | Deferred · phase 2 | A display can own several windows, but switching the selected rendering window rejects. Multiple active contexts/presentation and their ownership require a separately selected extension. | [window plan](planning/long-term/README.md#multiple-active-rendering-windows) |
| Windows native acceptance | Deferred · phase 2 | Verify GLFW/Gainput input, resize/events, controller behavior and HID notifications on Windows; some guarded platform-specific code already exists. | [platform-acceptance.md](planning/long-term/platform-acceptance.md) |
| Alternative graphics APIs | Deferred · phase 2 | No selected Vulkan, Metal or Direct3D renderer; future modules should test existing neutral graphics/resource contracts. | [plan](planning/long-term/README.md#additional-graphics-backends) |
| Adaptive timing adviser | Deferred · phase 2 | Potential profiling-driven timing recommendations without changing explicit simulation/input policy or silently retiming updates. | [plan](planning/mid-term/README.md#measured-optimization) |
| Steam Input | Deferred · phase 2 | Define controller ownership, device selection and native/Steam interaction; prove SDK-free input policies before transport. | [plan](planning/long-term/README.md#steam-api-and-input-integration) |
| Multi-source input composition | Deferred · phase 2 | Merge native and optional provider contributions before publishing one logical action snapshot, with precise edge and focus semantics. | [plan](planning/long-term/README.md#policy-before-sdk-transport) |
| Multiple native input owners | Deferred · phase 2 | Generalize beyond one Gainput-initialized owner and coordinate native notification-window lifetime where demanded by consumers. | [plan](planning/long-term/README.md#broader-native-input-ownership) |
| Additional UI toolkit modules | Deferred · phase 2 | Add another toolkit only for a demonstrated consumer requirement; preserve neutral Engine contracts and toolkit-owned widget APIs. | [plan](planning/long-term/README.md#ui-adapters) |
| Expanded manifest families | Deferred · phase 2 | Broader declarative asset formats remain long-term until a specific application requires their semantics. | [plan](planning/README.md#topics-awaiting-plans) |
| Entity-component system | Deferred · phase 2 | No built-in entity registry, components/archetypes or prescribed ECS; game logic architecture belongs to the consumer. | [abstract-game.h](../projects/engine/include/cheryl/core/game-framework/abstract-game.h) |
| Tilemap/world system | Deferred · phase 2 | Autotile selection is provided, but tilemap storage, regions, streaming and world-level map management are not. | [tile-selection.h](../projects/engine/include/cheryl/assets/selection/tile-selection.h) |
| Physics system | Deferred · phase 2 | No physics world, rigid-body simulation or built-in solver; this simulation scope is separate from manual collision queries. | [cheryl/](../projects/engine/include/cheryl/) |
| Save-game serialization | Deferred · phase 2 | Manifest JSON parsing does not constitute an application save-state or serialization subsystem. | [resources/](../projects/engine/include/cheryl/core/resources/) |
| Text shaping/glyph optimization | Deferred · phase 3 | Profile paragraph shaping, glyph preparation and upload before adding caches or accelerated line fitting. | [plan](planning/mid-term/README.md#measured-optimization) |
| GPU memory budgets/eviction | Deferred · phase 3 | Resource handles and caches are implemented, but automatic residency budgets, eviction and GPU-memory policy are absent. | [plan](planning/long-term/README.md#residency-accounting) |
| Independent provider cache domains | Deferred · phase 3 | Global asset managers currently use one active provider/loading-owner domain; parallel independent cache domains are unsupported. | [plan](planning/long-term/README.md#residency-accounting) |
| Monitor hotplug event service | Deferred · phase 3 | Monitor enumeration exists; an engine-level dynamic monitor-added/removed subscription and lifecycle contract is absent. | [plan](planning/long-term/README.md#monitor-hotplug) |
| Windows worker CPU affinity | Deferred · phase 4 | Provide calling-thread CPU binding on Windows while preserving memory placement and inherited CPU eligibility. Preferred policies currently fall back; required CPU/cache locality is unavailable. | [Windows affinity scope](planning/long-term/README.md#windows-worker-affinity) |
| macOS native acceptance | Deferred · phase 4 | Validate dependency configuration, windowing, input and graphics behavior on macOS. | [platform-acceptance.md](planning/long-term/platform-acceptance.md) |
| Wayland native acceptance | Deferred · phase 4 | Verify GLFW Wayland windows, focus, input and OpenGL presentation; X11 is the active desktop acceptance path. | [platform-acceptance.md](planning/long-term/platform-acceptance.md) |
| Steam API services | Deferred · phase 4 | Define application-scoped service session, callback scheduling, SDK selection and integration requirements from a real consumer. | [plan](planning/long-term/README.md#steam-api-and-input-integration) |
| Spatial/positional audio | Deferred · phase 4 | Audio handles expose gain, looping and playback, but no listener/source coordinates or spatial mixing contract. | [system.h](../projects/engine/include/cheryl/audio/system.h) |
| OS clipboard services | Deferred · phase 4 | Both toolkit adapters reject clipboard API requests rather than reading or writing OS clipboard data. | [session.cpp](../projects/modules/ui/tgui/src/session.cpp) |
| IME composition/preedit | Deferred · phase 4 | Committed Unicode text is delivered, but multi-stage input-method composition/preedit integration is missing. | [session.cpp](../projects/modules/ui/rmlui/src/session.cpp) |
| Relative pointer capture | Deferred · phase 4 | Cursor visibility exists, but generalized raw/locked relative pointer capture is not exposed by the window contract. | [window-interface.h](../projects/engine/include/cheryl/core/display/window-interface.h) |
| Toolkit OS cursor styling | Deferred · phase 4 | Toolkit cursor-style callbacks are no-ops and there is no integrated cursor-style provider. | [session.cpp](../projects/modules/ui/rmlui/src/session.cpp) |
| Gamepad-driven UI navigation | Deferred · phase 4 | Input records can represent controllers, but no general controller-focus/navigation mapping for TGUI/RmlUi is supplied. | [ui/](../projects/modules/ui/) |
| Android platform module | Deferred · phase 5 | No Android lifecycle adapter, EGL/mobile rendering path, Android input implementation or packaging target. | [platform/](../projects/modules/platform/) |
| iOS platform module | Deferred · phase 5 | No iOS lifecycle, Metal/OpenGL ES integration, touch-input implementation or application packaging. | [platform/](../projects/modules/platform/) |
| PlayStation platform module | Deferred · phase 5 | No console SDK adapter, graphics/input module or platform deployment integration. | [modules/](../projects/modules/) |
| Xbox platform module | Deferred · phase 5 | No Xbox SDK adapter, graphics/input module or packaging integration. | [modules/](../projects/modules/) |
| Nintendo platform module | Deferred · phase 5 | No Nintendo SDK adapter, graphics/input module or packaging integration. | [modules/](../projects/modules/) |
| 3D mesh/model rendering | Deferred · phase 5 | Camera3D projects perspective matrices, but there is no complete mesh/model draw path; the current mesh header is empty. | [mesh.h](../projects/engine/include/cheryl/assets/types/3d/mesh.h) |
| Networking/multiplayer | Deferred · phase 5 | No network transport, game session, replication or network-state reconciliation engine facilities. | [modules/](../projects/modules/) |
| Automatic asset hot reload | Deferred · phase 5 | Explicit shader/material replacement exists; the manifest loader has no atomic watch/reload-and-republish mechanism. | [asset-loader.h](../projects/engine/include/cheryl/core/resources/asset-management/asset-loader.h) |

*38 entries.*

## 4. Implemented

| Feature / integration | Status | Description / scope | Investigate |
| --- | --- | --- | --- |
| **Runtime and concurrency** | | | |
| EngineContext | Implemented | Owns the compatible display, input, rendering, presentation and resource adapters; controls the session domain. | [engine-context.h](../projects/engine/include/cheryl/core/engine/engine-context.h) |
| AbstractGame lifecycle | Implemented | Consumer hooks for initialization, simulation update, frame preparation, quiescence and teardown. | [abstract-game.h](../projects/engine/include/cheryl/core/game-framework/abstract-game.h) |
| GameRuntime execution modes | Implemented | Coordinates platform polling, simulation, rendering and shutdown in sequential or concurrent mode. | [game-runtime.cpp](../projects/engine/src/core/game-framework/game-runtime.cpp) |
| SimulationScheduler | Implemented | Variable/fixed cadence with bounded recovery, skipped time and catch-up policy. | [simulation-scheduler.h](../projects/engine/include/cheryl/core/game-framework/simulation-scheduler.h) |
| TickContext | Implemented | Provides simulation delta, aggregated input, framebuffer/logical dimensions and stop-request access. | [tick-context.h](../projects/engine/include/cheryl/core/game-framework/tick-context.h) |
| `CE::Workers::Pool` | Implemented | Shared threads with futures, acceptance/closure, draining and exception delivery. | [worker-pool.h](../projects/engine/include/cheryl/core/engine/worker-pool.h) |
| Worker CPU placement | Implemented | Application-owned topology providers, optional runtime hwloc core/cache discovery and calling-thread binding where supported, automatic/reuse/spread policies and validated C++/JSON settings. Windows binding remains deferred to phase 4; NUMA memory placement remains deferred. Topology is not a speedup guarantee. | [worker contract](site/runtime/worker-execution.md) |
| Weighted worker groups | Implemented | Isolated logical workloads with concurrency limits, priority, fair scheduling and statistics. | [worker-pool.cpp](../projects/engine/src/core/engine/worker-pool.cpp) |
| PlatformDispatcher | Implemented | Submits captured work for execution on the native graphics/platform owner. | [platform-dispatcher.h](../projects/engine/include/cheryl/core/engine/platform-dispatcher.h) |
| SimulationDispatcher | Implemented | Delivers queued callbacks on the simulation owner through future-bearing submissions. | [simulation-dispatcher.h](../projects/engine/include/cheryl/core/engine/simulation-dispatcher.h) |
| EventBus | Implemented | Owned event listener registrations, immediate delivery, optional queued delivery and controlled removal. | [event-bus.h](../projects/engine/include/cheryl/core/subsystems/event-bus.h) |
| Typed EventChannel | Implemented | Compile-time payload typing in addition to event-name identity, using the persistent bus. | [event-channel.h](../projects/engine/include/cheryl/core/subsystems/event-channel.h) |
| Runtime/worker diagnostics | Implemented | Domain IDs and counts for updates, input polls, backpressure, dispatch, work queues and failures. | [game-runtime.h](../projects/engine/include/cheryl/core/game-framework/game-runtime.h) |
| Logging subsystem | Implemented | Structured spdlog-backed logging, sink management and synchronized logger open/close behavior. | [log.h](../projects/engine/include/cheryl/core/logging/log.h) |
| Failure/crash reporting | Implemented | Custom exception classes, best-effort failure reports, stack traces and optional native Debug handlers. | [crash bootstrap](../projects/engine/private/diagnostics/src/main.cpp) |
| **Input and display** | | | |
| Neutral input system API | Implemented | Defines native adapter lifecycle, device IDs, capabilities and immutable published input views. | [input-interface.h](../projects/engine/include/cheryl/core/controls/input-interface.h) |
| Action/chord binding | Implemented | Maps physical controls to game-defined buttons and axes; supports chords and binding changes. | [input-bindings.h](../projects/engine/include/cheryl/core/controls/input-bindings.h) |
| Button activity and timing | Implemented | Tracks held/pressed/released states, transition counts and observed press durations. | [action-snapshot.h](../projects/engine/include/cheryl/core/controls/action-snapshot.h) |
| Absolute/relative axes | Implemented | Analog state and delta actions with physical button/axis mapping into logical controls. | [input-bindings.h](../projects/engine/include/cheryl/core/controls/input-bindings.h) |
| Immutable poll snapshots | Implemented | Publishes stable per-poll action and input snapshots for concurrent consumption. | [poll-snapshot.h](../projects/engine/include/cheryl/core/controls/poll-snapshot.h) |
| Input polling backlog | Implemented | Lockstep, finite and unlimited policies with capacity and observation spacing controls. | [polling-backlog.h](../projects/engine/include/cheryl/core/controls/polling-backlog.h) |
| Tick input aggregation | Implemented | Accumulates physical observations and transitions into simulation-owned tick records. | [input-accumulator.h](../projects/engine/include/cheryl/core/controls/input-accumulator.h) |
| Ordered input records | Implemented | Captures event and text data, timestamps, ordering and gameplay/UI eligibility. | [input-record.h](../projects/engine/include/cheryl/core/controls/input-record.h) |
| Capture leases | Implemented | RAII capture subscriptions for ordered events and committed text. | [input-capture.h](../projects/engine/include/cheryl/core/controls/input-capture.h) |
| Keyboard focus routing | Implemented | Focus IDs, exclusive routing and epoch-aware leases for UI/gameplay consumers. | [input-routing.h](../projects/engine/include/cheryl/core/controls/input-routing.h) |
| GLFW keyboard/mouse adapter | Implemented | Collects GLFW events and maps Gainput/native observations to Cheryl records. | [input-system.cpp](../projects/modules/platform/native-glfw/src/core/controls/input-system.cpp) |
| Native controller input (non-HID) | Implemented | Gainput-driven gamepad collection, retained-state and disconnect reconciliation exist; the Linux joystick path has an accepted HID-disabled baseline. | [native module](site/modules/native-glfw.md#controller-backend-limits) |
| Display/monitor abstraction | Implemented | Monitor enumeration, primary selection, logical and framebuffer sizes and content scale. | [display-system-interface.h](../projects/engine/include/cheryl/core/display/display-system-interface.h) |
| GLFW window modes | Implemented | Native resizing, normal/borderless/fullscreen transitions, close detection and cursor visibility. | [window.cpp](../projects/modules/platform/native-glfw/src/core/display/window.cpp) |
| **Rendering** | | | |
| Positioned text runs | Implemented | Opt-in prepared glyph placement geometry and contiguous atlas-page draws preserve authored order, clipping, sampling and retained generations; arbitrary shader and exact boundary-pixel semantics need the per-glyph path. | [text contract](site/assets/text-layout.md#positioned-text-runs) |
| TGUI texture sampling | Implemented | Texture/font smoothing is copied into retained draws; immutable nearest/linear samplers share image generations without mutation. Toolkit policies disable mipmaps/anisotropy while ordinary engine defaults request anisotropy. Selected Linux sampling/UI checks are accepted; native nearest-font appearance remains separate. | [TGUI contract](site/modules/tgui.md#recording-and-publication), [sampling](site/rendering/pipelines-and-materials.md#immutable-sampling) |
| Shader manifest schema and indexed material loading | Implemented · demo QA pending | Graphics 2.0 indexes select CPU-only program/material recipes, owned source bytes and backend-owned construction. Main text/image recipes migrated; textures and shaders stay independent, with optional asset selections and explicit incremental replacement. Linux CPU/backend checks are accepted; shader-pair failure/recovery QA remains pending. | [manifest guide](site/assets/asset-manifests.md#shadermaterial-definitions), [loading](site/assets/asset-loading.md) |
| Renderer abstraction | Implemented | Backend-neutral lifecycle, frame rendering, viewport, clear and camera matrix operations. | [renderer.h](../projects/engine/include/cheryl/core/rendering/renderer.h) |
| OpenGL backend | Implemented | GL-based pass playback, context validation, native rendering and resource maintenance. | [renderer.cpp](../projects/modules/graphics/opengl/src/backends/opengl/renderer.cpp) |
| GLFW OpenGL context | Implemented | Context creation, activation and proc loading through the GLFW graphics module. | [glfw-context.h](../projects/modules/graphics/opengl/include/cheryl/backends/opengl/glfw-context.h) |
| PresentationSurface | Implemented | Presentation abstraction separating display buffering from renderer operations. | [presentation-surface.h](../projects/engine/include/cheryl/core/rendering/presentation-surface.h) |
| Camera2D | Implemented | Orthographic Y-up projection, view matrices and revision tracking. | [camera.h](../projects/engine/include/cheryl/core/rendering/camera.h) |
| Camera3D | Implemented | Perspective projection and configurable field-of-view/near/far plane, without a full 3D mesh system. | [camera.h](../projects/engine/include/cheryl/core/rendering/camera.h) |
| RenderFrame/pass writers | Implemented | Owned render-pass and draw storage, frame recycling, and separated simulation/render ownership. | [render-frame.h](../projects/engine/include/cheryl/core/rendering/render-frame.h) |
| Resolved draw packets | Implemented | Immutable, validated CPU commands retaining geometry/material handles and copied parameters. | [draw-packet.h](../projects/engine/include/cheryl/core/rendering/draw-packet.h) |
| 2D asset draw resolution | Implemented | Resolves sprite, tile, graphic and text assets into render packets without live simulation queries. | [draw2d.h](../projects/engine/include/cheryl/assets/submission/draw2d.h) |
| Clip regions and scissor | Implemented | Per-draw clipping contracts and backend clip-state handling. | [clip-region.h](../projects/engine/include/cheryl/core/rendering/clip-region.h) |
| GLSL compilation/linking | Implemented | Builds native shader programs from supplied stage sources and bindings. | [glslprogram.cpp](../projects/modules/graphics/opengl/src/backends/opengl/glslprogram.cpp) |
| Graphics pipelines | Implemented | Validated vertex layout, topology, blend/depth/cull state and backend parameter mapping. | [pipeline.cpp](../projects/modules/graphics/opengl/src/backends/opengl/pipeline.cpp) |
| Immutable materials | Implemented | Retained pipeline recipes and layered default/pass/draw parameter resolution. | [pipeline.h](../projects/engine/include/cheryl/assets/resources/pipeline.h) |
| Typed shader parameters | Implemented | Typed values, sampler bindings and projection/view/model/alpha/scale semantics. | [parameters.h](../projects/engine/include/cheryl/assets/resources/parameters.h) |
| GPU resource-provider API | Implemented | Neutral upload interface with OpenGL support for images, font atlases, geometry and programs. | [resource-provider.h](../projects/engine/include/cheryl/assets/resources/resource-provider.h) |
| Native GPU lifetime control | Implemented | Deferred backend retirement, retained frame generations, context ownership and late-release handling. | [resource-lifetime.cpp](../projects/modules/graphics/opengl/src/backends/opengl/resource-lifetime.cpp) |
| Textures/font atlases | Implemented | OpenGL RGBA textures and grayscale glyph-atlas resources with immutable content handles. | [texture.cpp](../projects/modules/graphics/opengl/src/backends/opengl/texture.cpp) |
| Vertex geometry/VAOs | Implemented | OpenGL vertex-array uploads, indexed vertex ranges and colored or UV vertex types. | [vertex-array-object.cpp](../projects/modules/graphics/opengl/src/backends/opengl/vertex-array-object.cpp) |
| Explicit shader/material reload | Implemented | Publishes replacement generations while retained frames continue referencing prior resources. | [material-mgr.cpp](../projects/engine/src/core/resources/asset-management/material-mgr.cpp) |
| **Assets and text** | | | |
| Asset manifest definitions | Implemented | Schema-backed CPU definitions of sprites, tilesets, pivots, grids, animations and texture references. | [manifest.h](../projects/engine/include/cheryl/assets/definitions/manifest.h) |
| Manifest parser | Implemented | Parses sprite/tileset and shader documents selected by graphics indexes 2.0; validates exact registered paths and CPU references before construction. | [manifest-loader.cpp](../projects/engine/src/core/resources/asset-management/manifest-loader.cpp) |
| Asset preparation/upload | Implemented | Separates CPU manifest scan/decode/validation from serialized provider-side GPU upload. | [asset-loader.cpp](../projects/engine/src/core/resources/asset-management/asset-loader.cpp) |
| Shared asset cache contracts | Implemented | Global asset managers retain immutable resource generations with synchronized publication. | [asset-mgr.h](../projects/engine/include/cheryl/templates/asset-mgr.h) |
| Texture manager | Implemented | Normalized-path image cache, exact-key lookup and provider-owner loading checks. | [texture-mgr.h](../projects/engine/include/cheryl/core/resources/asset-management/texture-mgr.h) |
| Sprite definitions/assets | Implemented | Shared immutable texture/grid resources, pivot, orientations, views and clip definitions. | [sprite.h](../projects/engine/include/cheryl/assets/types/2d/sprite.h) |
| Sprite animation cursors | Implemented | Named animation selection, facing filters, independent clock advancement and loop/clamp. | [sprite.cpp](../projects/engine/src/assets/types/2d/sprite.cpp) |
| Tileset resources | Implemented | Indexed static tiles, animated targets, shared geometry and retained metadata. | [tileset.h](../projects/engine/include/cheryl/assets/types/2d/tileset.h) |
| Tile animation resolution | Implemented | Uses durations and caller-owned time to resolve tileset animation frames. | [animation.h](../projects/engine/include/cheryl/assets/definitions/animation.h) |
| CPU autotile selection | Implemented | Wang edge/corner matching, 4-/8-neighbor bitmask matching and deterministic weighted variants. | [tile-selection.h](../projects/engine/include/cheryl/assets/selection/tile-selection.h) |
| Grid geometry and UVs | Implemented | Converts image atlas grid cells into pivot-aware 2D geometry and UV coordinates. | [grid-geometry.h](../projects/engine/include/cheryl/assets/geometry/grid-geometry.h) |
| Image decoding | Implemented | CPU-side decoded RGBA image buffers with resource-provider separation. | [decoded-image.h](../projects/engine/include/cheryl/assets/resources/decoded-image.h) |
| UTF-8 scalar mapping | Implemented | Decodes Unicode scalar values and retains byte/scalar indices as layout source maps. | [utf8.h](../projects/engine/include/cheryl/text/utf8.h) |
| Legacy bitmap/font paths | Implemented | `CE::Text::Legacy::AsciiFont` and deprecated `BitmapFont` remain available; recovered binary widths/atlas restore bitmap-font inputs, while JSON widths are metadata only. | [2d/](../projects/engine/include/cheryl/assets/types/2d/) |
| System font discovery | Implemented | Searches font roots and selects installed font-file candidates. | [fonts-system.h](../projects/engine/include/cheryl/core/resources/fileio/fonts-system.h) |
| **Ui and audio** | | | |
| TGUI UI bridge | Implemented | Owns toolkit session, widget APIs, input translation, CPU scene recording and scene upload. | [tgui/](../projects/modules/ui/tgui/) |
| RmlUi UI bridge | Implemented | Owns native documents/fonts, keyboard/text/pointer input, rectangular clipping and retained scene uploads. Core integration is sufficient to defer further extensions. | [rmlui/](../projects/modules/ui/rmlui/) |
| Nonblocking UI scene adoption | Implemented | Uploads recorded UI resources on graphics owner and adopts completed scene futures without blocking update. | [scene.h](../projects/modules/ui/tgui/include/cheryl/ui/tgui/scene.h) |
| Audio Clip/Voice/System API | Implemented | Backend-neutral immutable PCM clips, synchronized voice control and independent audio lifecycle. | [system.h](../projects/engine/include/cheryl/audio/system.h) |
| WAV/FLAC/MP3 decoding | Implemented | miniaudio-based CPU decode with supported-format checks and bounded output size. | [decode.cpp](../projects/modules/audio/miniaudio/src/decode.cpp) |
| Offline audio mixer | Implemented | Device-free audio output rendered to caller-provided floating-point buffers. | [miniaudio.h](../projects/modules/audio/miniaudio/include/cheryl/backends/miniaudio.h) |
| **Utilities and memory** | | | |
| ObservedVariable | Implemented | Versioned, synchronized values plus immediate caller-thread observer callbacks. | [observed-variables.h](../projects/engine/include/cheryl/templates/observed-variables.h) |
| VersionedVariable | Implemented | Synchronized snapshots and revisions with condition-variable wait-for-change. | [versioned-variable.h](../projects/engine/include/cheryl/templates/versioned-variable.h) |
| StateTracker | Implemented | Tracks successive values, changes and comparable delta. | [state-tracker.h](../projects/engine/include/cheryl/templates/state-tracker.h) |
| StateMachine | Implemented | Typed guarded transitions with exit/action/enter ordering and callbacks. | [state-machine.h](../projects/engine/include/cheryl/templates/state-machine.h) |
| DeltaTime | Implemented | Monotonic interval sampling/check-ins, independent of simulation policy. | [delta.h](../projects/engine/include/cheryl/templates/delta.h) |
| CRTP Frame cursor | Implemented | Index/offset/limit metadata with explicit wrap/clamp selection. | [frame.h](../projects/engine/include/cheryl/assets/types/primitives/frame.h) |
| Pooled memory manager | Implemented | Concurrent bookkeeping for recycled byte ranges, allocation growth and release diagnostics. | [mem-mgr.h](../projects/engine/include/cheryl/core/resources/memory/mem-mgr.h) |
| Typed object pools | Implemented | Constructed-object and raw-block recycling with retained release contexts. | [pool.h](../projects/engine/include/cheryl/core/resources/objects/pool.h) |
| Block transactions | Implemented | Synchronized multi-collection allocation bookkeeping and safe owner release. | [block-transactions.h](../projects/engine/include/cheryl/templates/block-transactions.h) |
| Pivot/anchor geometry | Implemented | Pivot presets, anchor conversion, quad/strip generation and Y-up geometry. | [anchor.h](../projects/engine/include/cheryl/math/anchor.h) |
| General math utilities | Implemented | Byte, pointer, binary, fitting, numeric and time helper functions. | [math/](../projects/engine/include/cheryl/math/) |
| File manager and path I/O | Implemented | Core file-manager interface and path-oriented file operations, distinct from installed font discovery. | [file-mgr.h](../projects/engine/include/cheryl/core/resources/fileio/file-mgr.h) |
| External Debug terminal | Implemented · Linux desktop acceptance pending | Optional Linux module owns process capture/viewer and test-report routing. Selection defaults on for supported single-configuration Debug builds; explicit ON includes every configuration, with non-Debug startup requiring a runtime enable option. Engine supplies an optional startup callback attachment. | [module guide](site/modules/debug-terminal-linux.md), [desktop QA](testing-requests.md#tr18-qa-linux-debug-terminal-on-a-real-desktop) |
| **Build and verification** | | | |
| Installed CMake package | Implemented | CherylSDK component exports prebuilt selected libraries, headers, dependency targets and licenses; selected hosted Linux and Windows installed consumers are accepted. Windows bundles native dependencies. | [SDK consumption](site/development/consuming-engine.md#prebuilt-development-sdks) |
| GitHub CI and SDK artifacts | Implemented | Linux GCC/Clang and Windows MSVC focused builds/tests plus relocated SDK consumers; successful main validation collects compiled development SDK artifacts without applications or release publication. Hosted builds and relocated consumers are accepted with explicit Windows skipped-case limits; first main-push artifact collection remains pending. | [CI matrix](site/development/ci.md), [TR20](testing-requests.md#tr20-automated-main-push-sdk-collection) |
| Modular CMake root | Implemented | C++23 build splits Engine and native graphics/platform/UI/audio owners. | [CMakeLists.txt](../CMakeLists.txt) |
| CMake option selection | Implemented | Root switches for GLFW, OpenGL, TGUI, RmlUi, miniaudio, native input, demo and test modes. | [CherylOptions.cmake](../cmake/CherylOptions.cmake) |
| Named library target aliases | Implemented | Consistent identities such as Cheryl::Engine, Cheryl::NativeGLFW and Cheryl::Audio::Miniaudio. | [CherylTargets.cmake](../cmake/CherylTargets.cmake) |
| Standalone module composition | Implemented | Module entry points assemble their local dependency graph without requiring every other module. | [CherylModules.cmake](../cmake/CherylModules.cmake) |
| Build-tree consumers | Implemented | Embeddable source consumers can use add_subdirectory() and target-linked Engine/module libraries. | [consuming-engine.md](site/development/consuming-engine.md) |
| GoogleTest suites | Implemented | Engine/module unit, suite, aggregate and optional acceptance runners are declared and have test source; subject guides own configuration-specific acceptance. | [CherylTests.cmake](../cmake/CherylTests.cmake) |
| CTest discovery | Implemented | Selected GoogleTest runners are registered with CTest using test discovery. | [CherylTests.cmake](../cmake/CherylTests.cmake) |
| OpenGL native acceptance sources | Implemented | Existing tests exercise clipping, resource lifetime, input recovery, renderer shutdown and failure paths; source presence does not establish every native path. | [native-opengl.cpp](../projects/modules/graphics/opengl/tests/acceptance/src/native-opengl.cpp) |
| Cross-UI integration test | Implemented | Source/target for a consumer linking both TGUI and RmlUi modules; the UI guide records selected root-composition acceptance. | [UI validation](site/development/ui-adapters.md#repeating-linux-root-validation) |
| Consumer header probes | Implemented | Checks selected public API headers and detects accidental native GL/GLFW leakage into neutral includes. | [CherylConsumers.cmake](../cmake/CherylConsumers.cmake) |
| Native demo application | Implemented | Runnable composition example for input, camera, asset drawing, text and optional UI toolkits. | [main.cpp](../projects/apps/demo/src/main.cpp) |
| Debug sanitizers | Implemented | Non-MSVC Debug configuration enables address/undefined-behavior sanitizers. | [CMakeLists.txt](../CMakeLists.txt) |
| Third-party dependency wiring | Implemented | Pinned submodules plus FreeType, HarfBuzz, ICU, GLM, STB, JSON, spdlog, backward-cpp, CTTI and CLI11. | [CherylDependencies.cmake](../cmake/CherylDependencies.cmake) |

*100 entries.*

## Assessment and direction

The original review's qualitative estimate of foundational architecture maturity
was approximately **85%**. It concerned established engine foundations and was not
calculated from feature counts, coverage or test results.
The implemented facilities support a desktop 2D consumer; higher-level game services
remain consumer-selected. Cache/core locality, positioned text submission and CI/SDK
source work are implemented. Selected Linux controlled/native worker, text-run,
sampling/UI and relocated SDK checks are accepted within their
[subject-guide limits](README.md). Hosted Linux/Windows builds and relocated SDK
consumers, the independent Engine-only Release graph and the terminal configuration
matrix are accepted within their recorded limits. Desktop/device acceptance and
the first main-push SDK collection remain separate below. NUMA remains deferred
indefinitely; general packet merging and sorting require separate scope and workload
evidence.

Recorded Linux acceptance covers the initial
[Unicode scope](site/assets/text-layout.md#native-acceptance-scope),
[audio baseline](site/modules/miniaudio.md#acceptance-boundaries)
and [sprite/tile samples](../projects/apps/demo/README.md#tile-and-sprite-samples),
as well as the selected startup/UI composition. Their guides retain meaningful
coverage limits, including bidi observations, compressed streaming, shader-failure
recovery and simultaneous close/resize. Dedicated new regressions and supplied-SDK
composition gaps remain in their owning plans; they are not blanket pending QA.

The [integration guide](site/development/consuming-engine.md#context-assembly) explains system composition;
[subject guides](README.md) own detailed contracts. The
[planning catalogue](planning/README.md) routes to subject plans,
[long-term plans](planning/long-term/README.md) retain platform/consumer prerequisites,
and the [testing queue](testing-requests.md) owns pending executable acceptance.

## Remaining acceptance

Linux/GLFW/X11/OpenGL is the active native platform. Deferred platform coverage
does not gate Linux work. These remaining checks have independent prerequisites:

| Work | Next action and owner |
| --- | --- |
| Main-push SDK collection | Verify the first successful main-push collection gate in [TR20](testing-requests.md#tr20-automated-main-push-sdk-collection). Hosted builds and relocated SDK consumers are accepted; PR/develop/manual runs intentionally skip collection. |
| Hosted Windows skipped cases | Supply a CFF font fixture and a temporary directory on the checkout drive, then accept the affected font-allocation and logging cases. The [CI coverage limits](site/development/ci.md#hosted-windows-coverage-limits) retain the exact scope. |
| Linux Debug terminal | Complete real-desktop [TR18](testing-requests.md#tr18-qa-linux-debug-terminal-on-a-real-desktop) with matching probe binaries, including emulator windows and desktop/IDE launch. |
| Demo material/artwork QA | Complete shader-pair failure/recovery and available artwork observations in [TR19](testing-requests.md#tr19-qa-indexed-demo-materials-and-recovered-graphics). Missing images and showcase fixtures remain blockers. |

The [short-term plan](planning/short-term/README.md) owns unimplemented
font-selection and startup/UI regressions, supplied RmlUi composition acceptance
and the upstream placeholder follow-up. Existing Linux acceptance does not close
those dedicated gaps. The [demo guide](../projects/apps/demo/README.md#repeating-native-qa)
owns reusable native procedures.

## Supporting plans

These plans cover follow-up work, asset content and deferred investigations outside
the feature classification counts above. Each has an owning document or section:

| Work | Readiness | Plan |
| --- | --- | --- |
| Font and startup/UI regression coverage | Existing baseline accepted; dedicated boundary, result-handoff, callback and placeholder cases remain. | [Font selection](planning/short-term/README.md#font-selection-regressions), [Startup/UI](planning/short-term/README.md#startup-and-ui-follow-up) |
| Supplied RmlUi composition | Needs a corrected dependency and independent consuming host. | [Supplied dependency acceptance](planning/short-term/README.md#supplied-rmlui-acceptance) |
| Upstream placeholder correction | Needs an unmodified reproduction and matching regression before submission. | [Upstream follow-up](planning/short-term/README.md#upstream-placeholder-follow-up) |
| Artwork metadata | Needs source evidence or artwork-owner decisions for remaining manifest semantics. | [Unresolved metadata](planning/long-term/asset-manifest-metadata.md) |
| Additional dungeon manifests | Deferred; acquired packages still need separate image inventories and manifests. | [Dungeon packages](planning/long-term/README.md#additional-dungeon-asset-manifests) |
| Atlas padding/repacking | Deferred; scope not selected. | [Atlas scope](planning/long-term/README.md#atlas-padding-and-repacking) |
| NUMA discovery/memory placement | Indefinitely deferred; needs a consumer and multi-node hardware justification. | [NUMA prerequisites](planning/long-term/README.md#numa-locality) |

Some deferred feature entries have only a description or consumer-required deferral,
not a concrete development plan. The [planning gaps](planning/README.md#topics-awaiting-plans)
identify that remaining scoping work; they are not ready implementation plans.

## Status maintenance

Update an entry when its implementation or acceptance changes, checking current
source and linking the relevant request or owning plan. Keep planned work in
[planning/](planning/README.md). For a complete new review, update the baseline/date
and reconsider the qualitative estimate; for a narrower update, identify its scope
rather than implying the whole inventory was re-reviewed.
