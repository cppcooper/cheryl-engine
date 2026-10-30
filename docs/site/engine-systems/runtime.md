# Runtime

## Purpose

Runtime coordinates a single game session: adapter initialization, input polling,
simulation scheduling, render-frame handoff, presentation and orderly shutdown.
It runs application-defined game hooks rather than prescribing entities, scenes
or a world model.

## Interface

Types below are in `CE::Runtime` unless another namespace is shown.

### Types and operations

| API and purpose |
| --- |
| {cpp:class}`Context <CE::Runtime::Context>`<br>Own compatible adapters and obtain borrowed accessors and tracked worker groups. [Adapter catalogue](../architecture/interfaces-adapters.md); [context ownership](../runtime/worker-execution.md#context-ownership). |
| {cpp:class}`GameRuntime <CE::Runtime::GameRuntime>`<br>Borrow a game/context for one session; block in `run()` and request shutdown with `stop()`. [Runtime frame boundary](../runtime/runtime-frame-boundary.md). |
| {cpp:class}`RuntimeLifecycle <CE::Runtime::RuntimeLifecycle>`, {cpp:struct}`RuntimeLifecycleSnapshot <CE::Runtime::RuntimeLifecycleSnapshot>`<br>Read milestones and a retained terminal outcome without borrowing Runtime. [Lifecycle observation](../runtime/runtime-frame-boundary.md#lifecycle-observation). |
| {cpp:class}`ShutdownEndpoint <CE::Runtime::ShutdownEndpoint>`, {cpp:class}`ShutdownPolicy <CE::Runtime::ShutdownPolicy>`<br>Stage ordinary requests and own the single host decision policy. Stop/failure bypass negotiation. [Shutdown protocol](../runtime/runtime-frame-boundary.md#negotiable-shutdown). |
| {cpp:class}`RuntimeServices <CE::Runtime::RuntimeServices>`, {cpp:class}`ServiceAttachment <CE::Runtime::ServiceAttachment>`<br>Attach required owner work to maintenance or pre-update phases. [Service phases](../runtime/runtime-frame-boundary.md#runtime-service-phases). |
| {cpp:struct}`WindowState <CE::Runtime::WindowState>`, {cpp:struct}`SimulationView <CE::Runtime::SimulationView>`<br>Read copied selections and attach required sizing through Context sources. [Committed views](../runtime/display-and-window-contract.md#committed-window-and-simulation-views). |
| {cpp:struct}`CE::Game::AbstractGame`<br>Supply `init`, `update`, frame preparation, `quiesce` and `deinit` hooks. [Game hooks and ownership](../development/consuming-engine.md#game-hooks-and-ownership); [shutdown](../runtime/runtime-frame-boundary.md#shutdown). |
| {cpp:struct}`CE::Game::TickContext`<br>Read simulation delta, copied dimensions, immutable input and the session stop request. [Input and owner threads](../runtime/runtime-frame-boundary.md#input-and-owner-threads); [tick time](../runtime/simulation-timing.md#input-and-tick-time). |
| {cpp:class}`CE::Application::Startup`<br>Configure the supplied command-line composition path before creating the runtime. [Command-line startup](../development/consuming-engine.md#command-line-startup); [Startup module](../modules/startup.md). |

### Configuration and enums

| Type | Choices and details |
| --- | --- |
| {cpp:enum}`RunMode <CE::Runtime::RunMode>` | `Sequential` updates on platform; `Concurrent` uses a dedicated simulation thread. [Execution and frame handoff](../runtime/runtime-frame-boundary.md). |
| {cpp:struct}`ExecutionOptions <CE::Runtime::ExecutionOptions>` | Lazy owned worker capacity or an application-supplied shared pool. [Context worker ownership](../runtime/worker-execution.md#context-ownership). |
| {cpp:struct}`Simulation::TimingOptions <CE::Runtime::Simulation::TimingOptions>`, {cpp:enum}`Simulation::Mode <CE::Runtime::Simulation::Mode>` | Configure variable/fixed updates, interval and bounded recovery. [Timing defaults](../runtime/simulation-timing.md#defaults-and-pacing); [fixed steps](../runtime/simulation-timing.md#fixed-steps). |
| {cpp:enum}`Simulation::LagRecovery <CE::Runtime::Simulation::LagRecovery>`, {cpp:enum}`Simulation::UpdateKind <CE::Runtime::Simulation::UpdateKind>` | Select dropped excess lag or variable catch-up; identify the kind of the current tick. [Catch-up recovery](../runtime/simulation-timing.md#catch-up-recovery). |
| {cpp:struct}`CE::Input::PollingOptions`, {cpp:enum}`CE::Input::PollingPolicy` | Configure completed-poll backlog and spacing independently of simulation mode. [Polling backlog](../runtime/input-state-model.md#polling-backlog-and-scheduling). |

Start with sequential execution when polling, updates and rendering can share
one owner. Concurrent execution keeps platform polling/rendering separate from
a slow update, but requires copied observations and retained frame publication
instead of live adapter access from another thread. Both modes use the same
[timing choices](../runtime/simulation-timing.md#choose-a-timing-policy).

## Usage

Assemble a compatible adapter set into {cpp:class}`Runtime::Context <CE::Runtime::Context>`,
implement {cpp:struct}`Game::AbstractGame <CE::Game::AbstractGame>`, then construct
{cpp:class}`Runtime::GameRuntime <CE::Runtime::GameRuntime>` and call `run()` on
the platform owner. The supplied GLFW/OpenGL factory handles adapter construction;
[First Game](../getting-started/first-game.md) shows the complete application.

```{engine-diagram} runtime-focus-assembly
```

[Explore runtime dependencies and application composition](diagram-explorer.md#runtime-internal),
including the [tick/frame boundary](diagram-explorer.md#runtime-focus-frame) and
[shutdown capabilities](diagram-explorer.md#runtime-focus-shutdown).

### Choose the integration point

| Application work | Entry point | Boundary to preserve |
| --- | --- | --- |
| Bind devices, upload initial resources, acquire producer endpoints | `AbstractGame::init_platform()` | Platform/graphics owner; retain acquired handles for later work. |
| Create simulation-owned controllers and UI, attach automatic view following | `AbstractGame::init_simulation()` | Simulation owner, with its initial view established before Ready and the first update. |
| Advance gameplay | `AbstractGame::update()` with {cpp:struct}`Game::TickContext <CE::Game::TickContext>` | Use immutable input and copied sizes; advance state with the selected simulation delta. |
| Publish draw commands | Frame preparation with {cpp:class}`Rendering::FrameWriter <CE::Rendering::FrameWriter>` | Publish retained render state. Preparation can be skipped when no frame slot is free. |
| Run required recurring module work | {cpp:class}`Runtime::RuntimeServices <CE::Runtime::RuntimeServices>` | Attach on the initialized owner and retain the returned `ServiceAttachment`; use events for optional observation. |
| Request or negotiate shutdown | `tick.request_stop()`, `runtime.stop()`, or {cpp:class}`Runtime::ShutdownEndpoint <CE::Runtime::ShutdownEndpoint>` | Stop is unconditional. Ordinary endpoint requests may be negotiated by the single installed host policy. |

The [game hook contract](../development/consuming-engine.md#game-hooks-and-ownership)
defines hook ownership and cleanup. The [frame boundary](../runtime/runtime-frame-boundary.md)
explains when each operation runs in sequential and concurrent modes.

### Uses

These dependencies supply the session's adapters, data and execution boundaries.
`Context` owns display, presentation, renderer and resource adapters, and owns or
explicitly borrows input. `GameRuntime` borrows the context and game.

| Dependency | Runtime's responsibility |
| --- | --- |
| {cpp:struct}`Game::AbstractGame <CE::Game::AbstractGame>` | Invoke initialization, update, preparation and cleanup hooks on their owners. |
| {cpp:class}`Platform::DisplaySystem <CE::Platform::DisplaySystem>`, {cpp:class}`Platform::Window <CE::Platform::Window>` | Select copied sizes/focus and sample native close intent on platform. [Display](display.md). |
| {cpp:class}`Input::System <CE::Input::System>` | Initialize and poll the adapter; transfer complete observations into ticks. [Input](input.md). |
| {cpp:class}`Rendering::Renderer <CE::Rendering::Renderer>`, {cpp:class}`Rendering::PresentationSurface <CE::Rendering::PresentationSurface>` | Consume complete retained frames, maintain graphics resources and present. [Rendering](rendering.md). |
| {cpp:struct}`Rendering::Resources::Provider <CE::Rendering::Resources::Provider>` | Keep the resource backend alive through frame and game-resource release. [Resources](resources.md). |
| {cpp:class}`Events::EventBus <CE::Events::EventBus>` | Route context observations; required view/service work uses owned attachments. [Events](events.md). |
| {cpp:class}`Runtime::PlatformDispatcher <CE::Runtime::PlatformDispatcher>`, {cpp:class}`Runtime::SimulationDispatcher <CE::Runtime::SimulationDispatcher>` | Open, service and close owner queues. [Dispatch](dispatch.md). |
| {cpp:class}`Workers::Group <CE::Workers::Group>` | Close and settle context-tracked jobs while required platform work can progress. [Workers](workers.md). |

### Used by

| Consumer | How it participates |
| --- | --- |
| Application entry point, optionally {cpp:class}`Application::Startup <CE::Application::Startup>` | Select adapters/options, retain game and context, then run one session. [Context assembly](../development/consuming-engine.md#context-assembly). |
| Application game/controllers | Implement the hooks and read `TickContext`; Runtime supplies no scene or entity model. [Character tutorial](../getting-started/create-a/character.md). |
| [GUI sessions](gui.md) | Attach required simulation-view sizing and step-time integration where supported; the application retains session lifetime. |
| [Audio source](audio.md) | Opt into maintenance pumping when desired; audio lifetime remains application-owned. |
| Host or tooling observer | Retain {cpp:class}`Runtime::RuntimeLifecycle <CE::Runtime::RuntimeLifecycle>` for current/terminal state without borrowing the runtime. [Lifecycle observation](../runtime/runtime-frame-boundary.md#lifecycle-observation). |

## Contracts

A context and runtime each support one session. Borrowed game/context/input
objects must survive all use and cleanup. `init_platform()` may upload resources and
register bindings; `update()` and frame preparation must not perform graphics
operations when simulation is concurrent. A borrowed tick, frame writer or pass
writer is valid only for its hook call.
`init_simulation()` runs on the simulation owner with services and the initial
view ready, before Ready, timing and the first update. Initialization and
deinitialization hooks default to no-ops; use constructors for setup without
runtime/thread dependencies.

Input observation time and simulation delta have distinct meanings. Rendering
sees complete published snapshots. Frame preparation may be skipped when no
slot is free, so it must not advance essential game state. Hook failures stop
the session; cleanup preserves the first failure while reporting later ones.

## Lifecycle

The application constructs adapters/context before running. Runtime initializes
input and graphics, attempts `init_platform()`, then establishes the selected
simulation owner/view and calls `init_simulation()`. Each cleanup hook pairs with
its attempted initialization and must tolerate partial failure.

Shutdown stops new context-group submissions and simulation, servicing accepted
platform dependencies while simulation closes its dispatcher, calls
`deinit_simulation()` on its owner, and joins. `quiesce()` then stops external producers
and invalidates borrowed registrations without destroying dependencies still
needed by accepted work. After that work settles and retained frames recycle,
`deinit_platform()` releases game resources while adapters remain alive. Input detaches
before its window disappears; graphics then closes. An injected worker pool's
unrelated groups remain usable.

## Execution

The caller of `run()` owns platform polling and graphics operations. Sequential
mode also updates/prepares on that thread using one frame. Concurrent mode gives
updates and preparation to one simulation thread and uses three frame slots.
Tick input and window/framebuffer dimensions cross as values; accessors do not
marshal live adapter operations onto their required thread.

## Integration

A game requests graceful shutdown from its update hook:

```cpp
void update(const CE::Game::TickContext& tick) override {
    if (tick.input.button(quit_action).pressed()) {
        tick.request_stop();
        return;
    }
    // Advance application-owned state using tick.delta_seconds.
}
```

`quit_action` is an application-defined action registered during initialization.
The stop request does not interrupt the current hook; return when further work
is unnecessary. [First Game](../getting-started/first-game.md) shows the complete
main function and hook wiring.

## Limits and further reading

There is no built-in world, entity, physics or scene lifecycle. The runtime does
not automatically own toolkit sessions or audio. Read the detailed
[frame and shutdown contract](../runtime/runtime-frame-boundary.md),
[simulation timing](../runtime/simulation-timing.md),
[context assembly](../development/consuming-engine.md#context-assembly) and
[failure reporting](../runtime/failure-reporting.md). Public declarations live in
[Context](../../../projects/engine/include/cheryl/core/engine/engine-context.h),
[GameRuntime](../../../projects/engine/include/cheryl/core/game-framework/game-runtime.h)
and [AbstractGame](../../../projects/engine/include/cheryl/core/game-framework/abstract-game.h).
