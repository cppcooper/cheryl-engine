# Events

## Purpose

Events delivers notifications through persistent registrations. A bus owns
callbacks and channel identity; it does not create a thread, choose simulation
timing or automatically own the objects borrowed by those callbacks.

## Interface

Types below are in `CE::Events` unless another namespace is shown.

### Types and operations

| API and purpose |
| --- |
| {cpp:class}`EventBus <CE::Events::EventBus>`<br>Own listener registrations, dispatch, remove/wait and close acceptance. [Registrations](../runtime/event-delivery.md#registrations); [removal and shutdown](../runtime/event-delivery.md#removal-and-shutdown). |
| {cpp:class}`EventChannel <CE::Events::EventChannel>`<br>Give a copy-constructible payload exact name/type identity. [Typed channels](../runtime/event-delivery.md#typed-channels). |
| {cpp:class}`EventBus::Registration <CE::Events::EventBus::Registration>`<br>Retain an explicit removal identity; dropping it does not unregister. [Registration lifetime](../runtime/event-delivery.md#registrations). |
| {cpp:class}`EventBus::Publisher <CE::Events::EventBus::Publisher>`<br>Retain a weak notification route without retaining the bus or Context. [Publishing handles](../runtime/event-delivery.md#weak-publishing-handles). |
| {cpp:type}`EventBus::Delivery <CE::Events::EventBus::Delivery>`, {cpp:type}`EventBus::ErrorHandler <CE::Events::EventBus::ErrorHandler>`<br>Select queued execution and an independent failure sink. [Queued delivery](../runtime/event-delivery.md#optional-queued-delivery); [adapter catalogue](../architecture/interfaces-adapters.md#integrations-alongside-the-runtime). |
| {cpp:struct}`EventSystem <CE::Events::EventSystem>`<br>Access the default bus when an isolated application-owned bus is unnecessary. [Bus ownership](../runtime/event-delivery.md). |

### Delivery and cleanup choices

| Choice | Purpose and details |
| --- | --- |
| Immediate delivery | Invoke on each dispatch caller, in registration order; concurrent callers may overlap. [Immediate delivery](../runtime/event-delivery.md#immediate-delivery). |
| Platform / simulation delivery | Queue owned payloads through a saved owner endpoint; never execute inline. [Queued adapters](../runtime/event-delivery.md#optional-queued-delivery); [dispatch](../runtime/thread-dispatch.md). |
| Worker delivery | Give one listener stream serial FIFO execution over shared group capacity. [Worker streams](../runtime/event-delivery.md#worker-stream-acceptance-and-cancellation). |
| Unregister / unregister-and-wait / close | Invalidate new invocation, optionally settle entered callbacks, or close all acceptance. [Removal and shutdown](../runtime/event-delivery.md#removal-and-shutdown). |

## Usage

Register on the bus selected by the producer, choose the callback's execution
owner, and retain the registration for explicit cleanup. Use the Context bus for
runtime observations or an application-owned bus for an isolated domain.

```{engine-diagram} events-focus-delivery
```

[Explore the immediate-only path](diagram-explorer.md#events-focus-immediate),
[application registrations](diagram-explorer.md#events-application), or
[engine producers and delivery dependencies](diagram-explorer.md#events-internal).

### Choose delivery and participation

| Need | Choose | Responsibility |
| --- | --- | --- |
| Observe immediately on the publishing owner | No delivery adapter | The callback runs synchronously; overlapping publishers may enter it concurrently. |
| Change platform/graphics-owned state | `Events::platform_delivery(endpoint)` | Save a platform submission endpoint and supply an independently owned, nonthrowing error sink. |
| Change simulation-owned gameplay or UI | `Events::simulation_delivery(endpoint)` | Save a simulation endpoint; callback entry occurs at a scheduled simulation boundary. |
| Process observations serially away from owners | `Events::worker_delivery(group)` | Keep physical worker capacity alive; copies of one adapter share one FIFO stream. |
| Apply required sizing or recurring runtime work | View/service attachment | Use the owner's `watch()`, `on_maintenance()` or `on_step()` API and retain its attachment. An optional event cannot gate dependent runtime work. |

A bus subscription has no replay. Read the producer's retained snapshot or use
its seeded watch when current state matters. The full
[observation/participation contract](../runtime/event-delivery.md#observe-a-change-or-participate-in-an-operation)
and [queued-delivery example](../runtime/event-delivery.md#optional-queued-delivery)
cover setup, failure handling and teardown.

### Uses

| Dependency | Where it belongs |
| --- | --- |
| {cpp:class}`Events::EventChannel <CE::Events::EventChannel>` and the exact payload type | The bus identifies typed subscriptions by channel name and payload type. |
| {cpp:type}`Events::EventBus::Delivery <CE::Events::EventBus::Delivery>`, {cpp:type}`Events::EventBus::ErrorHandler <CE::Events::EventBus::ErrorHandler>` | Optional queued transport and its required error authority. The immediate bus itself owns no dispatcher or worker pool. |
| {cpp:class}`Runtime::PlatformDispatcher::Submission <CE::Runtime::PlatformDispatcher::Submission>`, {cpp:class}`Runtime::SimulationDispatcher::Submission <CE::Runtime::SimulationDispatcher::Submission>` | Retained by the optional owner-delivery adapters. [Dispatch](dispatch.md). |
| {cpp:class}`Workers::Group <CE::Workers::Group>` | Shared CPU capacity used by the optional worker-delivery adapter. [Workers](workers.md). |
| {cpp:type}`Diagnostics::DomainId <CE::Diagnostics::DomainId>` | Identify bus observations and failures; diagnostics does not settle callbacks. [Diagnostics](diagnostics.md). |

### Used by

| Consumer | How it connects |
| --- | --- |
| [Runtime](runtime.md), [Input](input.md), [Resources](resources.md) and [Rendering](rendering.md) | Runtime connects supported producers to its Context bus at session start. View/lifecycle/service facts are also published there. |
| Native GLFW [Display](display.md) | Publish raw framebuffer resize through the default `Events::EventSystem` bus. |
| [Asset loaders/catalogues](assets.md) | Publish executed upload outcomes and committed exact-key catalogue changes through configured weak routes. |
| [Audio output](audio.md) | Stage terminal playback outcomes and explicitly publish them on its designated owner. |
| Application producers/listeners | Define their own channels and bus boundaries; own callback targets and explicit registration removal. |

### Engine events

The following are the public typed channels supplied by Engine and its modules.
Names omit the leading `CE::`. Register on the producer's bus, not merely on a
channel with the same spelling. Unless stated otherwise, Context-connected
producers below publish through `engine.events()`; the default bus is a separate
instance. Payloads describe observations, not authority to change the producing
system.

#### Window, view and input observations

| Channel and payload | Producer and meaning |
| --- | --- |
| `Platform::window_resized_event`<br>{cpp:struct}`Platform::WindowResized <CE::Platform::WindowResized>` | Native GLFW, on platform via `Events::EventSystem::get()`: changed drawable pixels and a borrowed window pointer. The legacy named channel is offered first. Retain the size alone for deferred use. [Resize contract](../runtime/display-and-window-contract.md#sizes-modes-and-observation). |
| `Platform::window_close_requested_event`<br>{cpp:struct}`Platform::WindowCloseRequested <CE::Platform::WindowCloseRequested>` | Runtime, on platform: one sampled main-window close intent with owned context/session/source/intent IDs. No window pointer or shutdown decision capability. [Close bridge](../runtime/display-and-window-contract.md#native-close-intent). |
| `Runtime::window_state_changed_event`<br>{cpp:struct}`Runtime::WindowStateChanged <CE::Runtime::WindowStateChanged>` | Runtime's platform selection: previous/current logical size, pixels and optional native focus. Read `engine.window_state().snapshot()` for current state. [Committed views](../runtime/display-and-window-contract.md#committed-window-and-simulation-views). |
| `Runtime::simulation_view_changed_event`<br>{cpp:struct}`Runtime::SimulationViewChanged <CE::Runtime::SimulationViewChanged>` | Runtime's simulation selection before a dependent tick: previous/current copied size pairs. Required sizing uses `engine.simulation_view().watch()` instead. [View contract](../runtime/display-and-window-contract.md#committed-window-and-simulation-views). |
| `Input::gamepad_availability_changed_event`<br>{cpp:struct}`Input::GamepadAvailabilityChanged <CE::Input::GamepadAvailabilityChanged>` | Input's platform owner after complete polls and explicit deinitialization: current/previous convenience-slot state. Deinitialization does not mean physical disconnection; destruction closes the source silently. Subscribe then read `gamepad_availability()` on platform. [Availability](../runtime/input-state-model.md#gamepad-availability). |

#### Session and shutdown observations

All six channels run their immediate observers on platform and carry an owned
lifecycle selection. `runtime.lifecycle().snapshot()` retains current/terminal
state after runtime destruction. Milestone delivery does not replay to a late
subscriber.

| Channel and payload | When it is observed |
| --- | --- |
| `Runtime::runtime_ready_event`<br>{cpp:struct}`Runtime::RuntimeReady <CE::Runtime::RuntimeReady>` | Successful owner/game initialization, before the first update. Simulation is still gated during immediate Ready fanout; do not synchronously wait for it. |
| `Runtime::shutdown_starting_event`<br>{cpp:struct}`Runtime::ShutdownStarting <CE::Runtime::ShutdownStarting>` | Shutdown commits, before simulation joins; partial startup can reach this milestone without Ready. |
| `Runtime::runtime_quiescing_event`<br>{cpp:struct}`Runtime::RuntimeQuiescing <CE::Runtime::RuntimeQuiescing>` | Simulation/mailbox has stopped and applicable game quiescence has been attempted, before accepted CPU work settles. |
| `Runtime::runtime_stopped_event`<br>{cpp:struct}`Runtime::RuntimeStopped <CE::Runtime::RuntimeStopped>` | Ordered cleanup was attempted and the terminal outcome/counters were retained. Failed cleanup does not prove successful native resource release. |
| `Runtime::shutdown_requested_event`<br>{cpp:struct}`Runtime::ShutdownRequested <CE::Runtime::ShutdownRequested>` | An ordinary request was admitted Pending. Only the installed host policy receives the decision capability. |
| `Runtime::shutdown_request_cancelled_event`<br>{cpp:struct}`Runtime::ShutdownRequestCancelled <CE::Runtime::ShutdownRequestCancelled>` | An ordinary request was cancelled, with its identity and reason. Forced supersession does not fabricate cancellation. |

Read the [lifecycle milestones](../runtime/runtime-frame-boundary.md#lifecycle-observation)
and [negotiable shutdown protocol](../runtime/runtime-frame-boundary.md#negotiable-shutdown)
for their ordering and retained request state.

#### Runtime service observations

| Channel and payload | Producer and meaning |
| --- | --- |
| `Runtime::platform_maintenance_event`<br>{cpp:struct}`Runtime::PlatformMaintenance <CE::Runtime::PlatformMaintenance>` | A real platform service pass, including its reason and pass identity. Maintenance does not pump native input. |
| `Runtime::simulation_step_starting_event`<br>{cpp:struct}`Runtime::SimulationStepStarting <CE::Runtime::SimulationStepStarting>` | Simulation selected input/view and an attempted step's timing before the game hook. This does not prove that update completed. |

Use [required service attachments](../runtime/runtime-frame-boundary.md#runtime-service-phases)
for work that must finish before dependent runtime operations.

#### Asset, graphics and audio observations

| Channel and payload | Producer, setup and meaning |
| --- | --- |
| `Assets::Loading::asset_upload_settled_event`<br>{cpp:struct}`Assets::Loading::AssetUploadSettled <CE::Assets::Loading::AssetUploadSettled>` | Loader's serialized loading owner: one aggregate outcome per executed attempt, with counts and an owned failure. Configure `loader.publish_events_to(...)`; CPU preparation and pre-execution cancellation produce no settlement event. Direct return/exception/future remains authoritative. [Upload settlement](../assets/asset-loading.md#upload-settlement-notifications). |
| `Assets::Cache::asset_catalogue_changed_event<AssetType, Key>`<br>{cpp:struct}`Assets::Cache::AssetCatalogueChanged <CE::Assets::Cache::AssetCatalogueChanged>` | Catalogue drainer, potentially a producer thread: an exact-key publish/replace or reset, with generations and loss metadata. Each asset/key type pair is a distinct channel. Runtime routes provider catalogues; standalone sources opt in. Read a snapshot or seeded watch to reconcile current keys. [Catalogue changes](../assets/asset-loading.md#catalogue-changes). |
| `Rendering::graphics_domain_state_changed_event`<br>{cpp:struct}`Rendering::GraphicsDomainStateChanged <CE::Rendering::GraphicsDomainStateChanged>` | Graphics owner: actual native availability/invalidation, with a historical snapshot and retained observation token. Runtime configures renderer routing. The token retains no native objects. [Graphics domains](../resources/resource-residency.md#graphics-domain-observation). |
| `Audio::playback_ended_event`<br>{cpp:struct}`Audio::PlaybackEnded <CE::Audio::PlaybackEnded>` | Designated audio publication owner: a logical voice generation's terminal reason. The application configures a weak publisher and explicitly pumps staged records, optionally through runtime maintenance. Counters expose bounded-record loss. [Playback outcomes](../runtime/audio.md#playback-outcomes). |

No public event family currently reports `FontService` catalogue/configuration changes, monitor hotplug,
full physical-device topology, or OS suspend/resume. Input's ordered `Input::Events::Record` stream is
read from ticks/polls, separately from these bus channels. Runtime pressure and
shutdown-policy transport channels are implementation details.

## Contracts

Registration persists until explicit removal or bus destruction; dropping its
identifier does not unregister. Immediate callbacks run in registration order
on each dispatch caller, and overlapping dispatches can overlap listeners.
Typed channels require the exact copy-constructible payload type.

Unregister prevents fresh invocation but a callback already entered may finish.
Use `unregister_and_wait()` before destroying a borrowed target, without holding
a lock that callback needs. Waiting from that same listener rejects. Queued
payloads own their copied values; pointees remain governed by the payload type.

## Lifecycle

Construct an owned bus or deliberately use the default. Retain registration IDs
for cleanup. Stop producers, invalidate registrations and settle entered
callbacks before destroying borrowed targets. `close()` invalidates and rejects
new work but does not wait for running callbacks. An independent queued error
sink must survive pending-ticket destruction even after its listener is removed.

## Execution

Immediate delivery is synchronous on dispatch's caller. Queued delivery owns
the work without invoking it inline, preserving each listener's FIFO stream.
Platform/simulation adapters follow those owners; copies of one worker-delivery
adapter share a serial stream. Independent listeners/targets have no global
completion order. Immediate `dispatch()` failures propagate and stop its remaining
listeners; `notify()` isolates failures and attempts later listeners. Queued
failures reach the registration-owned error sink.

## Integration

An application-owned bus can notify simulation-owned listeners synchronously:

```cpp
CE::Events::EventBus bus;
const CE::Events::EventChannel<int> score_changed{"score-changed"};
int displayed_score = 0;
auto registration = bus.register_listener(score_changed, [&](const int& value) {
    displayed_score = value;
});
bus.dispatch(score_changed, 10);
static_cast<void>(bus.unregister_and_wait(registration));
```

Here the caller supplies synchronization by performing all operations on one
owner. Choose a delivery adapter before registering when producers use other
threads.

## Limits and further reading

Channel names/RTTI form an in-process identity, not a serialization protocol.
The default bus does not provide automatic callback lifetime or UI ownership.
Read [event delivery and failure contracts](../runtime/event-delivery.md),
[thread dispatch](../runtime/thread-dispatch.md) and
[the bus interface](../../../projects/engine/include/cheryl/core/subsystems/event-bus.h).
