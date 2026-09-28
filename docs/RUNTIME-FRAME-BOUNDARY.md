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

The render-state representation is still open. A generic top-level render
operation needs an agreed description of draw submissions, camera/pass state,
and resource lifetimes. Today, `iDraw::draw()` and font printing perform graphics
work immediately; they cannot simply be called from simulation and treated as
published render state. Until a frame format and submission contract are chosen,
the runtime and renderer remain skeletons. `AbstractGame` no longer exposes
`draw()`, but it does not yet expose a render-state publication method. The
thread and service requirements of its `init()` and `deinit()` hooks also need
definition before a concurrent runtime can call them.
