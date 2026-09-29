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

Each platform poll produces an `ActionSnapshot`. Each simulation tick receives
`TickInput` assembled from zero or more polls since the previous tick. Simulation produces
complete render state, which remains stable while rendering consumes it. The
renderer must never access mutable simulation objects across that boundary.
These rules also apply when simulation and rendering run sequentially. The
sequential runtime owns one reusable frame; its concurrent scheduler still needs
an ownership handoff. Platform event polling and graphics-context use must
respect their respective thread affinity.

The caller of `GameRuntime::run()` owns platform polling and the graphics context
in both modes. Sequential mode polls, updates, prepares, renders, and presents on
that thread. Concurrent mode runs only `update()` and `prepare_render_frame()` on
one simulation worker; it passes completed input polls and framebuffer dimensions
as tick values instead of giving that worker access to a live window. A resize
callback runs on the platform thread and must not mutate simulation state.
`init()` runs after input and graphics initialization but before starting the
worker, so it can register bindings and load GPU assets. After the worker joins
and frames are recycled, `deinit()` runs while the graphics context is current.
Future resource uploads requested during simulation need a graphics-thread queue.
`stop()` uses an atomic request; the concurrent scheduler must wake any waits
when that request is made.

`AbstractGame::prepare_render_frame(writer)` runs on the simulation thread after
an update. The runtime lends it a free, reusable `RenderFrame` slot through the
writer. The game fills ordered passes with copied projection/view matrices and
ordered sprite, tile, graphic, and text commands. Each command's model matrix
already contains its position, rotation, and scale; the simulation retains the
authoritative entity transform and does not read an old frame to advance it.
Grid commands contain a **resolved atlas cell**, not an animation cursor; text
commands own their strings. The runtime publishes the complete slot without
copying it and prevents further mutation while the renderer consumes it.

Recycling a consumed slot clears only the active passes' draw commands, releasing
their asset handles while the graphics context is current. It keeps pass objects
and draw-vector capacity for later ticks. One slot suffices when update and render
are sequential; concurrent update/render need at least two. A third permits a
completed frame to wait while one is being written and one is being rendered.
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
polls once per tick, samples delta time after initialization, prepares and
recycles one frame, and tears down game, input, and graphics in that order even
after a loop failure. Concurrent runtime still needs frame-slot handoff, a
clock cadence, and a policy for input polls accumulating while simulation is busy.
