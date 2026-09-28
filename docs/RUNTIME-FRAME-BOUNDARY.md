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

`AbstractGame::prepare_render_frame()` runs on the simulation thread after an
update. It returns an owned `RenderFrame` value: ordered passes with copied
projection/view matrices and ordered sprite, tile, graphic, and text commands.
Grid commands contain a **resolved atlas cell**, not an animation cursor;
text commands own their strings. The runtime must move the complete frame across
the handoff and prevent further mutation while the renderer consumes it.

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
then choose a frame handoff and a graphics-context-safe destruction policy for
assets retained by frames. The thread and service requirements of game
`init()`/`deinit()` also need definition before a concurrent runtime calls them.
