# Runtime and backend boundaries

The selected backend is composed through `EngineContext`; `GameRuntime` uses its
display, window, input, presentation, renderer, and resource contracts. The current
GLFW/OpenGL implementation and the in-memory integration probe follow those same
contracts. Source implementation is prepared. Compilation, automated execution,
and real-platform acceptance remain open; see
[RUNTIME-IMPLEMENTATION-STATUS.md](RUNTIME-IMPLEMENTATION-STATUS.md).

| Boundary | Prepared implementation |
| --- | --- |
| CPU asset data | Vertex layouts, typed manifests, grid generation, and owned RGBA decoding have no graphics-context dependency. `Loader::prepare()` runs independently of a provider. |
| Display and presentation | `iDisplaySystem` owns a selected window implementation. `iWindow` and monitor snapshots carry neutral data; `iPresentationSurface` presents. GLFW handles remain inside the platform/backend implementation. The renderer does not own the display. |
| Input | `iInputSystem` supplies State plus independently requested ordered Events/Text. GLFW/Gainput translation stays in its adapter; games consume tick values and routed views. The default factory owns input; an explicit-reference overload borrows it. |
| Draw and material | Simulation writes complete ordered `RenderFrame` passes. Sprite, tile, graphic, and text commands retain immutable asset handles and copied transforms. `ShaderPass` and `ShaderDraw` express standard parameters; `GLSLMaterialBindings` maps them to OpenGL names. |
| Resource creation and caches | `ResourceProvider` uploads decoded pixels and transient vertex spans, creates font atlases, and links programs. Cache readers retain handles under shared locks; construction and retired-handle destruction stay outside locks. One provider/loading thread binds the singleton caches until teardown. |
| Composition and scheduling | One runtime session owns platform polling and presentation. Concurrent mode adds one simulation worker and three reusable frame slots. `platform_tasks().submit()` transfers owned resource requests with future results; shutdown cancels pending requests before game cleanup. |
| OpenGL lifetime | Active resource use requires the owner thread and its actual current context. Handles retire without OpenGL calls from their destructors. Renderer shutdown restores its context, deletes tracked handles, closes their lifetime, and releases the context. Failed destructor cleanup invalidates retained handles. |
| Adapter proof source | The memory display/window, input, presentation, renderer, and resource provider exercise both runtime modes without OpenGL/GLFW headers. Regression source covers lifecycle, dispatch, cache publication, materials, preparation, and input routing. These cases have not been executed in this pass. |

## API migration

Use `engine.display()`, `engine.window()`, and `engine.surface()` for the selected
platform and presentation contracts; the old renderer-owned display API is gone.
Use `engine.resources()` for backend asset creation after renderer initialization.
Construct an owned `Loader(root)` for each root. Its retained `manifests()` snapshot
replaces the old borrowed vector reference; singleton compatibility rejects a
different root instead of silently reusing the first one.

Shader-cache loading only links and publishes. Bind cameras through frame-pass
matrices or explicit `DrawInfo::camera`; shader-manager camera broadcasts and
setters are removed. Standard draw parameters go through semantic bindings.
Application-specific raw uniform methods remain available. `reload_program()`
publishes a successfully linked replacement while existing frames retain their
old handle; failed linking preserves the previous cache entry.

Backend providers implement `create_image(DecodedImage)` and
`upload_geometry(span<const Vertex2D>, topology)`. The shared-pointer/count
geometry overload remains a synchronous compatibility wrapper. An OpenGL context
adapter also implements `is_current()`; thread identity alone cannot establish
that its context is selected.

Generic asset loading does not choose a host font or infer shader recipes. Those
are application bootstrap choices. Sprite/tileset definitions and image upload
remain generic. CPU preparation and upload are described in
[ASSET-LOADING.md](ASSET-LOADING.md); runtime ownership and frame handoff are in
[RUNTIME-FRAME-BOUNDARY.md](RUNTIME-FRAME-BOUNDARY.md).
