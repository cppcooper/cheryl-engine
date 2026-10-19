# Runtime frame boundary

Use this guide to place application work in the correct hook and to preserve
its dependencies through shutdown. Start from the runnable
[First Game](../getting-started/first-game.md) or
[assemble a context](../development/consuming-engine.md#context-assembly) first.
The sequence below applies to both runtime modes. Polling, simulation and
presentation have independent eligibility; it does not require one poll per update
or one update per rendered frame.

| Boundary | Execution owner | Value or responsibility transferred |
| --- | --- | --- |
| Initialize adapters, then `game.init()` | Platform/graphics caller of `run()` | Live input/window/graphics become available; the game registers bindings and uploads initial resources. |
| Service platform requests and eligible input polls | Platform/graphics | Owned request results, complete input polls and copied dimensions become available. |
| Drain simulation requests, transfer input, call `update()` | Simulation owner | One detached mailbox batch precedes each actual update; the update consumes the available polls and changes game state. |
| `prepare_render_frame()` after a useful update batch | Simulation owner | Copy camera/draw values and retain resources in an available frame slot; skip visual publication when no slot is free. |
| Render, present and maintain resources | Platform/graphics | Consume the newest completed frame without reading mutable game state; recycle superseded frames and deferred native resources. |
| Stop, join, quiesce, settle accepted work, recycle, `deinit()` | Coordinated shutdown, with cleanup on platform | Keep dependencies alive until accepted users finish; follow [the full shutdown order](#shutdown). |

`CE::Runtime::GameRuntime` coordinates input polling, simulation, rendering, presentation, and
shutdown. Both execution modes use the [same configurable timing policy](simulation-timing.md), with variable/fixed steps and bounded direct/hybrid recovery. The application chooses a compatible platform, graphics context,
renderer, resources, and input adapter. The renderer uses the selected context
and render target; it does not own the display system or read live game state.
The presentation surface presents a completed frame.
The GLFW/OpenGL factory accepts the game's window configuration, constructs the
display and window, and lends the window to its presentation context. The display
owns the GLFW library lifetime; renderer initialization makes the context current
and loads OpenGL entry points before game initialization uploads assets.

## Input and owner threads

Each platform poll produces a timestamped `CE::Input::Actions::Snapshot`. Each independently
scheduled `update()` consumes the complete pending batch in one `CE::Input::Tick`.
Input supplies edges, counts, and observed hold durations; it never subdivides
simulation time or schedules extra updates. Simulation
produces complete render state, which remains stable while rendering consumes it. The
renderer must never access mutable simulation objects across that boundary.
Each complete `CE::Input::PollSnapshot` carries State and independently captured ordered
Events/Text records. The runtime transfers both at the same consumption boundary;
`CE::Input::Tick::records()` retains their shared observation order. Scoped capture
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

The caller of `CE::Runtime::GameRuntime::run()` owns platform polling and the graphics context
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

## Queued preparation and stop requests

`engine.platform_dispatcher().submit(work)` transfers owned request data to the platform
thread, where graphics is current. The result is a future: inspect readiness from
`update()`, then publish the resulting handle through the next render frame. Do not
block simulation on it or capture live simulation objects in platform callbacks.
The queue drains FIFO batches before polling, including while the input backlog
is full; nested requests wait for the next batch. Futures, cancellation and saved
submission endpoints follow the [dispatcher contract](thread-dispatch.md).
Accepted dependencies settle under the [shutdown sequence](#shutdown) below.
Callbacks must preserve the session's current graphics context. Backend resource
guards reject another context even when it is selected on the correct thread.
Game code calls `tick.request_stop()` to request graceful shutdown from `update()`.
Every `CE::Game::TickContext` shares the session's `std::stop_source` with `CE::Runtime::GameRuntime::stop()`;
both entry points wake scheduler waits and are safe to repeat. The request ends
further simulation updates and uses the normal join, quiesce and cleanup sequence.
It does not interrupt the current `update()`; return early when further game work
is unnecessary. Sequential execution can still prepare and present its final
updated frame before teardown.
A copied `tick.runtime_stop` retains only stop state and remains safe after runtime
destruction; do not retain the tick itself or its borrowed input view. Manually
constructed ticks default to no runtime stop source; `request_stop()` then throws
`failed_operation`. Tests or another scheduler can supply a source explicitly.

## Frame publication and recycling

During ordinary publication, each slot follows this lifecycle. Rendering may keep
using the current complete slot while simulation prepares another; platform owns
recycling.

```mermaid
stateDiagram-v2
    accTitle: Render-frame slot lifecycle
    accDescr: Simulation prepares a free slot and publishes it ready. Platform renders the newest ready slot and recycles superseded or replaced slots, releasing their retained resources.
    Free --> Preparing: simulation obtains a slot
    Preparing --> Ready: publish complete frame
    Ready --> Rendering: platform claims newest frame
    Ready --> Free: superseded and recycled on platform
    Rendering --> Rendering: reuse current frame
    Rendering --> Free: replaced or stopped, then recycled
```

`CE::Game::AbstractGame::prepare_render_frame(writer)` runs on the simulation thread after
the final useful update of a bounded batch when a slot is free. If all slots are occupied, the update still
advances the simulation and only that tick's visual snapshot is skipped. The
runtime lends the game a reusable `CE::Rendering::Frame` slot through the writer. The game
fills ordered passes with copied camera values, custom pass parameters, constraints,
and resolved `CE::Rendering::DrawPacket2D` values. Asset helpers select geometry ranges and lay out
text on simulation, resolving transforms, engine semantics, custom parameters, and
image requests into owned values. Packets retain geometry and material generations;
no asset, font interface, string, animation cursor, or live entity is needed by playback.
The caller model carries placement/rotation; `CE::Rendering::DrawStyle2D::scale` also scales local
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

At each concurrent render handoff, platform claims the newest completed slot.
If simulation supersedes a waiting frame, platform recycles that older slot before
lending it out again. Rendering consumes authored packet order and complete
validated pipeline state. The [render contract](../rendering/pipelines-and-materials.md)
owns packet resolution and writer rules; [asset playback](../assets/asset-values-and-playback.md)
and [Unicode text](../assets/text-layout.md) resolve their values before publication.

Polling admission and fresh-batch consumption follow the
[input contract](input-state-model.md#polling-backlog-and-scheduling). Frame
preparation occurs once after a bounded update batch under the
[timing policy](simulation-timing.md). Retirement maintenance runs independently
of new frames under the [residency contract](../resources/resource-residency.md#native-retirement-and-maintenance).
The [demo guide](../../../projects/apps/demo/README.md) owns its options, controls and
application bootstrap.

## Shutdown

While joining simulation and settling accepted CPU work, platform keeps servicing
dependencies. The remaining phases preserve their owners through cleanup:

```mermaid
flowchart TB
    accTitle: Runtime shutdown phases
    accDescr: Stop closes CPU-group acceptance, simulation finishes while platform dependencies are serviced, producers quiesce, accepted CPU work settles, frames recycle and game cleanup precedes adapter shutdown.
    stop["Stop CPU acceptance"] --> join["Join simulation"]
    join --> quiesce["Quiesce producers"]
    quiesce --> settle["Settle accepted work"]
    settle --> recycle["Cancel queue; recycle frames"]
    recycle --> cleanup["Game and adapter cleanup"]
```

Stopping closes context CPU-group acceptance and stops simulation. Simulation
cancels its pending dispatcher work on its owner before publishing completion;
initialization failure before owner binding cancels on the initializing thread.
Platform keeps pumping accepted dependencies while joining simulation, so an
accepted callback awaiting platform work cannot strand it in a blocking join.

After simulation joins, `game.quiesce()` stops external producers and invalidates
borrowed event registrations while their targets/resources remain alive. Do not
block this hook on work that still needs platform dispatch. It pairs with attempted
game initialization, including partial startup. Accepted CPU work then settles
while platform continues servicing dependencies; bounded 1 ms shutdown waits
observe completion without borrowed runtime wake callbacks in workers. Only
context-created groups close; an injected pool's unrelated groups remain open.
An owned root joins before game/resource cleanup.

Remaining platform requests cancel, all retained frames recycle on graphics,
and `game.deinit()` runs before input and graphics shutdown. Preserve the first
failure while completing cleanup; report later failures with phase context through
[failure reporting](failure-reporting.md). Owned input detaches before window
destruction, and the context must outlive its renderer. External logical handles
follow the [native lifetime contract](../resources/resource-residency.md): they can
survive for destruction/CPU inspection but cannot draw through a closed domain.
