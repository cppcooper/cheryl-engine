# Workers

## Purpose

Workers shares CPU execution capacity between workload groups. Groups define
admission, concurrency, relative scheduling share and optional CPU/locality
policy without creating another physical pool per workload.

## Interface

Types below are in `CE::Workers` unless another namespace is shown.

### Types and operations

| API and purpose |
| --- |
| {cpp:class}`Pool <CE::Workers::Pool>`, {cpp:struct}`PoolOptions <CE::Workers::PoolOptions>`<br>Own reusable CPU capacity, optionally informed by discovered topology. [Pool lifetime](../runtime/worker-execution.md#pool-lifetime); [capacity](../runtime/worker-execution.md#topology-providers-and-capacity). |
| {cpp:class}`Group <CE::Workers::Group>`, {cpp:struct}`GroupOptions <CE::Workers::GroupOptions>`<br>Submit owned jobs with concurrency caps, scheduling share and CPU policy; close then drain. [Jobs](../runtime/worker-execution.md#jobs); [CPU policy](../runtime/worker-execution.md#cpu-policy-and-capabilities). |
| {cpp:class}`TopologyProvider <CE::Workers::TopologyProvider>`, {cpp:struct}`Topology <CE::Workers::Topology>`<br>Supply/inspect native CPU, core and cache observations. [Topology providers](../runtime/worker-execution.md#topology-providers-and-capacity); [adapter catalogue](../architecture/interfaces-adapters.md#backend-capabilities-and-resource-implementations). |
| {cpp:struct}`Configuration <CE::Workers::Configuration>`<br>Validate typed pool/group configuration; optional Workers integration loads JSON. [Typed and JSON configuration](../runtime/worker-execution.md#typed-and-json-configuration); [Workers module](../modules/workers.md). |
| {cpp:struct}`GroupPolicy <CE::Workers::GroupPolicy>`, {cpp:struct}`GroupStatus <CE::Workers::GroupStatus>`, {cpp:struct}`Capabilities <CE::Workers::Capabilities>`<br>Compare requested/effective policy, activity and supported native behavior. [CPU policy](../runtime/worker-execution.md#cpu-policy-and-capabilities); [worker statistics](../runtime/subsystem-diagnostics.md#worker-statistics). |

### CPU policy enums

| Type | Choices and details |
| --- | --- |
| {cpp:struct}`CpuPolicy <CE::Workers::CpuPolicy>` | Explicit eligible CPU IDs, strength, locality and optional cache-domain selection. [CPU policy and capabilities](../runtime/worker-execution.md#cpu-policy-and-capabilities). |
| {cpp:enum}`PolicyStrength <CE::Workers::PolicyStrength>` | `Preferred` may report fallback; unsupported `Required` policy rejects. [Required and preferred policy](../runtime/worker-execution.md#cpu-policy-and-capabilities). |
| {cpp:enum}`LocalityPolicy <CE::Workers::LocalityPolicy>` | `Automatic`, `Reuse`, `Spread` within reported topology and binding support. [Locality selection](../runtime/worker-execution.md#cpu-policy-and-capabilities). |
| {cpp:enum}`CacheType <CE::Workers::CacheType>` | `Data`, `Instruction`, `Unified` classify discovered cache domains. [Topology snapshots](../runtime/worker-execution.md#topology-providers-and-capacity). |

## Usage

For a running game, create a tracked group through
`engine.make_worker_group(options)`. Submit CPU-only work with owned inputs and
retain its future. Runtime settles these groups during shutdown while accepted
platform dependencies can still progress.

```{engine-diagram} workers-focus-groups
```

[Explore application preparation](diagram-explorer.md#workers-application),
[a pool shared by contexts](diagram-explorer.md#workers-focus-shared), or
[worker implementation dependencies](diagram-explorer.md#workers-internal).

### Choose pool and group ownership

| Situation | Construction path | Who closes and drains it? |
| --- | --- | --- |
| One game's ordinary CPU preparation | Set {cpp:struct}`Runtime::ExecutionOptions <CE::Runtime::ExecutionOptions>`, then `Context::make_worker_group()` | Runtime/Context tracks every resulting group. Owned physical capacity is created lazily; the default is one worker. |
| Several contexts or services share physical capacity | Supply `ExecutionOptions::shared_pool`, then create each context's groups | Each context closes/drains its groups; the application owns the pool and unrelated groups. |
| A service runs independently of Runtime | Own {cpp:class}`Workers::Pool <CE::Workers::Pool>` and call `pool.make_group()` | The service stops producers, closes acceptance and services dependencies before drain/join. |
| CPU/cache policy needs native topology | Set {cpp:struct}`Workers::PoolOptions <CE::Workers::PoolOptions>` with a retained {cpp:class}`Workers::TopologyProvider <CE::Workers::TopologyProvider>` | The pool retains the provider through worker joins. The optional [Workers module](../modules/workers.md) supplies hwloc discovery. |

### Separate capacity from scheduling policy

| Setting | Effect | Decision to make |
| --- | --- | --- |
| Pool worker count | Physical CPU threads shared by all groups | Choose total capacity once. Context's owned count must be positive; standalone `PoolOptions` supports explicit automatic capacity with zero. |
| `GroupOptions::max_concurrency` | Running-job cap, limited to pool capacity; zero uses that capacity | Limit a workload without allocating extra threads. |
| `weight` and `priority` | Relative selection share among eligible groups | Balance queued work; neither setting guarantees latency, completion order or OS priority. |
| `GroupOptions::cpu` | Eligible CPUs and optional locality/cache policy | Choose Required rejection or Preferred fallback, then inspect effective policy/capabilities. |

A job's future carries its result or failure. A closed group's `drain()` also
waits for accepted jobs and capture release; status counters alone are not that
barrier. The [worker contract](../runtime/worker-execution.md#jobs) defines
scheduling and [policy limits](../runtime/worker-execution.md#cpu-policy-and-capabilities).

### Uses

| Dependency | Workers' use |
| --- | --- |
| {cpp:class}`Workers::TopologyProvider <CE::Workers::TopologyProvider>`, {cpp:struct}`Workers::Topology <CE::Workers::Topology>` | Optional startup discovery and supported CPU policy application; the neutral pool does not implicitly load a provider. |
| {cpp:struct}`Workers::CpuPolicy <CE::Workers::CpuPolicy>` | Validate requested eligibility/locality against pool capabilities and verify required native policy before callback entry. |
| [Diagnostics](diagnostics.md) and [Logging](logging.md) | Identify groups/pools and expose queue, callback and policy observations. |

Graphics follow-up belongs to the application's job workflow through
[Dispatch](dispatch.md). The pool itself supplies CPU execution and has no
owner-dispatch dependency.

### Used by

| Consumer | Relationship to the pool |
| --- | --- |
| {cpp:class}`Runtime::Context <CE::Runtime::Context>` | Create tracked groups on lazy owned or supplied shared capacity. [Context ownership](../runtime/worker-execution.md#context-ownership). |
| `Events::worker_delivery()` | Execute one serial listener stream over a group; separate adapters can run concurrently. [Event delivery](../runtime/event-delivery.md#worker-stream-acceptance-and-cancellation). |
| {cpp:class}`Text::Rendering::Updater <CE::Text::Rendering::Updater>` | Create a Context-tracked serial preparation group, submit owned text requests, then upload through platform dispatch. [Text updates](../getting-started/how-to/update-text.md). |
| Application [asset](assets.md) or [audio](audio.md) preparation | Schedule independent CPU operations and retain their results; these services do not automatically submit preparation jobs. |
| Standalone application service | Share physical capacity without constructing a game session. |

## Contracts

Jobs own callables and settle futures with results/errors. FIFO selection does
not imply completion order when concurrency exceeds one. Caps limit running
jobs; weight/priority determine selection share rather than OS scheduling
priority. CPU eligibility is not reserved core ownership or guaranteed speedup.

Do not block a job on child futures from the same saturated pool. Close before
drain; drainage waits for accepted work and capture release. Same-pool drainage
and self-join reject. Required unsupported native policy rejects while preferred
policy may report fallback.

## Lifecycle

Create a pool and groups, submit owned work, close acceptance and drain before
destroying borrowed dependencies. Pool destruction closes, drains and joins;
saved group handles reject safely after pool disappearance. Thread-start failure
joins threads already created. Context-created groups close with that context;
unrelated groups in an injected shared pool remain open.

## Execution

Physical workers execute eligible groups according to scheduling policy. They
are separate from runtime's dedicated concurrent simulation thread. Graphics
work still needs platform dispatch. During runtime shutdown, accepted CPU work
settles while platform dependencies can continue making progress.

## Integration

Before context creation, select the desired physical capacity through
[`ExecutionOptions`](../runtime/worker-execution.md#context-ownership); its default
is one worker. During game initialization, retain a context-owned preparation group:

```cpp
auto preparation = engine_.make_worker_group({.max_concurrency = 2});
auto result = preparation.submit([] { return 6 * 7; });
```

Real preparation should return owned CPU data. Retain/read its future and send
completed results to platform for upload; the group's cap cannot exceed physical
pool capacity and does not create two additional threads. To permit two jobs
to run at once, the configured pool must contain at least two workers.

## Limits and further reading

No work stealing, forced termination, live pool reconfiguration or NUMA memory
placement is implied. Topology discovery does not prove a performance benefit.
Read [worker contracts and optional integration](../runtime/worker-execution.md),
[runtime shutdown](../runtime/runtime-frame-boundary.md#shutdown) and
[pool/group declarations](../../../projects/engine/include/cheryl/core/engine/worker-pool.h).
