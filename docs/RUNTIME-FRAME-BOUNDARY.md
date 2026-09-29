# Runtime frame boundary

`GameRuntime` coordinates input polling, simulation, rendering, presentation, and
shutdown. The application chooses a compatible platform, graphics context,
renderer, resources, and input adapter. The renderer uses the selected context
and render target; it does not own the display system or read live game state.
The presentation surface presents a completed frame.

Each platform poll produces an `ActionSnapshot`. Each simulation tick receives
`TickInput` assembled from zero or more polls since the previous tick. Simulation produces
complete render state, which remains stable while rendering consumes it. The
renderer must never access mutable simulation objects across that boundary.
These rules also apply when simulation and rendering run sequentially; a later
scheduler may put them on separate threads without changing their meaning.
`GameRuntime` will own that scheduling and the handoff, while platform event
polling and graphics-context use must respect their respective thread affinity.

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
and font calls still draw immediately; the renderer must consume frame commands
without calling these methods from the simulation thread. Text rendering also
needs to stop mutating the shared font's message and angle.

`OpenGLRenderer::render()` and `GameRuntime::run()` remain skeletons. Implement
the renderer's ordered passes, material/camera binding, and stateless text path;
then choose a frame-slot handoff and a graphics-context-safe destruction policy
for assets retained by frames. The thread and service requirements of game
`init()`/`deinit()` also need definition before a concurrent runtime calls them.
