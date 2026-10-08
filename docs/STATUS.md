# Cheryl Engine — Status and Architecture Review

**Review baseline:** [`65be712f`](https://github.com/cppcooper/cheryl-engine/commit/65be712f03019e50f0c86524f4e64c131926756d) on branch [`engine-foundations-and-consumer-facilities`](https://github.com/cppcooper/cheryl-engine/tree/engine-foundations-and-consumer-facilities) · **Reviewed:** 2026-10-08

> **Scope:** Source-code inspection of the engine, backend modules, build configuration, demo, and test sources. No compilation, tests, hardware QA, or deployment checks were performed as part of this review. These classifications are a snapshot at the reviewed commit, **not automatically updated release guarantees**.

**Classification:** **Partial / QA pending** = implemented functionality with known limitations or outstanding native acceptance; **Deferred** = deliberately postponed, unselected, or blocked work; **Not implemented** = no complete functional implementation identified; **Implemented** = substantive source implementation exists, but does not imply a verified build or universal platform coverage.

The following **four tables** comprise 141 distinct entries. Links are relative to `docs/STATUS.md`, so they follow the branch in which the document is viewed.

## 1. Partial / QA pending

| Feature / integration | Status | Description / scope | Investigate |
| --- | --- | --- | --- |
| Worker CPU placement | Partial | Linux CPU affinity, required/preferred policies and fallback behavior are implemented; automatic cache-domain discovery and NUMA placement are absent. | [worker-affinity.cpp](../projects/engine/src/core/engine/worker-affinity.cpp) |
| Native controller input | Partial | Gainput-driven gamepad collection exists, but complete HID detection, reports and lifecycle behavior are not accepted; that backend work is listed separately as deferred. | [input-system.cpp](../projects/modules/platform/native-glfw/src/core/controls/input-system.cpp) |
| Active rendering windows | Partial | A display may own several windows, but switching the active rendering window is explicitly rejected. | [display-system.cpp](../projects/modules/platform/native-glfw/src/core/display/display-system.cpp) |
| TGUI texture sampling | Partial | Recorded UI scenes and textures work; nearest-neighbor texture/font sampling is rejected under the present provider contract. | [rendering.cpp](../projects/modules/ui/tgui/src/rendering.cpp) |
| RmlUi rendering effects | Partial | Core scene recording and rectangular clipping exist; advanced effects are not an exposed capability in the current adapter. | [session.h](../projects/modules/ui/rmlui/include/cheryl/ui/rmlui/session.h) |
| Unicode rendering acceptance | QA pending · TR9 | UTF-8, shaping, bidirectional layout, font fallback and grayscale glyph upload are implemented; native appearance, wrapping and replacement still require display QA. | [testing-requests.md](testing-requests.md#tr9-qa-unicode-text-rendering) |
| Native audio acceptance | QA pending · TR11 | The miniaudio module implements device playback, streaming and controls; audible stereo output and sustained WAV streaming remain to be observed. | [testing-requests.md](testing-requests.md#tr11-qa-native-audio-and-streaming) |
| Sprite/tile showcase acceptance | QA pending · TR12 | Static and animated assets are exercised by demo code; artwork-dependent rendering, playback, optional-load behavior and shader replacement need QA. | [testing-requests.md](testing-requests.md#tr12-qa-demo-tiles-and-sprites) |

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

## Comprehensive architecture report

**Foundational architecture maturity: approximately 85% — qualitative estimate**

```text
█████████████████░░░  ~85%
```

*The indicator estimates maturity of the established engine foundations, not completion of all roadmap features. It is not calculated from the number of table rows, code coverage, or test pass rates.*

### Scope and reconciliation

This inventory reconciles the earlier code-area inventory and the subsequent status-grouped classification. It retains distinct, actionable capabilities while collapsing synonymous references to the same contract or subsystem. The four tables contain 8 partial/QA entries, 19 deferred entries, 22 not-implemented entries, and 92 implemented entries: 141 distinct feature records in total. Table 4 is grouped by area but remains one table.

Classification was corrected where a lower-level implementation and a higher-level missing feature had been treated as the same item. CPU Wang/bitmask autotiling is implemented; a persistent tilemap/world framework is not. Shared asset caches and explicit shader/material replacement are implemented; automatic GPU-residency budgets, independent simultaneous provider domains and atomic manifest hot reload are not. Basic native controller input is partial because HID integration is unfinished, whereas the remaining Gainput HID backend work is explicitly deferred/blocked. Native Unicode output, audible playback and the asset showcase have source implementations but are not fully accepted, so their end-to-end QA appears only in Table 1.

Potentially redundant earlier entries such as “OpenGL integration” versus “OpenGL renderer,” and “GLFW window management” versus “native GLFW adapter,” were folded into the best explanatory owner. Separate interfaces remain distinct when their contracts differ: for example, WorkerPool versus WorkerGroup, action binding versus immutable snapshots, shader programs versus pipeline/material recipes, and audio abstraction versus codec decoding. The reviewed branch is an architectural snapshot, not a promise that all features function on every platform.

### Executive assessment

Cheryl is a modular C++23 engine focused on desktop 2D game development. It is beyond the prototype stage in the foundations: game/session lifecycles, simulation scheduling, input snapshots, multi-owner dispatch, CPU render-frame publication, OpenGL drawing, asset/resource lifetimes, Unicode shaping, toolkit UI bridges and independent audio services all have concrete code. This is more substantial than a thin wrapper over GLFW and OpenGL.

The principal unfinished work is concentrated at subsystem boundaries: hardware-specific controller reports, native display/audio acceptance, performance optimizations and cross-platform deployment. Higher-level game services—physics, ECS, world management, networking and serialization—remain consumer-defined or absent. Their absence should not automatically be treated as a flaw in the engine foundation; deciding which belong inside Cheryl requires an actual game consumer and stable contract.

### Runtime architecture and lifecycle contracts

EngineContext owns a selected graph of display, presentation, renderer, resource and input adapters and treats a run as a single session. AbstractGame supplies initialization, update, render-frame preparation, quiescence and destruction hooks; GameRuntime orchestrates them and preserves ownership boundaries. In sequential mode the platform and simulation share a caller; in concurrent mode one simulation worker prepares frames while the platform thread remains responsible for native event polling and graphics.

The concurrent runtime publishes completed render frames rather than asking the graphics thread to inspect mutable game objects. This is an important correctness feature: simulation can advance while a prior immutable frame is in use, and a late frame can be superseded without copying live objects across threads. The frame system keeps storage for reuse and reclaims retained resources on the proper owner. SimulationScheduler independently selects fixed or variable time steps, enforces bounded recovery and reports dropped time to the tick context.

WorkerPool supports shared physical execution capacity and fair work groups with explicit limits, weights and priorities. PlatformDispatcher and SimulationDispatcher make cross-thread work ownership explicit instead of implicitly allowing GPU uploads or game callbacks anywhere. Linux CPU-affinity configuration is present, but topology-aware placement and cross-OS equivalence are not. Runtime and worker diagnostics provide counters suited to later performance work; they are observations, not performance guarantees.

### Input semantics, event delivery and platform constraints

The input design separates device collection, semantic bindings, observation history and simulation consumption. Applications bind logical actions to buttons, chords and analog axes; immutable per-poll snapshots preserve transitions. PollingBacklog supports bounded and unbounded polling policies, and InputAccumulator produces TickInput from completed polls. Ordered InputRecord data supports keyboard, pointer and committed text, while capture/focus leases route ownership between gameplay and UI.

This model is more robust than reading keyboard state once per frame: short press/release transitions can survive between simulation updates, and observation time remains distinct from selected simulation delta. The model does not by itself imply that every physical device or OS feature is integrated. Current native code uses GLFW and Gainput; HID reporting and lifecycle evidence are explicitly blocked, while generalized multi-provider composition is a later architectural extension.

Display APIs expose monitor enumeration, framebuffer/logical dimensions, window resizing and normal/borderless/fullscreen modes. Present limitations include one active rendering window and absent clipboard, IME preedit and generalized pointer capture/cursor styling in toolkit sessions. Those are discrete integration gaps, not deficiencies in button-action snapshots.

### Rendering model, resource ownership and performance

The neutral renderer consumes ordered RenderPass and DrawPacket2D values. The packet resolves geometry, material, vertex range, clipping and parameter values before playback, allowing simulation-side frame assembly with no live sprite/camera inspection by OpenGL. Camera2D and Camera3D provide matrix semantics; only the 2D asset/render pipeline is complete. The placeholder mesh header makes it inappropriate to call Cheryl a full 3D engine.

The OpenGL module supplies context management, GL function loading, shader compilation, material/pipeline state, textures, colored and UV geometry, clipping and render-frame playback. ResourceProvider separates transient CPU buffers from backend uploads. Shared retained handles allow a published frame to use older shader/material generations while replacement generations are introduced. Deferred GPU retirement respects context lifetime and owner-thread constraints.

Performance enhancements are intentionally postponed: current playback preserves authored order rather than optimistically sorting, and no draw-packet batching or automatic GPU residency eviction is provided. These are good candidates for profiling-driven work only after representative games establish state-switch, draw and publication costs. As implemented, safe ordering and resource lifetime are prioritized over speculative throughput optimizations.

### Asset pipeline, animation, tiles and text

The asset path begins with backend-independent manifests, schemas and parsed definitions. Loader prepares validated manifests and decoded images on CPU, then serializes upload/publication through the provider owner. Texture, sprite, tileset, shader and material managers handle cached resources, with the important limitation of one active global provider/cache domain. There is no general atomic hot-reload transaction, multi-domain cache isolation or automatic memory-budget eviction.

Sprite assets include named clips, facings, pivots, views and playback cursors. Tilesets support indexed cells, static and animated targets, and independent animation resolution. Wang and bitmask autotiling is real implemented CPU selection—including weighted alternatives and caller-sampled terrain—but it does not instantiate or persist a tilemap world. This distinction helps prevent unnecessary expansion of the asset-layer contract.

Unicode work includes UTF-8 scalar mapping, font-file and installed-font discovery, fallback collections, HarfBuzz shaping, ICU bidirectional/grapheme logic, grayscale glyph atlas preparation and upload into retained draw resources. That is meaningful implementation depth. TR9 nonetheless requires visible QA for glyph appearance, wrapping, fallback and replacement in both runtime modes. Color emoji and large reusable glyph caches are intentionally later work.

### UI and audio integration

TGUI and RmlUi are independent modules that adapt native toolkit authoring into Cheryl scene recordings, with selected input focus and controlled platform-side upload. Both support useful widget/document authoring without making the neutral engine depend on a specific toolkit. They differ in content and alpha semantics: TGUI integrates its widget backend, while RmlUi integrates RML/RCSS documents and premultiplied scene rendering. Both constrain OS clipboard and cursor integration, and RmlUi reports unsupported advanced effects.

Audio follows a separate module boundary from rendering. The neutral Clip, Voice and System interfaces model PCM storage, voice playback and system lifecycle. miniaudio provides WAV/FLAC/MP3 decoding, native or offline systems, streaming, looping, volume control and synchronized voice state. The source includes focused tests for offline mixing and decoding, but a successful offline mixer does not certify live sound output; TR11 still asks for audible native and sustained streaming observations.

### Build system, consumers and verification confidence

The root CMake configuration selects native GLFW, OpenGL and TGUI by default, with RmlUi and miniaudio opt-in. Public target aliases give consumers named library identities, while each module owns its SDK dependencies and build tests. Neutral engine headers are checked against accidental native SDK leakage. The current supported integration mechanism is source/build-tree composition with add_subdirectory; installed find_package(Cheryl) distribution is not implemented.

GoogleTest unit/aggregate targets, CTest registration, consumer header probes, native OpenGL acceptance tests and cross-UI coexistence checks exist in the repository. Source presence is evidence of test infrastructure and intended coverage, not proof of a passing build at this commit. This review deliberately did not compile code or run tests. The active acceptance queue is narrower: TR6 for HID is blocked; TR9 Unicode, TR11 audio and TR12 demo assets are ready for user-run native QA. Documentation and code agree on that distinction.

### Platform readiness and product direction

Linux/X11 is the active native acceptance platform, not a guarantee that every GLFW-supported operating system is already accepted. Windows, macOS and Wayland validation have been moved out of the near-term gate. Android and iOS require platform lifecycle, window/input, rendering and distribution adapters that do not exist in this branch. Console support would add proprietary SDK/toolchain constraints beyond the current freely selectable module graph.

Mobile should be viewed as a platform-product milestone rather than a simple cross-compile exercise. The existing neutral runtime, input interfaces and render/resource contracts provide useful seams, but touch and lifecycle behavior, graphics context choice, suspend/resume, application assets and packaging all need consumer-proven integration. A future console-specific binary module could similarly preserve neutral public contracts while isolating proprietary SDK code, but this is an architectural option, not implemented functionality.

### Priorities, dependencies and risks

First, close the finite existing QA items with evidence: glyph/text rendering in both runtime modes, audible audio/streaming and the sprite/tile showcase. These observations test the actual consumer path and can expose integration bugs that unit tests will not. Resolve Gainput HID as a separately planned backend task rather than conflating it with generic action mapping. Native tests should remain tied to the reviewed source/configuration and their known platform assumptions.

Second, exercise the engine through a small real 2D game rather than extending abstractions on speculation. Such a consumer can validate asset layout, input capture/focus, scene publication, audio ownership, and the suitability of current world/gameplay organization. It will also give representative data for batching, glyph caches, update pacing and graphics-resource policy. Introduce ECS, physics or world facilities only when their reusable contracts are established by more than their presence in a feature checklist.

Third, define mobile/other platform work as isolated milestones: selected backend and device target; input/window/lifecycle integration; end-to-end demo acceptance; reproducible consumer build; eventual store packaging. Avoid presenting deferred Windows/Wayland QA or the existence of a perspective camera as proof of production readiness on those platforms or for 3D games.

### Conclusion

Cheryl already has a coherent 2D engine foundation with especially explicit contracts around scheduling, input snapshots, retained render frames, resource ownership and modular native SDK integration. The substantial work left is best understood as acceptance, selected capability development and deployment reach—not as an incomplete basic engine loop.

The actionable interpretation of the four tables is to retain proven engine interfaces, finish visible/hardware acceptance, then let one or more real game consumers establish which extensions deserve shared engine ownership. This preserves the value of the existing architecture without mistaking long-term optional features for prerequisites to shipping a small desktop game.

### Source references and review limits

Primary evidence: public headers and corresponding implementation files under projects/engine and projects/modules; root and module CMake configuration; demo and test sources; docs/testing-requests.md; planning/long-term and planning/mid-term guidance. Each inventory row links to a repository-relative source file or directory. The exact audited branch head is 65be712f03019e50f0c86524f4e64c131926756d (8 October 2026).

Implementation statuses describe source-backed capabilities and explicit code-contract limitations. They do not establish that native functionality was compiled, run or accepted on untested operating systems. This review made no repository changes and did not compile or execute tests. Updating this Markdown file in the repository is a separate documentation change.

### Status maintenance

Update this inventory when the **implementation or acceptance status** of an entry changes. Prefer current source behavior over potentially outdated prose, and link any changes in platform/native acceptance to [`testing-requests.md`](testing-requests.md) and the appropriate planning document. Keep planned work in [`planning/`](planning/) rather than treating this inventory as an authoritative task queue. When conducting a new complete review, update the baseline commit/date and re-evaluate the qualitative maturity estimate.
