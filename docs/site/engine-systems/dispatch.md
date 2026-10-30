# Dispatch

## Purpose

Dispatch transfers owned work to the platform/graphics or simulation owner.
Use it when an operation requires that owner, rather than running arbitrary CPU
work through an owner queue or calling a thread-affine adapter directly.

## Interface

Types below are in `CE::Runtime` unless another namespace is shown.

### Types and operations

| API and purpose |
| --- |
| {cpp:class}`PlatformDispatcher <CE::Runtime::PlatformDispatcher>`<br>Submit work taking `Context&` to platform/graphics through the context. [Platform dispatch](../runtime/thread-dispatch.md). |
| {cpp:class}`SimulationDispatcher <CE::Runtime::SimulationDispatcher>`<br>Submit work without a context argument to simulation through the runtime. [Simulation boundary](../runtime/thread-dispatch.md#simulation-boundary). |
| {cpp:class}`PlatformDispatcher::Submission <CE::Runtime::PlatformDispatcher::Submission>`, {cpp:class}`SimulationDispatcher::Submission <CE::Runtime::SimulationDispatcher::Submission>`<br>Save copyable submission endpoints that reject safely after closure/destruction. [Saved endpoints and cancellation](../runtime/thread-dispatch.md). |
| {cpp:struct}`CE::Diagnostics::DispatchStats`<br>Inspect accepted/completed/pending observations without treating queue emptiness as completion. [Dispatcher statistics](../runtime/subsystem-diagnostics.md#dispatcher-statistics). |

### Submission and completion

| Operation | Purpose and details |
| --- | --- |
| `submit()` | Decay-own work/captures and retain its future; borrowed pointees still need a lifetime. [Ownership and future results](../runtime/thread-dispatch.md). |
| `submission()` | Retain queue state independently of the borrowed service facade. [Endpoint lifetime](../runtime/thread-dispatch.md). |
| Future readiness / result | Consume accepted results/errors without waiting on the owner needed to produce them. [Dependency progress](../runtime/runtime-frame-boundary.md#queued-preparation-and-stop-requests). |
| Runtime closure | Settle required accepted dependencies, then cancel unexecuted work before game cleanup. [Shutdown order](../runtime/runtime-frame-boundary.md#shutdown). |

## Usage

Save a copyable submission endpoint during initialization. Producers submit owned
inputs through that endpoint and retain the returned future; a later update
adopts the result after readiness is observed.

```{engine-diagram} dispatch-application
```

[Explore dispatcher ownership and engine consumers](diagram-explorer.md#dispatch-internal).

### Choose the destination

| Work | Queue or service | Execution boundary |
| --- | --- | --- |
| Upload resources, mutate/query a native window, perform graphics operations | `engine.platform_dispatcher().submission()` | {cpp:class}`Runtime::PlatformDispatcher <CE::Runtime::PlatformDispatcher>` invokes `work(Runtime::Context&)` on platform with the selected graphics context. |
| Apply an owned result to simulation state | `runtime.simulation_dispatcher().submission()` | {cpp:class}`Runtime::SimulationDispatcher <CE::Runtime::SimulationDispatcher>` invokes `work()` at the scheduled simulation boundary before update. Posting does not create an early tick. |
| Decode, shape or otherwise prepare CPU data | {cpp:class}`Workers::Group <CE::Workers::Group>` | Independent CPU capacity; the application arranges later owner upload/adoption. [Workers](workers.md). |
| Observe an event on a selected owner | `Events::platform_delivery()` / `simulation_delivery()` | Adapt a saved endpoint into event delivery; retain a registration and independent error sink. [Events](events.md#uses). |

Submission always queues, even when the producer is already on the destination
owner. Capture owned values, keep borrowed targets alive through execution or
cancellation, and avoid waiting on work that needs the current owner. The
[complete result-adoption example](../runtime/thread-dispatch.md#submit-and-adopt-a-result)
shows endpoint storage, readiness checking and error handling.

### Uses

| Dependency | Dispatch's use |
| --- | --- |
| {cpp:class}`Runtime::Context <CE::Runtime::Context>` | Supply the platform callback's live adapter/resource access. Simulation callbacks receive no Context argument. [Runtime](runtime.md). |
| {cpp:struct}`Diagnostics::DispatchStats <CE::Diagnostics::DispatchStats>` | Observe accepted, queued, running, completed and cancelled work. These counters do not replace the request's future. [Diagnostics](diagnostics.md). |
| [Logging](logging.md) | Report queue lifecycle and high-water observations; callback failures remain with their futures. |

`GameRuntime` controls opening, owner binding, draining and closure. A saved
endpoint retains queue state, without retaining Context or native adapters.

### Used by

| Consumer | What it submits |
| --- | --- |
| {cpp:class}`Runtime::GameRuntime <CE::Runtime::GameRuntime>` | Service owner batches and settle/cancel outstanding work in the session shutdown order. |
| {cpp:class}`UI::TGUI::SceneUploader <CE::UI::TGUI::SceneUploader>`, {cpp:class}`UI::RmlUi::SceneUploader <CE::UI::RmlUi::SceneUploader>` | Upload owned prepared scene data to the graphics owner. [GUI](gui.md). |
| [Text updates](../getting-started/how-to/update-text.md) | Prepare away from graphics, submit upload and adopt a ready generation on simulation. |
| [Event delivery adapters](../runtime/event-delivery.md#optional-queued-delivery) | Transfer optional callbacks through a saved platform/simulation endpoint; the bus itself does not own a dispatcher. |
| Application asset preparation or jobs | Submit owner-affine follow-up using saved endpoints. A Loader or worker pool does not automatically schedule that orchestration. [Assets](assets.md). |

## Contracts

Each drain detaches one FIFO batch. New/nested posts wait for another drain;
submission never executes inline. Captures are decay-owned, but references and
reference-valued results still borrow targets. Accepted exceptions reach the
future; unexecuted cancellation breaks its promise. Never wait on a future from
the owner needed to complete it.

Endpoints retain queue state rather than borrowed service pointers. They reject
before opening, after closing and after destruction. `has_pending()` only sees
queued work; an empty queue does not establish completion of a detached batch.

## Lifecycle

Runtime opens queues during initialization and binds simulation to its selected
owner. Save endpoints for longer-lived adapters without extending backend
lifetime. During shutdown, runtime services accepted dependencies while joining
simulation and settling tracked CPU work, then cancels remaining requests before
game cleanup. Borrowed captures must survive either execution or cancellation.

## Execution

Platform requests run with the runtime's selected graphics context on platform.
Simulation batches drain before scheduled updates without consuming input merely
because mailbox work ran. Cross-domain ordering follows explicit dependencies;
there is no total order between independent platform, simulation and worker work.

## Integration

During `init_platform()`, obtain a copyable endpoint and retain it in the producer that
will submit requests. During the live session that producer can request a window
observation on its owner:

```cpp
auto platform_requests = engine_.platform_dispatcher().submission(); // Save during init_platform().
// Later, the producer uses its saved endpoint:
auto pending = platform_requests.submit([](CE::Runtime::Context& owner) {
    return owner.window().framebuffer_size();
});
```

The endpoint and `pending` are retained values, not temporary service pointers.
[The complete readiness example](../runtime/thread-dispatch.md#submit-and-adopt-a-result)
shows their member storage and adoption during update. Inspect future readiness
from later updates; do not block simulation while it
depends on platform progress. Ordinary game layout should use tick dimensions
directly; the example shows where an explicitly requested live observation runs.

## Limits and further reading

Dispatch is not a general executor, a completion barrier based on queue emptiness
or automatic lifetime extension for captured raw pointers. Read
[thread dispatch](../runtime/thread-dispatch.md),
[runtime shutdown](../runtime/runtime-frame-boundary.md#shutdown) and
[the platform interface](../../../projects/engine/include/cheryl/core/engine/platform-dispatcher.h).
