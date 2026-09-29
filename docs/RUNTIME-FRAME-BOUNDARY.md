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

Each platform poll produces a timestamped `ActionSnapshot`. Unchanged polls need
no handoff. The runtime divides elapsed time at observed input changes; each
`update()` sees one input state and the seconds spent in that state. Several
updates can precede one frame preparation when polls accumulated. Simulation
produces complete render state, which remains stable while rendering consumes it. The
renderer must never access mutable simulation objects across that boundary.
This handoff implements the `State` capture channel only. Planned ordered
physical `Events` and OS `Text` channels, including textbox focus routing, are
recorded in [input-state-model.md](input-state-model.md#capture-channels-and-textbox-focus-planned);
they are not yet exposed by the runtime.
These rules also apply when simulation and rendering run sequentially. The
sequential runtime owns one reusable frame. Concurrent mode owns three slots:
one may be rendered, one may be the latest completed frame, and one may be in
preparation. Platform event polling and graphics-context use respect their
respective thread affinity.

The caller of `GameRuntime::run()` owns platform polling and the graphics context
in both modes. Sequential mode polls, updates, prepares, renders, and presents on
that thread. Concurrent mode runs only `update()` and `prepare_render_frame()` on
one simulation worker; it passes changed input polls and framebuffer dimensions
as values instead of giving that worker access to a live window. A resize
callback runs on the platform thread and must not mutate simulation state.
`init()` runs after input and graphics initialization but before starting the
worker, so it can register bindings and load GPU assets. After the worker joins
and frames are recycled, `deinit()` runs while the graphics context is current.
Future resource uploads requested during simulation need a graphics-thread queue.
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
polls once per frame, advances input intervals from a monotonic clock after
initialization, prepares and recycles one frame, and tears down game, input, and graphics in that order even
after a loop failure. Concurrent runtime polls on the platform thread at about
60 Hz, including while simulation is busy, and swaps the ordered changed polls
into the worker at each tick boundary. The worker ticks at the same nominal
rate even if no new input was polled; held state persists and edges are not
repeated. The worker publishes a prepared slot without copying
it. If it supersedes a waiting frame, the platform thread recycles the older
slot before lending it out again. At each render handoff the platform thread
claims the newest completed slot. Shutdown joins the worker and recycles all
frames before releasing the graphics context, including after either thread
throws.

The demo uses the same `EngineContext` and frame commands. It starts in
sequential mode; `--concurrent` selects the worker and latest-frame handoff.
`--full-assets` loads the asset tree instead of only the font and 2D shader.
