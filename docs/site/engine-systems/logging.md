# Logging

## Purpose

Logging records operational information in named destinations with explicit
configuration, filtering and sink lifetimes. Ordinary logs support diagnosis;
required failure propagation and bounded emergency reporting remain independent.

## Interface

Types below are in `CE::Logging` unless another namespace is shown.

### Types and operations

| API and purpose |
| --- |
| {cpp:class}`Logger <CE::Logging::Logger>`, {cpp:class}`Log <CE::Logging::Log>`<br>Select a named category and initialize, emit, filter, flush, close or reopen it. [Runtime settings and ownership](../runtime/logging.md#runtime-settings-and-ownership). |
| {cpp:struct}`Config <CE::Logging::Config>`<br>Choose destinations, rotation, levels and queue overflow behavior before producers start. [Configuration and ownership](../runtime/logging.md#runtime-settings-and-ownership). |
| {cpp:struct}`FileEventHandlers <CE::Logging::FileEventHandlers>`<br>Supply scoped file-open/close callbacks while retaining borrowed targets through final closure. [Callback and reentry boundaries](../runtime/logging.md#filtering-and-failure-boundaries). |
| {cpp:struct}`QueueStats <CE::Logging::QueueStats>`<br>Observe queued/discarded work in the shared logging pool. [Queue observations](../runtime/logging.md#runtime-settings-and-ownership). |

### Levels and policy enums

| Type | Choices and details |
| --- | --- |
| {cpp:enum}`Level <CE::Logging::Level>` | `trace`, `debug`, `info`, `warn`, `err`, `critical`, `off` for runtime logger/sink thresholds. [Filtering](../runtime/logging.md#filtering-and-failure-boundaries). |
| {cpp:enum}`Profile <CE::Logging::Profile>` | `Developer`, `Support`, `Release` presets through `Config::for_logger()`. [Runtime defaults](../runtime/logging.md#runtime-settings-and-ownership). |
| {cpp:enum}`OverflowPolicy <CE::Logging::OverflowPolicy>` | `Block` waits for queue capacity; `DiscardNew` rejects/counts new work without evicting accepted items. [Queue admission](../runtime/logging.md#runtime-settings-and-ownership). |
| {cpp:enum}`Compile::LogLevel <CE::Logging::Compile::LogLevel>` | Compile-time severity bits used by logging policy; runtime thresholds cannot restore stripped calls. [Compile policy](../runtime/logging.md#compile-policy); [declarations](../../../projects/engine/include/cheryl/core/logging/compile-policy.h). |

## Usage

Application composition owns logging lifetime and configures each used category
before producers begin. Writers use a named
{cpp:class}`Logging::Logger <CE::Logging::Logger>`; an explicitly owned
{cpp:class}`Logging::Log <CE::Logging::Log>` is another option when the caller
needs a separate facade/backend lifetime.

```{engine-diagram} logging-application
```

The [engine dependency view](diagram-explorer.md#logging-internal) follows
subsystem writers into the public logging interfaces.

### Configure, write and finish

| Boundary | Application action | Result or limit |
| --- | --- | --- |
| Process setup | Keep {cpp:class}`Logging::ProcessLifetime <CE::Logging::ProcessLifetime>` alive around all producers and diagnostic readers. The supplied Startup already owns a guard. | Nested guards share the lifetime; the last guard closes live logs and joins the private worker. Shutdown is terminal. |
| Before first category access | Build {cpp:struct}`Logging::Config <CE::Logging::Config>` with `for_logger()` and call `Logger<name>::initialize(handlers, config)`. | Configuration is per category. Repeated explicit initialization rejects, including after lazy creation. |
| Ordinary writes | Use `CE_LOG_*` or `Logger<name>::write_lazy()` when diagnostic argument preparation is costly or can throw. | Compile policy and runtime gates run before guarded argument preparation; direct severity-call arguments are evaluated first. |
| Investigation | Read `shared_queue_stats()` and `backend_stats()` on the existing log. | Queue loss spans categories; degradation/failure counters identify the individual log. Neither query proves output completion. |
| Shutdown/recovery | Stop producers, complete `close()`, then reopen only if recovery is needed and the process lifetime remains active. | A timed-out close leaves Closing intact. `flush()` queues a request; it does not acknowledge file completion or durability. |

Follow [Enable useful output](../runtime/logging.md#enable-useful-output) for a
complete category configuration. Choose `Block` when waiting for shared queue
capacity is acceptable, or `DiscardNew` when counting lost new records is
preferable; the latter can also discard a flush request.

### Uses

| Consumer | Dependency | Relationship |
| --- | --- | --- |
| `Logging::Logger<name>` | `Logging::Log<name>` | Provides lazy named access to centrally registered state keyed by category text across libraries. |
| `Logging::Log<name>` | {cpp:class}`Logging::LogHandle <CE::Logging::LogHandle>` | Derives the public lifecycle/filtering/observation operations; it is not a `State::Singleton` facade. |
| `Logging::LogHandle` | `Logging::Config` and {cpp:struct}`Logging::FileEventHandlers <CE::Logging::FileEventHandlers>` | Owns configured backend state and callbacks through final closure. Callback captures protect borrowed targets. |
| `Logging::Config` | {cpp:enum}`Logging::Profile <CE::Logging::Profile>`, {cpp:enum}`Logging::Level <CE::Logging::Level>` and {cpp:enum}`Logging::OverflowPolicy <CE::Logging::OverflowPolicy>` | Separates initial runtime thresholds and queue admission from the target's compile mask. |
| Guarded emission and backend failure paths | [Bounded Diagnostics failure reporting](../runtime/failure-reporting.md#emergency-reporting) | Reports required failure context independently of ordinary log filtering or sink health. |

The compiled backend owns its queue, worker and sink graph. Applications do not
replace native sinks or keep native logger owners through public API handles.

### Used by

| Consumer | Named destination | What to look for |
| --- | --- | --- |
| {cpp:class}`Runtime::GameRuntime <CE::Runtime::GameRuntime>`, {cpp:class}`Workers::Pool <CE::Workers::Pool>`, dispatchers and event buses | `Logging::Categories::engine` → `engine.log` | Session timing/lifecycle and explicit work/delivery summaries. [Runtime](runtime.md), [Workers](workers.md), [Dispatch](dispatch.md), [Events](events.md). |
| {cpp:class}`Platform::GLFW::DisplaySystem <CE::Platform::GLFW::DisplaySystem>` and native input | `Logging::Categories::platform` → `os-platform.log` | Window/context/input capability and focus observations. [Display](display.md), [Input](input.md). |
| {cpp:class}`Rendering::OpenGL::Renderer <CE::Rendering::OpenGL::Renderer>` and graphics resources | `Logging::Categories::rendering` → `rendering.log` | Renderer, shader and native resource observations. [Rendering](rendering.md), [Resources](resources.md). |
| {cpp:struct}`Assets::Loading::Loader <CE::Assets::Loading::Loader>` and caches | `Logging::Categories::assets` → `assets.log` | Preparation, upload/publication and reload outcomes. [Assets](assets.md). |
| {cpp:struct}`Memory::Manager <CE::Memory::Manager>` | `Logging::Categories::memory` → `memory.log` | Explicit preallocation and copied bookkeeping reports. [Memory](memory.md). |

These files are separate destinations, not an aggregate mirror. Configuring
`engine` alone does not configure the other categories. The
[category guide](../runtime/logging.md) owns default paths and custom names.

### Ordinary output and required failure context

Ordinary records can be compiled out, filtered, delayed, discarded or suppressed
by degraded sinks. Preserve the owning exception/future independently. Native
callbacks, final deleters and noexcept cleanup use the bounded
[emergency path](../runtime/failure-reporting.md#emergency-reporting), outside
ordinary logger acquisition. Keep file callbacks free of logger lifecycle
reentry and waits for work that needs the backend.

## Contracts

Initialize explicitly before lazy access/writers when configuration matters.
Severity filters cannot restore code removed by compilation policy. Queue
admission, rotation and callback failures follow the configured contract;
logging is not a substitute for the operation's exception or future.

Sink callbacks must respect backend reentry guards and their documented borrowed
arguments. Do not emit ordinary logging while holding sensitive subsystem locks
or from native/noexcept failure paths that require bounded emergency reporting.

## Lifecycle

Configure the used categories before starting producers, run those producers,
stop/settle them, then flush and close logging. Close/reopen serializes state
transitions; a timed close can report an incomplete operation rather than
pretending all queued work has completed. Keep callback targets alive until the
backend has finished with them. Startup composition owns its logging lifetime
when using the supplied startup path.

## Execution

Writers may enqueue from supported producer threads. Sink callbacks run according
to synchronous/asynchronous configuration, not necessarily on simulation.
Compile/filter checks should precede costly argument construction where the
selected frontend supports that. Named logger registry state is separate from
game/thread ownership.

## Integration

The application can emit through a built-in named category after startup:

```cpp
CE::Logging::Logger<CE::Logging::Categories::engine>::info("Game initialized");
```

Select the declared category spelling from the
[logging umbrella](../../../projects/engine/include/cheryl/core/logging.h) and
configure it before producers. For a known file/console outcome, follow
[Enable useful output](../runtime/logging.md#enable-useful-output) before this
first write. If it is absent, use the
[missing-records checklist](../runtime/logging.md#investigate-missing-or-delayed-records).
The detailed guide also covers custom names, handlers, profiles and controlled shutdown.

## Limits and further reading

Logging does not promise recovery from a native failure or durable completion
merely because a write returned. Read
[configuration, emission and filtering](../runtime/logging.md),
[failure reporting](../runtime/failure-reporting.md),
[logging acceptance](../development/logging-acceptance.md) and
[the logger interface](../../../projects/engine/include/cheryl/core/logging/logger.h).
