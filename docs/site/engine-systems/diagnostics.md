# Diagnostics

## Purpose

Diagnostics exposes operational observations: session counts, queue activity,
resource retirement, worker policy, asset publication and memory bookkeeping.
It helps correlate a failure without changing the owning operation's semantics.

## Interface

Types below are in `CE::Diagnostics` unless another namespace is shown.

### Types and observations

| API and purpose |
| --- |
| {cpp:type}`DomainId <CE::Diagnostics::DomainId>`<br>Correlate one process-local owner through `diagnostic_id()` or snapshot domain fields. [Domain identities](../runtime/subsystem-diagnostics.md). |
| {cpp:struct}`CE::Runtime::RuntimeStats`<br>Inspect completed-session update, poll, frame and pressure counts. [Runtime statistics](../runtime/subsystem-diagnostics.md#runtime-statistics). |
| {cpp:struct}`CE::Workers::GroupStatus`, {cpp:struct}`DispatchStats <CE::Diagnostics::DispatchStats>`<br>Observe CPU work, queue activity and reported policy behavior. [Worker statistics](../runtime/subsystem-diagnostics.md#worker-statistics); [dispatcher statistics](../runtime/subsystem-diagnostics.md#dispatcher-statistics). |
| {cpp:struct}`CE::Events::EventStats`<br>Observe registration, dispatch and delivery activity. [Event statistics](../runtime/subsystem-diagnostics.md#event-statistics). |
| {cpp:struct}`CE::Assets::Loading::UploadStats`, {cpp:struct}`CE::Memory::Stats`<br>Inspect asset publication and synchronized CPU allocation bookkeeping. [Asset diagnostics](../runtime/subsystem-diagnostics.md#asset-diagnostics); [memory statistics](../runtime/subsystem-diagnostics.md#memory-statistics). |

### Observation boundaries

| Observation | Purpose and details |
| --- | --- |
| `GameRuntime::diagnostics()` | Owned completed-session counts after synchronization with run completion; live sampling rejects. [Runtime statistics](../runtime/subsystem-diagnostics.md#runtime-statistics). |
| Queue/group status | Activity at the snapshot boundary; an empty queue does not prove detached work finished. [Worker](../runtime/subsystem-diagnostics.md#worker-statistics) and [dispatcher](../runtime/subsystem-diagnostics.md#dispatcher-statistics) limits. |
| Resource retirement statistics | Tracked native resources and retirement/collection activity in one backend domain. [Native resource statistics](../runtime/subsystem-diagnostics.md#native-resource-statistics). |
| Explicit diagnostic report | A bounded operational summary outside sensitive locks/native callbacks; required emergency failure output has a separate path. [Failure reporting](../runtime/failure-reporting.md#emergency-reporting); [validation limits](../runtime/subsystem-diagnostics.md#validation-and-limits). |

## Usage

Read the service that owns the observation, retain its copied values, then
interpret them with the operation's result or failure. Diagnostics does not
create a global monitor that queries every subsystem for the application.

```{engine-diagram} diagnostics-application
```

The [owner dependency view](diagram-explorer.md#diagnostics-internal) shows
the available snapshot families and their originating services.

### Choose an observation boundary

| Question | Query/result | When and how to interpret it |
| --- | --- | --- |
| How did this session progress? | `Runtime::GameRuntime::diagnostics()` → {cpp:struct}`Runtime::RuntimeStats <CE::Runtime::RuntimeStats>` | Synchronize with `run()` returning or throwing. Unstarted/completed sessions may be sampled; live sampling rejects. |
| Is worker work queued/running or using fallback policy? | `Workers::Group::status()` → {cpp:struct}`Workers::GroupStatus <CE::Workers::GroupStatus>`; `policy()` → {cpp:struct}`Workers::GroupPolicy <CE::Workers::GroupPolicy>` | Status observes activity/failures; policy reports effective eligibility and preferred fallback. Use each job's future for its result. |
| Is dispatched work still queued or detached? | Platform/simulation dispatcher `diagnostics()` → {cpp:struct}`Diagnostics::DispatchStats <CE::Diagnostics::DispatchStats>` | `pending` omits detached work; also inspect `running`. A snapshot is not a join. |
| What happened to event delivery? | `Events::EventBus::diagnostics()` → {cpp:struct}`Events::EventStats <CE::Events::EventStats>` | Registration, invocation, discard and queued-failure observations; no event payload/name sampling. |
| Did an upload publish anything before failing? | `Assets::Loading::Loader::diagnostics()` → {cpp:struct}`Assets::Loading::UploadStats <CE::Assets::Loading::UploadStats>` | Last-attempt counts/completion can describe partial publication. Keep the original upload error; no batch rollback is implied. |
| Are cache entries still owned? | `Assets::Cache::Store::diagnostics()` → {cpp:struct}`Assets::Cache::Stats <CE::Assets::Cache::Stats>` | Separates current entries from cumulative publications/replacements; clearing one cache does not drop every asset owner. |
| Which CPU ranges remain managed? | `Memory::Manager::diagnostics()` → {cpp:struct}`Memory::Stats <CE::Memory::Stats>` | Synchronized byte-domain totals, free bytes, owners and partitions; not live-object or GPU-byte counts. |
| Are native handles awaiting retirement? | `Rendering::OpenGL::ResourceLifetime::diagnostics()` → {cpp:struct}`Rendering::OpenGL::ResourceStats <CE::Rendering::OpenGL::ResourceStats>` | Compare live/pending/collected ownership on the selected backend; sampling performs no graphics collection. |

Follow the [symptom, evidence and action table](../runtime/subsystem-diagnostics.md#investigate-a-runtime-symptom)
when deciding what to sample. It connects an observation to the owner whose
policy or lifetime may need investigation.

### Uses

| Observer | Dependency | Relationship |
| --- | --- | --- |
| Application report | The owner-specific snapshot APIs above | Copies observations under each owner's access rules; it does not acquire service ownership through the result. |
| Runtime/resource/work correlation | {cpp:type}`Diagnostics::DomainId <CE::Diagnostics::DomainId>` | Joins observations from the same process-local owner. IDs are neither addresses nor persistent asset/session keys. |
| Ordinary formatted report | {cpp:class}`Logging::Logger <CE::Logging::Logger>` | Selects a named destination after sampling, outside containing locks/native callbacks. [Logging](logging.md). |
| Required native/cleanup failure report | [Bounded emergency output](../runtime/failure-reporting.md#emergency-reporting) | Preserves failure context independently of ordinary logging and leaves the owning exception/future authoritative. |

### Used by

| Owner | Diagnostic identity/observation | Useful correlation |
| --- | --- | --- |
| [Runtime](runtime.md) | `Runtime::GameRuntime` / `Runtime::Context` domains and completed `RuntimeStats` | Connect a session outcome to update, polling and frame counts. |
| [Workers](workers.md), [Dispatch](dispatch.md), [Events](events.md) | Pool/group/dispatcher/bus domains and queue/delivery observations | Distinguish admission, pending, detached/running, completion and failure. |
| [Assets](assets.md) | Prepared batch, `Rendering::Resources::Provider` and cache domains | Connect CPU preparation, provider upload and actual publication without relying on source paths. |
| [Memory](memory.md), [Resources](resources.md) | Shared byte-domain and native resource-lifetime counts | Separate retained CPU backing from backend-native retirement. |
| [Display](display.md), [Input](input.md), [Rendering](rendering.md) | Explicit platform/native operational reports | Inspect capabilities, resize/focus epochs or supported debug observations without assuming every native call is traced. |

### Retain values, compare domains

A copied snapshot can outlive the observed service. Its values do not keep that
service alive, refresh themselves or make a query safe during destruction.
Retain the owner through the query and obey its thread/lifecycle boundary.

Compare counters only within the domain and scope they describe. Memory growth
facades share the byte bookkeeping domain; worker group and physical pool IDs
identify different owners. A zero domain means unavailable/exhausted. Counts
sampled from different owners are not one atomic engine-wide snapshot. The
[diagnostics guide](../runtime/subsystem-diagnostics.md) defines each family's
sampling and reporting limits.

## Contracts

Counters are observations, not completion barriers, admission limits or resource
budgets. Domain IDs are monotonic process-local identities, not addresses/native
handles or durable keys; zero indicates exhaustion. Unavailable observations
must remain explicit.

`GameRuntime::diagnostics()` requires caller synchronization with run start and
return; live sampling rejects. An empty dispatcher queue does not establish that
detached work completed. Asset completion counts do not establish batch rollback
or distinguish every reused key from a newly published entry without their
documented counters.

## Lifecycle

An owner acquires its identity with construction and publishes observations at
documented boundaries. Take a completed runtime snapshot after `run()` returns
or throws, retaining the runtime through that query. Explicit reports must run
outside containing locks/native callbacks according to the subsystem contract.
Retained snapshots own their values without retaining the observed service.

## Execution

Each snapshot follows the observed owner's access rules. Reporting can allocate,
format or log, so it is not suitable for arbitrary destructor/native callback
contexts. Emergency reporting has a deliberately separate bounded contract and
must not mask the original exception.

## Integration

After synchronizing with runtime completion:

```cpp
const auto stats = runtime.diagnostics();
const auto updates = stats.updates;
const auto rendered = stats.rendered;
```

Use these values in an application report with the run outcome. Different
update/render counts are expected with independent scheduling and skipped or
superseded visual snapshots. Use the
[symptom, evidence and action table](../runtime/subsystem-diagnostics.md#investigate-a-runtime-symptom)
to decide which observation to inspect before changing a scheduling or resource policy.

## Limits and further reading

Counters do not measure cache speedups, native thread residency or unselected
hardware behavior. Read [subsystem observation limits](../runtime/subsystem-diagnostics.md),
[failure reporting](../runtime/failure-reporting.md),
[architecture acceptance limits](../development/architecture-validation.md#evidence-limits)
and [diagnostic declarations](../../../projects/engine/include/cheryl/core/diagnostics.h).
