# Runtime architecture and backend boundaries

The selected backend is composed through `EngineContext`; `GameRuntime` uses its
display, window, input, presentation, renderer, and resource contracts. The current
GLFW/OpenGL implementation and the in-memory integration probe follow those same
contracts. The architecture is implemented; recorded build, regression, and native
acceptance results are in [architecture-validation.md](../development/architecture-validation.md).

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

Shader-cache loading only links and publishes explicit programs. Frame preparation
resolves pass cameras and draw/material parameters into owned packets. MaterialMgr
publishes complete recipes after successful construction; existing frames retain
old generations and failures preserve the previous entry. Immediate draw APIs are
retired. OpenGLResourceProvider builds typed pipelines/materials through explicit
backend mappings; common submission never binds native resources.

Backend providers implement `create_image(DecodedImage)` and
`upload_geometry(span<const Vertex2D>, topology)`. The shared-pointer/count
geometry overload remains a synchronous compatibility wrapper. An OpenGL context
adapter also implements `is_current()`; thread identity alone cannot establish
that its context is selected.

Generic asset loading does not choose a host font or infer shader recipes. Those
are application bootstrap choices. Sprite/tileset definitions and image upload
remain generic. CPU preparation and upload are described in
[asset-loading.md](../assets/asset-loading.md); runtime ownership and frame handoff are in
[runtime-frame-boundary.md](runtime-frame-boundary.md).

## Execution and shutdown

Platform and simulation requests use separate dispatchers with safe saved submission
endpoints. EventBus owns persistent registrations; delivery adapters select platform,
simulation, or serial worker-stream execution. EngineContext creates tracked worker
groups on a lazy owned pool or an injected shared pool. The dedicated simulation
thread remains separate from that general CPU capacity. See
[thread-dispatch.md](thread-dispatch.md), [event-delivery.md](event-delivery.md), and
[worker-execution.md](worker-execution.md).

Runtime shutdown closes context worker submissions, stops simulation, and pumps
accepted platform dependencies while simulation joins. The game then quiesces its
external producers while targets remain alive. Accepted CPU work settles before
remaining platform requests cancel, frames recycle, and game/input/graphics cleanup
runs. An injected pool's unrelated application groups remain open. Cleanup preserves
the first failure, including after partial initialization, and reports later cleanup
failures with phase context. Native callback and emergency fallback boundaries are
documented in [failure-reporting.md](failure-reporting.md).

## Current limits

Singleton asset caches support one active provider/loading-owner domain. Repeated
generic loads preserve existing keys; upload is not an atomic hot-reload transaction.
Failed upload may leave completed cache entries while published metadata stays at
its previous successful snapshot. Material recipe reload has a separate successful-
replacement contract that preserves retained generations.

Tile-map neighbor selection and application meanings for views/orientations remain
gameplay work. Input and the demo editor retain committed Unicode scalars; the fonts
render printable ASCII, and the editor supplies no grapheme/IME contract. Audio,
networking, world/physics systems, text shaping, automatic cache eviction, and advanced worker topology
are separate extensions. Concrete unfinished work is in [todo.md](../planning/todo.md) and
[asset-manifest-todo.md](../planning/asset-manifest-todo.md).
