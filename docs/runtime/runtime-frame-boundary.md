# Runtime frame boundary

`GameRuntime` coordinates input polling, simulation, rendering, presentation, and
shutdown. Both execution modes use the [same configurable timing policy](simulation-timing.md), with variable/fixed steps and bounded direct/hybrid recovery. The application chooses a compatible platform, graphics context,
renderer, resources, and input adapter. The renderer uses the selected context
and render target; it does not own the display system or read live game state.
The presentation surface presents a completed frame.
The GLFW/OpenGL factory accepts the game's window configuration, constructs the
display and window, and lends the window to its presentation context. The display
owns the GLFW library lifetime; renderer initialization makes the context current
and loads OpenGL entry points before game initialization uploads assets.

Each platform poll produces a timestamped `ActionSnapshot`. Each independently
scheduled `update()` consumes the complete pending batch in one `TickInput`.
Input supplies edges, counts, and observed hold durations; it never subdivides
simulation time or schedules extra updates. Simulation
produces complete render state, which remains stable while rendering consumes it. The
renderer must never access mutable simulation objects across that boundary.
Each complete `PollSnapshot` carries State and independently captured ordered
Events/Text records. The runtime transfers both at the same consumption boundary;
`TickInput::records()` retains their shared observation order. Scoped capture
requests and backend fidelity are documented in
[input-state-model.md](input-state-model.md#ordered-events-and-os-text).
Each tick also carries logical window and framebuffer dimensions sampled together
on the platform owner. Simulation uses these copied values for UI layout and pixel
scaling without reading the live window. Manually constructed ticks supply their
logical dimensions explicitly; the default is zero.
These rules also apply when simulation and rendering run sequentially. The
sequential runtime owns one reusable frame. Concurrent mode owns three slots:
one may be rendered, one may be the latest completed frame, and one may be in
preparation. Platform event polling and graphics-context use respect their
respective thread affinity.

The caller of `GameRuntime::run()` owns platform polling and the graphics context
in both modes. Sequential mode polls, updates, prepares, renders, and presents on
that thread. Concurrent mode runs only `update()` and `prepare_render_frame()` on
one simulation worker; it passes complete polling batches and framebuffer dimensions
as values instead of giving that worker access to a live window. A resize
callback runs on the platform thread and must not mutate simulation state.
`init()` runs after input and graphics initialization but before starting the
worker, so it can register bindings and load GPU assets. After the worker joins
and frames are recycled, `deinit()` runs while the graphics context is current.
The default GLFW/OpenGL factory owns its input adapter; the overload taking an
input reference borrows it through context destruction. Owned input detaches
before the window is destroyed. A runtime and its context each permit one session.
Game cleanup is paired with attempted initialization, including partial failure;
cleanup preserves the first failure while still shutting down both adapters and
reports subsequent failures with phase context. See [failure-reporting.md](failure-reporting.md)
for deferred native callback checks and the bounded emergency reporting contract.
`engine.platform_dispatcher().submit(work)` transfers owned request data to the platform
thread, where graphics is current. The result is a future: inspect readiness from
`update()`, then publish the resulting handle through the next render frame. Do not
block simulation on it or capture live simulation objects in platform callbacks.
The queue drains FIFO batches before polling, including while the input backlog
is full; nested requests wait for the next batch. Each failure reaches its own
future. Shutdown closes context worker acceptance and stops simulation while pumping
platform requests needed by accepted work. Once simulation and CPU work settle,
remaining platform requests cancel before frame/game cleanup; their futures report
`broken_promise`. A shared scheduler wake remains valid through concurrent submit
and close without retaining the runtime itself. F5 in the demo queues a material recipe
reload and adopts its immutable generation during a later update. Failure retains
the old material and reports its error in the overlay.
Callbacks must preserve the session's current graphics context. Backend resource
guards reject another context even when it is selected on the correct thread.
`stop()` sets an atomic request and wakes the concurrent scheduler's waits.

`AbstractGame::prepare_render_frame(writer)` runs on the simulation thread after
the final useful update of a bounded batch when a slot is free. If all slots are occupied, the update still
advances the simulation and only that tick's visual snapshot is skipped. The
runtime lends the game a reusable `RenderFrame` slot through the writer. The game
fills ordered passes with copied camera values, custom pass parameters, constraints,
and resolved DrawPacket2D values. Asset helpers select geometry ranges and lay out
text on simulation, resolving transforms, engine semantics, custom parameters, and
image requests into owned values. Packets retain geometry and material generations;
no asset, Font, string, animation cursor, or live entity is needed by playback.
The caller model carries placement/rotation; DrawStyle2D.scale also scales local
geometry and glyph offsets explicitly. The runtime publishes the complete slot without
copying it and prevents further mutation while the renderer consumes it.

Recycling a superseded or shutdown slot clears only the active passes' draw
packets and pass parameters, releasing their resource handles on the graphics thread while its context
is current. The platform retains its latest complete slot between publications. Recycling keeps pass objects and draw-vector capacity for later ticks. One
slot suffices when update and render are sequential; concurrent update/render
need at least two. A third permits a completed frame to wait while one is being
written and one is being rendered.
Text strings may still allocate, and the final release of a GPU-backed handle
must occur on the graphics thread or through backend-managed deferred destruction.

The cached `Sprite` now owns immutable clip definitions and grid resources.
An entity keeps its sprite handle and its own SpriteAnimation playback value,
advances it using simulation time, and resolves animation.cell() before publishing.
The playback value aliases the shared definition without retaining GPU resources.
Cached sprite/tileset assets have no mutable selected cell. Graphic/Tile/Font
immediate drawing and DrawInfo/iDraw/Draw2D are retired; submission helpers select
ranges and const Font layout supplies glyph placements. Deprecated FFont retains
its typed alternate-bank option without stored print state or unchecked formatting
pointers. New fonts use STBFont with a supplied font file; see
[FFont deprecation](../resources/legacy-ffont.md).

OpenGLRenderer consumes authored packet order, checks its native pipeline domain,
and applies complete validated pipeline state/parameters/geometry for every draw.
GPU handles are
retired through the renderer's context-owned release queue. Sequential runtime
polls when eligible and starts the independent observation/simulation clocks after
initialization. It prepares one frame after a bounded update batch and retains it
until replacement or shutdown. It tears down game, input, and graphics even
after a loop failure. Concurrent runtime uses configurable lockstep (default), finite-capacity, or
unlimited polling, with a minimum completion-to-next-poll spacing. Full batches
pause polling while rendering continues. Every completed poll counts, including
unchanged samples; the worker takes the entire batch at a cycle boundary and
wakes polling into an empty backlog. Renderer retirement maintenance runs after platform work
and before waiting, even without a new frame; idle waits are bounded to 10 ms.
[Resource residency](../resources/resource-residency.md) describes the bound and ownership trace. Each selected update consumes fresh input;
held State persists and transient input is not repeated. Both runtime modes use
the same timing configuration, including fixed steps and capped VariableCatchUp. The worker publishes a prepared slot without copying
it. If it supersedes a waiting frame, the platform thread recycles the older
slot before lending it out again. At each render handoff the platform thread
claims the newest completed slot. Shutdown joins the worker and recycles all
frames before releasing the graphics context, including after either thread
throws.

The demo uses the same `EngineContext` and frame commands. It starts in
sequential mode; `--concurrent` selects the worker and latest-frame handoff.
`--full-assets` additionally loads the asset tree. Font selection and the 2D shader
recipe remain explicit application bootstrap in either path.

The demo accepts `--input-capacity=N` (finite polling), `--input-unlimited`, and
`--input-spacing-ms=N` in either runtime mode. These alter polling eligibility,
not the simulation schedule. The platform thread still shares polling with
presentation, so a blocking present can delay an eligible poll.

The demo's F2 textbox exercises independently requested Events/Text capture and
keyboard focus in either runtime mode. Focus is latched on the platform at poll
start; pending records retain their original target through the worker handoff.
Exclusive focus gates gameplay keyboard State while controller/mouse input
continues. UI code reads ordered text/editing records during simulation; it is
never invoked by a platform callback. Enter/Escape releases focus.

Input capture/routing contracts are described in
[input-state-model.md](input-state-model.md); recorded execution scopes are in
[architecture-validation.md](../development/architecture-validation.md).

Material contracts resolve ShaderPass/ShaderDraw engine semantics and copied custom
pass/material/draw values without common code selecting native uniform names.
GLSLPipelineBindings owns explicit backend mappings. MaterialMgr builds complete
recipe replacements before publishing; retained frame owners keep old generations
and failed builders leave the prior entry intact. See [pipelines-and-materials.md](../rendering/pipelines-and-materials.md).

Asset-manager lookups copy published handles under shared locks; publication and
clearing use unique locks. Asset construction and removed-handle destruction run
outside those locks. Provider binding serializes loads to the first loading
thread and rejects another provider until teardown finishes. Teardown marks the
binding as releasing before clearing caches, so reentrant deleters cannot refill
them. Retained external handles survive cache clearing; their backend lifetime
still governs use and final release. CPU preparation does not bind these caches.

Owned `Loader` instances prepare manifests and decoded RGBA pixels without the
provider, rescanning their immutable roots each time. Upload consumes those owned
pixels on the loading/platform thread and publishes a retained metadata snapshot
after success. Geometry upload accepts a transient vertex span and copies it
before returning. Use context-owned WorkerGroups to prepare owned data, then submit upload through
saved platform endpoints. Accepted group work settles during runtime shutdown;
independently owned workers require application lifetime coordination. See
[asset-loading.md](../assets/asset-loading.md).

OpenGL resource operations require a live owner thread and the selected current
context. Renderer shutdown first restores that context, deletes tracked handles,
and closes their lifetime before releasing it. Retained handles reject use after
closure without querying the borrowed context. If destructor cleanup cannot
recover the context, it invalidates handles without OpenGL calls; platform context
destruction releases remaining native resources. The context outlives its renderer.
Recorded build and native acceptance scopes are in
[architecture-validation.md](../development/architecture-validation.md).

### Saved platform submission endpoints

`engine.platform_dispatcher().submission()` returns a copyable endpoint which
owns submission state, not a borrowed dispatcher or EngineContext pointer.
It can be captured by a delivery adapter. After shutdown or dispatcher destruction
it throws `failed_operation` on submission; it cannot access destroyed resources.
Accepted requests retain FIFO enqueue order and execute only on the owner thread.
Posting on that thread still defers work. Requests posted during a detached drain
belong to a later drain, and nested drains cannot bypass that boundary.

A future reports callback failures. Unexecuted requests are destroyed outside
queue locks on the platform before game/resource cleanup, making their futures
report `broken_promise`. Do not wait on a platform future from the platform thread,
or block a simulation update on platform work. Wake callbacks retain scheduler
state because a producer can notify immediately after the queue closes.
