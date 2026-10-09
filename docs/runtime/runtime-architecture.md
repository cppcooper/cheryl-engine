# Runtime architecture and backend boundaries

The selected backend is composed through `EngineContext`; `GameRuntime` uses its
display, window, input, presentation, renderer, and resource contracts. The current
GLFW/OpenGL implementation and the in-memory integration probe follow those same
contracts. The [application guide](../development/consuming-engine.md#game-hooks-and-ownership)
shows composition and game hooks; [architecture validation](../development/architecture-validation.md)
describes isolation and integration checks.

Applications may use [command-line startup](../development/consuming-engine.md#command-line-startup)
to validate execution, polling, timing and backend options before constructing a
context. The result owns that context and supplies the runtime configuration;
`GameRuntime::run()` still owns adapter/game initialization and shutdown.
Startup support is separate from the Engine and OpenGL libraries and does not
select a backend dynamically.

| Boundary | Contract |
| --- | --- |
| CPU asset data | Vertex layouts, typed manifests, grid generation, and owned RGBA decoding have no graphics-context dependency. `Loader::prepare()` runs independently of a provider. |
| Display and presentation | `iDisplaySystem` owns a selected window implementation. `iWindow` and monitor snapshots carry neutral data; `iPresentationSurface` presents. GLFW handles remain inside the platform/backend implementation. The renderer does not own the display. |
| Input | `iInputSystem` supplies State plus independently requested ordered Events/Text. GLFW/Gainput translation stays in its adapter; games consume tick values and routed views. The default factory owns input; an explicit-reference overload borrows it. |
| Draw and material | Simulation writes complete ordered `RenderFrame` passes. CPU submission helpers resolve sprite, tile, graphic, and glyph packets retaining geometry/material generations and copied values. `GLSLPipelineBindings` maps contract keys to native names; each pipeline applies full fixed state. |
| Resource creation and caches | `ResourceProvider` uploads decoded pixels and transient vertex spans, creates font atlases, and links programs. Cache readers retain handles under shared locks; construction and retired-handle destruction stay outside locks. One provider/loading thread binds the singleton caches until teardown. |
| Composition and scheduling | One runtime session owns platform polling and presentation. Concurrent mode adds one simulation worker and three reusable frame slots. `platform_dispatcher().submit()` transfers owned resource requests with future results; shutdown cancels pending requests before game cleanup. |
| OpenGL lifetime | Active resource use requires the owner thread and its actual current context. Handles retire without OpenGL calls from their destructors. Renderer shutdown restores its context, deletes tracked handles, closes their lifetime, and releases the context. Failed destructor cleanup invalidates retained handles. |
| Portable integration | The in-memory display/window, input, presentation, renderer, and resource provider exercise both runtime modes without OpenGL/GLFW headers. Aggregate regressions cover lifecycle, dispatch, cache publication, materials, preparation, and input routing. |

## Application APIs

Use `engine.display()`, `engine.window()`, and `engine.surface()` for the selected
platform and presentation contracts.
Use `engine.resources()` for backend asset creation after renderer initialization.
Construct an owned `Loader(root)` for each root. `manifests()` returns a retained
immutable snapshot; singleton compatibility rejects a different root after its
first initialization.

The [loading guide](../assets/asset-loading.md) owns preparation/publication and
application bootstrap; the [render guide](../rendering/pipelines-and-materials.md)
owns program/material construction and packet resolution. Optional
[audio](audio.md) is application-owned outside the runtime adapter graph.

## Execution and shutdown

Games request shutdown from `update()` with `tick.request_stop()`. The tick carries
the runtime's shared stop state, so games need no runtime singleton or callback
wiring. Applications holding the runtime can still call `GameRuntime::stop()`;
both requests use the same normal cleanup sequence. The stop request and tick
lifetime contracts are in [runtime-frame-boundary.md](runtime-frame-boundary.md).

Platform and simulation requests use separate dispatchers with safe saved submission
endpoints. EventBus owns persistent registrations; delivery adapters select platform,
simulation, or serial worker-stream execution. EngineContext creates tracked worker
groups on a lazy owned pool or an injected shared pool. The dedicated simulation
thread remains separate from that general CPU capacity. See
[thread-dispatch.md](thread-dispatch.md), [event-delivery.md](event-delivery.md), and
[worker-execution.md](worker-execution.md).

The [frame boundary](runtime-frame-boundary.md#shutdown) owns shutdown ordering,
including producer quiescence and accepted-work completion. Native callback and
emergency fallback boundaries are in [failure-reporting.md](failure-reporting.md).

## Current limits

The [status inventory](../STATUS.md) records source-backed capabilities and known
gaps at its review baseline. The [planning catalogue](../planning/README.md) links
their owning plans; each subject guide defines its current capability limits.
