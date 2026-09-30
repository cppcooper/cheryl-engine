# Runtime frame boundary

`GameRuntime` coordinates input polling, simulation, rendering, presentation, and
shutdown. The application chooses a compatible platform, graphics context,
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
cleanup preserves the first failure while still shutting down both adapters.
`engine.platform_dispatcher().submit(work)` transfers owned request data to the platform
thread, where graphics is current. The result is a future: inspect readiness from
`update()`, then publish the resulting handle through the next render frame. Do not
block simulation on it or capture live simulation objects in platform callbacks.
The queue drains FIFO batches before polling, including while the input backlog
is full; nested requests wait for the next batch. Each failure reaches its own
future. Shutdown rejects new requests and destroys unexecuted captures on the
platform before joining the worker and cleaning up the game; their futures report
`broken_promise`. A shared scheduler wake remains valid through concurrent submit
and close without retaining the runtime itself. F5 in the demo queues a shader
reload and adopts the resulting handle during a later update.
Callbacks must preserve the session's current graphics context. Backend resource
guards reject another context even when it is selected on the correct thread.
`stop()` sets an atomic request and wakes the concurrent scheduler's waits.

`AbstractGame::prepare_render_frame(writer)` runs on the simulation thread after
an update when a slot is free. If all slots are occupied, the update still
advances the simulation and only that tick's visual snapshot is skipped. The
runtime lends the game a reusable `RenderFrame` slot through the writer. The game
fills ordered passes with copied projection/view matrices and ordered sprite,
tile, graphic, and text commands. Each command's model matrix
already contains its position, rotation, and scale; the simulation retains the
authoritative entity transform and does not read an old frame to advance it.
Grid commands contain a **resolved atlas cell**, not an animation cursor; text
commands own their strings. The runtime publishes the complete slot without
copying it and prevents further mutation while the renderer consumes it.

Recycling a rendered or superseded slot clears only the active passes' draw
commands, releasing their asset handles on the graphics thread while its context
is current. It keeps pass objects and draw-vector capacity for later ticks. One
slot suffices when update and render are sequential; concurrent update/render
need at least two. A third permits a completed frame to wait while one is being
written and one is being rendered.
Text strings may still allocate, and the final release of a GPU-backed handle
must occur on the graphics thread or through backend-managed deferred destruction.

The cached `Sprite` now owns immutable clip definitions and grid resources.
An entity keeps its sprite handle and its own `SpriteAnimation` playback value,
advances the latter using simulation time, and publishes `animation.cell()` with
the sprite handle. The playback value aliases the shared definition; it neither
copies the frame list nor retains the GPU resources. Cached sprite and tileset
assets have no mutable selected cell. Legacy `Tile`, `TileAnimation`, `Graphic`,
and font calls still draw immediately; the renderer consumes frame commands on
the graphics thread instead. Text commands and the legacy font path now read
shared glyph data without storing the message or angle on the font.

`OpenGLRenderer::render()` consumes ordered passes, binds each material and pass
camera, and reads font glyphs without mutating shared assets. GPU handles are
retired through the renderer's context-owned release queue. Sequential runtime
polls when eligible, summarizes State activity and elapsed time from a monotonic
clock after initialization, prepares and recycles one frame, and tears down game, input, and graphics in that order even
after a loop failure. Concurrent runtime uses configurable lockstep (default), finite-capacity, or
unlimited polling, with a minimum completion-to-next-poll spacing. Full batches
pause polling while rendering continues. Every completed poll counts, including
unchanged samples; the worker takes the entire batch at a cycle boundary and
wakes polling into an empty backlog. The worker ticks at a nominal 16.667 ms cadence even if no new input was polled; held state persists and edges are not
repeated. The worker publishes a prepared slot without copying
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

The input implementation's remaining validation gate is recorded in
[INPUT-IMPLEMENTATION-STATUS.md](INPUT-IMPLEMENTATION-STATUS.md).

Common draw calls bind `ShaderPass` (projection/view) and `ShaderDraw`
(model/alpha/scale/texture unit). `GLSLMaterialBindings` maps those roles to the
OpenGL shader's names; empty names omit unused roles. Custom uniform methods
remain available for application-specific parameters. The shader cache links and
publishes programs without binding or broadcasting camera state. Use pass matrices
or `DrawInfo::camera` instead of the removed shader-cache camera setters.
`reload_program` publishes a successfully linked replacement while older frames
retain the old handle; a failed reload leaves the previous entry intact.

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
before returning. Preparation workers are application-owned; coordinate their
lifetime before submitting completed data through the platform queue. See
[ASSET-LOADING.md](ASSET-LOADING.md).

OpenGL resource operations require a live owner thread and the selected current
context. Renderer shutdown first restores that context, deletes tracked handles,
and closes their lifetime before releasing it. Retained handles reject use after
closure without querying the borrowed context. If destructor cleanup cannot
recover the context, it invalidates handles without OpenGL calls; platform context
destruction releases remaining native resources. The context outlives its renderer.
Full preparation status and the outstanding execution gate are recorded in
[RUNTIME-IMPLEMENTATION-STATUS.md](RUNTIME-IMPLEMENTATION-STATUS.md).
