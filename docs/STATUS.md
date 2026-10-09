# Cheryl Engine — Status and Architecture Review

**Review baseline:** [`65be712f`](https://github.com/cppcooper/cheryl-engine/commit/65be712f03019e50f0c86524f4e64c131926756d) on branch [`engine-foundations-and-consumer-facilities`](https://github.com/cppcooper/cheryl-engine/tree/engine-foundations-and-consumer-facilities) · **Reviewed:** 2026-10-08

> **Scope:** Source-code inspection of the engine, backend modules, build configuration, demo, and test sources. No compilation, tests, hardware QA, or deployment checks were performed as part of this review. These classifications are a snapshot at the reviewed commit, **not automatically updated release guarantees**.

**Classification:** **Partial / QA pending** = implemented functionality with known limitations or outstanding native acceptance; **Deferred** = deliberately postponed, unselected, or blocked work; **Not implemented** = no complete functional implementation identified; **Implemented** = substantive source implementation exists, but does not imply a verified build or universal platform coverage.

The following **four tables** comprise 141 distinct entries. Links are relative to `docs/STATUS.md`, so they follow the branch in which the document is viewed.

## 1. Partial / QA pending

| Feature / integration | Status | Description / scope                                                                                                                                                        | Investigate |
| --- | --- |----------------------------------------------------------------------------------------------------------------------------------------------------------------------------| --- |
| Worker CPU placement | Partial | Linux CPU affinity, required/preferred policies and fallback behavior are implemented; automatic cache-domain discovery and NUMA/NUCA placement are absent.                   | [worker-affinity.cpp](../projects/engine/src/core/engine/worker-affinity.cpp) |
| Native controller input | Partial | Gainput-driven gamepad collection exists, but complete HID detection, reports and lifecycle behavior are not accepted; that backend work is listed separately as deferred. | [input-system.cpp](../projects/modules/platform/native-glfw/src/core/controls/input-system.cpp) |
| Active rendering windows | Partial | A display may own several windows, but switching the active rendering window is explicitly rejected.                                                                       | [display-system.cpp](../projects/modules/platform/native-glfw/src/core/display/display-system.cpp) |
| TGUI texture sampling | Partial | Recorded UI scenes and textures work; nearest-neighbor texture/font sampling is rejected under the present provider contract.                                              | [rendering.cpp](../projects/modules/ui/tgui/src/rendering.cpp) |
| RmlUi rendering effects | Partial | Core scene recording and rectangular clipping exist; advanced effects are not an exposed capability in the current adapter.                                                | [session.h](../projects/modules/ui/rmlui/include/cheryl/ui/rmlui/session.h) |
| Unicode rendering acceptance | QA pending at review | At the review baseline, native appearance, wrapping and replacement remained unobserved; the text guide owns current accepted scope and bidi coverage limits.               | [text guide](assets/text-layout.md#native-acceptance-scope) |
| Native audio acceptance | QA pending at review | At the review baseline, audible stereo output and sustained WAV streaming remained unobserved; the module guide owns current acceptance scope.                            | [audio guide](../projects/modules/audio/miniaudio/README.md#acceptance-boundaries) |
| Sprite/tile showcase acceptance | QA pending at review | At the review baseline, native sample rendering/playback and optional-load behavior needed QA; the demo guide owns current acceptance and shader/resize coverage limits.    | [demo guide](../projects/apps/demo/README.md#tile-and-sprite-samples) |

*8 entries.*

## 2. Deferred

| Feature / integration | Status | Description / scope | Investigate |
| --- | --- | --- | --- |
| Gainput HID backend | Blocked · TR6 | Complete backend initialization, device identity, retained HID reports and lifecycle notification ownership; establish a reliable observation harness. | [HID roadmap](planning/develop-review-and-development-plan.md) |
| Windows native acceptance | Deferred | Verify GLFW/Gainput input, resize/events, controller behavior and HID notifications on Windows; some guarded platform-specific code already exists. | [platform-acceptance.md](planning/long-term/platform-acceptance.md) |
| macOS native acceptance | Deferred | Validate dependency configuration, windowing, input and graphics behavior on macOS. | [platform-acceptance.md](planning/long-term/platform-acceptance.md) |
| Wayland native acceptance | Deferred | Verify GLFW Wayland windows, focus, input and OpenGL presentation; X11 is the active desktop acceptance path. | [platform-acceptance.md](planning/long-term/platform-acceptance.md) |
| Alternative graphics APIs | Deferred | No selected Vulkan, Metal or Direct3D renderer; future modules should test existing neutral graphics/resource contracts. | [README.md](planning/long-term/README.md) |
| Render batching | Deferred | Measure state-switch and draw overhead; batch compatible contiguous draw packets with generation-aware compatibility keys. | [README.md](planning/mid-term/README.md) |
| Reorder-safe render sorting | Deferred | Sorting is not performed; a future policy must identify explicitly reorderable regions and preserve required authored draw order. | [README.md](planning/mid-term/README.md) |
| Text shaping/glyph optimization | Deferred | Profile paragraph shaping, glyph preparation and upload before adding caches or accelerated line fitting. | [README.md](planning/mid-term/README.md) |
| Color emoji glyphs | Deferred | Specify accepted color-font formats, RGBA page handling, material semantics and multi-codepoint fallback after grayscale Unicode QA. | [README.md](planning/mid-term/README.md) |
| Adaptive timing adviser | Deferred | Potential profiling-driven timing recommendations without changing explicit simulation/input policy or silently retiming updates. | [README.md](planning/mid-term/README.md) |
| Steam API services | Deferred | Define application-scoped service session, callback scheduling, SDK selection and integration requirements from a real consumer. | [README.md](planning/long-term/README.md) |
| Steam Input | Deferred | Define controller ownership, device selection and native/Steam interaction; prove SDK-free input policies before transport. | [README.md](planning/long-term/README.md) |
| Multi-source input composition | Deferred | Merge native and optional provider contributions before publishing one logical action snapshot, with precise edge and focus semantics. | [README.md](planning/long-term/README.md) |
| Multiple native input owners | Deferred | Generalize beyond one Gainput-initialized owner and coordinate native notification-window lifetime where demanded by consumers. | [README.md](planning/long-term/README.md) |
| Additional UI toolkit modules | Deferred | Add another toolkit only for a demonstrated consumer requirement; preserve neutral Engine contracts and toolkit-owned widget APIs. | [README.md](planning/long-term/README.md) |
| Alternative audio SDK (e.g., FMOD) | Deferred / unselected | No alternative audio module is selected; the audio contracts admit replacement implementations when a consumer justifies one. | [audio/](../projects/modules/audio/) |
| External Debug terminal | Planned · unscheduled | Developer-facing native terminal/logging interface is scoped separately from the eventual in-game graphical console. | [debug-console.md](planning/unscheduled/debug-console.md) |
| In-game developer console | Deferred | Graphical command/output panel, history, scrolling and input routing are described as a future engine-owned facility. | [README.md](planning/long-term/README.md) |
| Expanded manifest families | Deferred | Broader declarative asset formats remain long-term until a specific application requires their semantics. | [README.md](planning/long-term/README.md) |

*19 entries.*

## 3. Not implemented

| Feature / integration | Status | Description / scope | Investigate |
| --- | --- | --- | --- |
| Android platform module | Not implemented | No Android lifecycle adapter, EGL/mobile rendering path, Android input implementation or packaging target. | [platform/](../projects/modules/platform/) |
| iOS platform module | Not implemented | No iOS lifecycle, Metal/OpenGL ES integration, touch-input implementation or application packaging. | [platform/](../projects/modules/platform/) |
| PlayStation platform module | Not implemented | No console SDK adapter, graphics/input module or platform deployment integration. | [modules/](../projects/modules/) |
| Xbox platform module | Not implemented | No Xbox SDK adapter, graphics/input module or packaging integration. | [modules/](../projects/modules/) |
| Nintendo platform module | Not implemented | No Nintendo SDK adapter, graphics/input module or packaging integration. | [modules/](../projects/modules/) |
| 3D mesh/model rendering | Not implemented | Camera3D projects perspective matrices, but there is no complete mesh/model draw path; the current mesh header is empty. | [mesh.h](../projects/engine/include/cheryl/assets/types/3d/mesh.h) |
| Entity-component system | Not implemented | No built-in entity registry, components/archetypes or prescribed ECS; game logic architecture belongs to the consumer. | [abstract-game.h](../projects/engine/include/cheryl/core/game-framework/abstract-game.h) |
| Tilemap/world system | Not implemented | Autotile selection is provided, but tilemap storage, regions, streaming and world-level map management are not. | [tile-selection.h](../projects/engine/include/cheryl/assets/selection/tile-selection.h) |
| Collision/physics system | Not implemented | No physics world, collision engine, rigid-body simulation or built-in solver. | [cheryl/](../projects/engine/include/cheryl/) |
| Networking/multiplayer | Not implemented | No network transport, game session, replication or network-state reconciliation engine facilities. | [modules/](../projects/modules/) |
| Save-game serialization | Not implemented | Manifest JSON parsing does not constitute an application save-state or serialization subsystem. | [resources/](../projects/engine/include/cheryl/core/resources/) |
| Spatial/positional audio | Not implemented | Audio handles expose gain, looping and playback, but no listener/source coordinates or spatial mixing contract. | [system.h](../projects/engine/include/cheryl/audio/system.h) |
| Automatic asset hot reload | Not implemented | Explicit shader/material replacement exists; the manifest loader has no atomic watch/reload-and-republish mechanism. | [asset-loader.h](../projects/engine/include/cheryl/core/resources/asset-management/asset-loader.h) |
| GPU memory budgets/eviction | Not implemented | Resource handles and caches are implemented, but automatic residency budgets, eviction and GPU-memory policy are absent. | [resource-provider.h](../projects/engine/include/cheryl/assets/resources/resource-provider.h) |
| Independent provider cache domains | Not implemented | Global asset managers currently use one active provider/loading-owner domain; parallel independent cache domains are unsupported. | [asset-mgr.h](../projects/engine/include/cheryl/templates/asset-mgr.h) |
| Installed CMake package | Not implemented | The supported consumer path is the build tree; no installed find_package(Cheryl) package/export workflow exists. | [consuming-engine.md](development/consuming-engine.md) |
| OS clipboard services | Not implemented | Both toolkit adapters reject clipboard API requests rather than reading or writing OS clipboard data. | [session.cpp](../projects/modules/ui/tgui/src/session.cpp) |
| IME composition/preedit | Not implemented | Committed Unicode text is delivered, but multi-stage input-method composition/preedit integration is missing. | [session.cpp](../projects/modules/ui/rmlui/src/session.cpp) |
| Relative pointer capture | Not implemented | Cursor visibility exists, but generalized raw/locked relative pointer capture is not exposed by the window contract. | [window-interface.h](../projects/engine/include/cheryl/core/display/window-interface.h) |
| Toolkit OS cursor styling | Not implemented | Toolkit cursor-style callbacks are no-ops and there is no integrated cursor-style provider. | [session.cpp](../projects/modules/ui/rmlui/src/session.cpp) |
| Gamepad-driven UI navigation | Not implemented | Input records can represent controllers, but no general controller-focus/navigation mapping for TGUI/RmlUi is supplied. | [ui/](../projects/modules/ui/) |
| Monitor hotplug event service | Not implemented | Monitor enumeration exists; an engine-level dynamic monitor-added/removed subscription and lifecycle contract is absent. | [display-system-interface.h](../projects/engine/include/cheryl/core/display/display-system-interface.h) |

*22 entries.*

## 4. Implemented

| Feature / integration | Status | Description / scope | Investigate |
| --- | --- | --- | --- |
| **Runtime and concurrency** | | | |
| EngineContext | Implemented | Owns the compatible display, input, rendering, presentation and resource adapters; controls the session domain. | [engine-context.h](../projects/engine/include/cheryl/core/engine/engine-context.h) |
| AbstractGame lifecycle | Implemented | Consumer hooks for initialization, simulation update, frame preparation, quiescence and teardown. | [abstract-game.h](../projects/engine/include/cheryl/core/game-framework/abstract-game.h) |
| GameRuntime execution modes | Implemented | Coordinates platform polling, simulation, rendering and shutdown in sequential or concurrent mode. | [game-runtime.cpp](../projects/engine/src/core/game-framework/game-runtime.cpp) |
| SimulationScheduler | Implemented | Variable/fixed cadence with bounded recovery, skipped time and catch-up policy. | [simulation-scheduler.h](../projects/engine/include/cheryl/core/game-framework/simulation-scheduler.h) |
| TickContext | Implemented | Provides simulation delta, aggregated input, framebuffer/logical dimensions and stop-request access. | [tick-context.h](../projects/engine/include/cheryl/core/game-framework/tick-context.h) |
| WorkerPool | Implemented | Shared threads with futures, acceptance/closure, draining and exception delivery. | [worker-pool.h](../projects/engine/include/cheryl/core/engine/worker-pool.h) |
| Weighted worker groups | Implemented | Isolated logical workloads with concurrency limits, priority, fair scheduling and statistics. | [worker-pool.cpp](../projects/engine/src/core/engine/worker-pool.cpp) |
| PlatformDispatcher | Implemented | Submits captured work for execution on the native graphics/platform owner. | [platform-dispatcher.h](../projects/engine/include/cheryl/core/engine/platform-dispatcher.h) |
| SimulationDispatcher | Implemented | Delivers queued callbacks on the simulation owner through future-bearing submissions. | [simulation-dispatcher.h](../projects/engine/include/cheryl/core/engine/simulation-dispatcher.h) |
| EventBus | Implemented | Owned event listener registrations, immediate delivery, optional queued delivery and controlled removal. | [event-bus.h](../projects/engine/include/cheryl/core/subsystems/event-bus.h) |
| Typed EventChannel | Implemented | Compile-time payload typing in addition to event-name identity, using the persistent bus. | [event-channel.h](../projects/engine/include/cheryl/core/subsystems/event-channel.h) |
| Runtime/worker diagnostics | Implemented | Domain IDs and counts for updates, input polls, backpressure, dispatch, work queues and failures. | [game-runtime.h](../projects/engine/include/cheryl/core/game-framework/game-runtime.h) |
| Logging subsystem | Implemented | Structured spdlog-backed logging, sink management and synchronized logger open/close behavior. | [log.h](../projects/engine/include/cheryl/core/logging/log.h) |
| Failure/crash reporting | Implemented | Custom exception classes, best-effort failure reports, stack traces and optional native Debug handlers. | [signal-handlers/](../projects/engine/support/signal-handlers/) |
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
| Display/monitor abstraction | Implemented | Monitor enumeration, primary selection, logical and framebuffer sizes and content scale. | [display-system-interface.h](../projects/engine/include/cheryl/core/display/display-system-interface.h) |
| GLFW window modes | Implemented | Native resizing, normal/borderless/fullscreen transitions, close detection and cursor visibility. | [window.cpp](../projects/modules/platform/native-glfw/src/core/display/window.cpp) |
| **Rendering** | | | |
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
| Manifest parser | Implemented | Parses document schema 1.0 and validates references into backend-independent objects. | [manifest-loader.cpp](../projects/engine/src/core/resources/asset-management/manifest-loader.cpp) |
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
| Legacy bitmap/font paths | Implemented | Earlier font types remain usable alongside the new Unicode text facilities. | [2d/](../projects/engine/include/cheryl/assets/types/2d/) |
| System font discovery | Implemented | Searches font roots and selects installed font-file candidates. | [fonts-system.h](../projects/engine/include/cheryl/core/resources/fileio/fonts-system.h) |
| **Ui and audio** | | | |
| TGUI UI bridge | Implemented | Owns toolkit session, widget APIs, input translation, CPU scene recording and scene upload. | [tgui/](../projects/modules/ui/tgui/) |
| RmlUi UI bridge | Implemented | Owns document/context session, keyboard/pointer input, UI geometry capture and scene upload. | [rmlui/](../projects/modules/ui/rmlui/) |
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
| **Build and verification** | | | |
| Modular CMake root | Implemented | C++23 build splits Engine and native graphics/platform/UI/audio owners. | [CMakeLists.txt](../CMakeLists.txt) |
| CMake option selection | Implemented | Root switches for GLFW, OpenGL, TGUI, RmlUi, miniaudio, native input, demo and test modes. | [CherylOptions.cmake](../cmake/CherylOptions.cmake) |
| Named library target aliases | Implemented | Consistent identities such as Cheryl::Engine, Cheryl::NativeGLFW and Cheryl::Audio::Miniaudio. | [CherylTargets.cmake](../cmake/CherylTargets.cmake) |
| Standalone module composition | Implemented | Module entry points assemble their local dependency graph without requiring every other module. | [CherylModules.cmake](../cmake/CherylModules.cmake) |
| Build-tree consumers | Implemented | Embeddable source consumers can use add_subdirectory() and target-linked Engine/module libraries. | [consuming-engine.md](development/consuming-engine.md) |
| GoogleTest suites | Implemented · not run | Engine/module unit, suite, aggregate and optional acceptance runners are declared and have test source. | [CherylTests.cmake](../cmake/CherylTests.cmake) |
| CTest discovery | Implemented · not run | Selected GoogleTest runners are registered with CTest using test discovery. | [CherylTests.cmake](../cmake/CherylTests.cmake) |
| OpenGL native acceptance sources | Implemented · not run | Existing tests exercise clipping, resource lifetime, input recovery, renderer shutdown and failure paths. | [native-opengl.cpp](../projects/modules/graphics/opengl/tests/acceptance/src/native-opengl.cpp) |
| Cross-UI integration test | Implemented · not run | Source/target for a consumer linking both TGUI and RmlUi modules. | [ui-coexist/](../projects/tests/ui-coexist/) |
| Consumer header probes | Implemented · not run | Checks selected public API headers and detects accidental native GL/GLFW leakage into neutral includes. | [CherylConsumers.cmake](../cmake/CherylConsumers.cmake) |
| Native demo application | Implemented | Runnable composition example for input, camera, asset drawing, text and optional UI toolkits. | [main.cpp](../projects/apps/demo/src/main.cpp) |
| Debug sanitizers | Implemented | Non-MSVC Debug configuration enables address/undefined-behavior sanitizers. | [CMakeLists.txt](../CMakeLists.txt) |
| Third-party dependency wiring | Implemented | Pinned submodules plus FreeType, HarfBuzz, ICU, GLM, STB, JSON, spdlog, backward-cpp, CTTI and CLI11. | [CherylDependencies.cmake](../cmake/CherylDependencies.cmake) |

*92 entries.*

## Assessment and direction

The review's qualitative estimate of foundational architecture maturity is
approximately **85%**. It concerns established engine foundations, not completion
of the roadmap, and is not calculated from feature counts, coverage or test results.
The implemented facilities support a desktop 2D consumer; higher-level game services
remain consumer-selected. Close the pending acceptance work, then use a small real
game to establish which extensions and performance changes warrant shared ownership.

The [runtime architecture](runtime/runtime-architecture.md) explains system composition;
[subject guides](README.md) own detailed contracts. The
[roadmap](planning/develop-review-and-development-plan.md) owns sequencing,
[long-term plans](planning/long-term/README.md) retain platform/consumer prerequisites,
and the [testing queue](testing-requests.md) owns pending executable acceptance.

## Status maintenance

Update an entry when its implementation or acceptance changes, checking current
source and linking the relevant request or owning plan. Keep planned work in
[planning/](planning/README.md). For a complete new review, update the baseline/date
and reconsider the qualitative estimate; for a narrower update, identify its scope
rather than implying the whole inventory was re-reviewed.
