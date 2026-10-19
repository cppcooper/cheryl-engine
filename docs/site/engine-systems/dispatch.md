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

```{graphviz}
:caption: Dispatch integration
:alt: Asset, text, GUI and worker producers submit owned requests to Dispatch. Runtime drains its queues. Platform callbacks use Resources or Display; simulation callbacks change application state. Events can opt into owner delivery.

digraph dispatch_usage {
    rankdir=LR;
    dispatch [label="Dispatch", penwidth=2];
    producers [label="Assets / Text / GUI\napplication producers / jobs"];
    runtime [label="Runtime"];
    events [label="Events"];
    platform [label="Platform / graphics owner\nResources / Display"];
    simulation [label="Simulation owner\napplication state"];
    producers -> dispatch [label="submits owned requests;\nretains futures"];
    runtime -> dispatch [label="opens / drains / closes\non selected owners"];
    events -> dispatch [label="optional: owner delivery", style=dashed];
    dispatch -> platform [label="executes platform callbacks\nwith live Context"];
    dispatch -> simulation [label="executes simulation callbacks\nbefore updates"];
}
```

### Uses

Runtime opens, drains and closes queues on their selected owners. Submitted
platform work receives its live Context.

| Types | Systems |
| --- | --- |
| {cpp:class}`CE::Runtime::Context`<br>{cpp:struct}`CE::Diagnostics::DispatchStats`<br>{cpp:type}`CE::Diagnostics::DomainId` | [Runtime](runtime.md)<br>[Diagnostics](diagnostics.md) |

### Used by

Consumers send owned uploads or simulation changes to the required owner.
Platform callbacks may operate Resources or Display through Context. Event
delivery remains composed outside the bus; general parallel CPU work belongs to
Workers.

| Types | Systems |
| --- | --- |
| {cpp:type}`CE::Events::EventBus::Delivery`<br>{cpp:class}`CE::UI::TGUI::SceneUploader`<br>{cpp:class}`CE::UI::RmlUi::SceneUploader`<br>[`Tutorial::UIController`](../getting-started/create-a/ui-controller.md) | [Events](events.md)<br>[Assets](assets.md)<br>[Text](text.md)<br>[GUI](gui.md)<br>[Resources](resources.md) |

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

During `init()`, obtain a copyable endpoint and retain it in the producer that
will submit requests. During the live session that producer can request a window
observation on its owner:

```cpp
auto platform_requests = engine_.platform_dispatcher().submission(); // Save during init().
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
