# Worker pools and groups

`CE::Workers::Pool` owns physical CPU threads. `CE::Workers::Group` is a copyable
workload handle sharing that capacity; a group does not create another set of
threads. Pools, groups, CPU policies and topology form the reusable `CE::Workers`
facility. Applications can use it without a game session or runtime context. The
default pool has one worker so a `CE::Runtime::Context` need not reserve
machine-wide capacity accidentally.

## Jobs

Jobs own their callables and return futures. Callback exceptions reach those
futures, without stopping another job. FIFO selection within a group does not
promise completion order when its concurrency is greater than one. A group's
`max_concurrency` enforces its running-job limit; zero uses the pool's capacity.
Eligible groups receive smooth weighted fair selection. Weight (1..1024) times
priority+1 (priority 0..7) determines job-selection share, not OS thread priority.
A concurrency cap or empty queue removes a group from that selection round.
Preferred previous-worker reuse breaks otherwise equal choices.

## Pool lifetime

`group.close()` stops acceptance without cancelling ordinary accepted work.
`group.drain()` requires closure, waits for pending/running work and capture
release, and leaves other groups available. `pool.close()` closes all groups;
`shutdown()` also joins. Destruction drains and joins. Saved group handles reject
safely after the pool disappears. Destroying the pool from one of its own jobs is
invalid; explicit self-join and same-pool drain reject before blocking.

Do not block a worker on child futures from the same saturated pool. Dependency
scheduling/work stealing is separate work. No worker is forcibly terminated.
Thread-start failure wakes and joins every thread already created. Scheduler
locks protect queue/accounting changes; user work and its captured destructors
run outside them. Submitted jobs retain their group even if its handle is dropped.


## Topology providers and capacity

Neutral topology descriptions, pool/group options and scheduling live in
`CE::Workers` and are supplied by `Cheryl::Engine`. The optional
[`Cheryl::Workers` module](../modules/workers.md) supplies
`CE::Workers::Hwloc::make_topology_provider()` and independent JSON loading.
Both features live under `projects/modules/runtime/workers/` in one implementation
library; their existing `Cheryl::WorkerTopology::Hwloc`/`Json` names are
capability-dependent compatibility aliases. Engine has no reverse dependency.
Public contracts expose neither hwloc nor JSON types. The shared module owns
its static hwloc implementation; static SDKs supply its final archive closure.

Supply a `CE::Workers::TopologyProvider` through `CE::Workers::PoolOptions::topology_provider`.
The pool retains it through worker joins. There is no global registrar, entry-point
hook or implicit discovery. The existing numeric constructor still defaults to one
worker and requires a positive count. `CE::Workers::PoolOptions::worker_count = 0` explicitly
selects automatic capacity: eligible physical cores when all eligible CPUs have core
information, eligible logical CPUs otherwise, or one worker when eligibility is unknown.
Group concurrency zero uses pool capacity; a larger requested cap is limited to that
capacity and creates no extra threads.

```cpp
#include <cheryl/core/engine/worker-pool.h>
#include <cheryl/core/engine/worker-topology/hwloc.h>
#include <memory>
#include <utility>

CE::Workers::PoolOptions options;
options.worker_count = 0;
options.topology_provider = CE::Workers::Hwloc::make_topology_provider();
auto pool = std::make_shared<CE::Workers::Pool>(std::move(options));
CE::Workers::GroupOptions simulation_options;
simulation_options.max_concurrency = 1;
simulation_options.cpu.locality = CE::Workers::LocalityPolicy::Reuse;
auto simulation = pool->make_group(simulation_options);
```

`topology()` returns an owned snapshot of eligible OS CPU IDs, physical-core/SMT
relationships and reported cache domains. Each cache has a snapshot-scoped ID,
level, data/instruction/unified type, member CPUs and optional capacity/line size.
Private and shared caches at all reported levels are represented; missing values
are omitted. CPU IDs need not be consecutive, and core widths/cache sizes need not
be uniform. No policy assumes a particular CPU vendor or one processor package.

The provider discovers the executing machine at runtime. The pool intersects its
advertised CPUs with the pool creator's inherited thread mask, validates IDs and
memberships, and retains that restriction for later binding/restoration. Discovery
does not run per job, change creator affinity or place memory. Invalid structural
snapshots reject before worker startup. Missing runtime capabilities remain explicit
in `capabilities()` and `limitations`.

## CPU policy and capabilities

`CE::Workers::Group` owns its CPU/locality policy. The neutral root scheduler enforces the
policy before each job, so groups are more than labels on a shared queue. Explicit
CPU IDs must fit the inherited eligible CPU set; required unavailable CPUs reject.
Preferred eligibility may fall back, and `policy()` describes the requested and
effective set. `capabilities()` exposes unsupported affinity/topology facilities.
Capabilities are a startup snapshot, not a live OS-eligibility query; each job's
native policy application supplies its execution-time check. `policy()` and
`status()` return owned values. Status counters observe progress without locking
application state or establishing completion; closed-group drainage is the barrier.
`shared_cache_domain` selects a domain from this pool's snapshot and intersects its
members with the group's CPU eligibility. Missing/conflicting required domains
reject; preferred requests report fallback. Required explicit locality also needs
thread binding and physical-core information for every selected CPU. Without a
provider, existing explicit affinity remains available, preferred locality falls
back and unavailable hard requirements reject. An unspecified locality policy uses
Automatic with a provider and ordinary affinity without one.

| Locality | CPU targeting |
| --- | --- |
| Automatic | Reuses for effective concurrency one; otherwise spreads across eligible cores/CPUs. |
| Reuse | Prefers the group's last verified CPU, then its physical core, subject to current load. |
| Spread | Rotates equal candidates and favors idle physical cores before SMT siblings. |

Busy-core/CPU load takes precedence over soft reuse. Fair selection and concurrency
limits remain authoritative, so locality alone does not stall available work.
`policy()` reports effective locality, cache domain, concurrency, eligibility and
fallback; `status().last_cpu` is the last verified singleton mask at job entry,
not a live residency observation. A CPU mask is eligibility, not exclusive core
ownership. Applications identify related jobs and own their working sets; the
facility cannot infer that relationship from callable captures, inspect cache
contents or select internal cache banks. Discovery/binding do not establish a speedup.
Representative workload measurements are required before making performance claims.

NUMA discovery as an application facility, memory allocation/binding, first-touch
and migration remain [indefinitely deferred](../planning/long-term/README.md#other-engine-extensions).
`numa_placement` remains false; required NUMA requests reject and preferred requests
report fallback. CPU binding must not enable memory placement as a side effect.

Without a provider, the Linux adapter queries pthread eligibility, sets and verifies
the actual mask, and reports errors through job futures. That adapter retains its
`CPU_SETSIZE` limit; platforms without it report unsupported affinity. The hwloc
provider uses hwloc CPU sets without that fixed limit, retains instruction-cache
objects, queries thread-specific binding and uses strict calling-thread binding with
`HWLOC_CPUBIND_NOMEMBIND`. Missing support or failed verification is reported rather
than pretending to honor a hard request. Native OS/real-time priority remains
separate from scheduler priority.

On Windows, the hwloc provider retains discovered CPUs, cores and caches but reports
`cpu_affinity = false`: its backend rejects `HWLOC_CPUBIND_NOMEMBIND`. Preferred
CPU/cache locality uses portable scheduling; required explicit CPU, cache-domain or
locality requests reject. Windows binding that preserves memory placement is a
[phase 4 deferral](../planning/long-term/README.md#windows-worker-affinity).

Workers retain an unchanged verified mask between compatible jobs. Before another
group's work they apply its eligibility, restoring inherited eligibility for an
unconstrained group. A required job checks that cached eligibility remains valid.
Failed policy application never runs a job under an unverified mask; preferred
policy attempts an inherited-mask fallback and records `policy_failures`. Live OS
restrictions can still reject even a previously valid group.

For two groups restricted to overlapping CPUs, both use the same physical workers;
the scheduler chooses an eligible uncapped group by fair share, then verifies its
mask on that worker before execution. This is CPU eligibility, not reserved core
ownership. Different masks can require migration; stable reuse is a preference.

Native references: [pthread affinity](https://man7.org/linux/man-pages/man3/pthread_setaffinity_np.3.html)
and [Linux cpuset constraints](https://docs.kernel.org/admin-guide/cgroup-v2.html#cpuset).
The kernel can restrict an otherwise successful set operation, which is why the
adapter reads back the effective mask.

## Typed and JSON configuration

`CE::Workers::Configuration` owns pool options and a map of named group options.
`CE::Workers::validate_group_options()` and `CE::Workers::validate_configuration()` perform
structural checks without discovering topology, creating threads or binding CPUs.
Runtime availability and CPU/cache intersections are resolved by pool/group
construction. Named groups match `[a-z][a-z0-9_]*`.

Enable the JSON feature, link `Cheryl::Workers` and include
`<cheryl/core/engine/worker-topology/configuration.h>` for
`CE::Workers::Json::parse_configuration(document, source)` or `CE::Workers::Json::load_configuration(path)`;
the latter also accepts a stream and source path. The supported version is `"1.0"`:

```json
{
  "version": "1.0",
  "workers": "auto",
  "groups": {
    "simulation": {"locality": "reuse", "concurrency": 1},
    "asset_preparation": {"locality": "spread", "concurrency": "auto"}
  }
}
```

Omitted workers default to one; omitted group concurrency defaults to pool capacity.
`workers`/`concurrency` accept `"auto"` or a positive integer within `size_t` range,
not numeric zero. Groups also accept weight 1..1024, priority 0..7 and a `cpu` object
with distinct nonnegative OS `cpus`, `strength` (`preferred`/`required`),
`cache_domain` and `prefer_same_worker`. Select domain IDs from the pool's
advertised snapshot; do not assume they are portable between snapshots or machines.
NUMA fields are not in this format.
The [schema](../../projects/modules/runtime/workers/schema/worker-configuration-v1.schema.json)
and [example](../../projects/modules/runtime/workers/examples/workers.json)
define the exact shape.

Unknown versions/fields, duplicate JSON properties, invalid names/types/ranges and
invalid structural options fail with a source/field diagnostic. C++ and JSON use
the same structural validation. Loading is inert: callers attach a provider, create
the pool and select groups themselves. It neither silently starts work nor changes
existing pools/groups. It supplies no live resize/reconfiguration or process-wide
affinity change. JSON loading works with hwloc omitted.

## Context ownership

`CE::Runtime::Context` accepts `CE::Runtime::ExecutionOptions`: owned `worker_count`
(default one) or an application-supplied `shared_pool`. Its owned root is created
only when `make_worker_group()` is first called. Groups obtained there belong to that
context's shutdown domain. Shutdown closes/drains those groups and joins an owned
root; unrelated groups on an injected pool remain open. A saved context group
rejects after context shutdown, while separately owned physical pools remain
independent. Do not destroy a context from one of its jobs. The runtime's
[shutdown sequence](runtime-frame-boundary.md#shutdown) settles accepted
dependencies before game/resource cleanup.

The normal GLFW/OpenGL factory forwards `CE::Application::GLFWOpenGL::Options::execution` to the
context. Set `config.execution.worker_count` or `config.execution.shared_pool`
there before creating the backend graph; owned and borrowed input use the same
execution configuration.

The [shared-pool example](../../projects/modules/runtime/workers/examples/shared-pool.cpp)
composes loaded or direct settings with an application-supplied backend factory and
named context-owned groups. It is a separately buildable example, not a global hook.

## Policy example

An application can choose a share and cap independently of CPU eligibility:

```cpp
CE::Workers::GroupOptions preparation;
preparation.max_concurrency = 2;
preparation.weight = 3;
preparation.cpu.cpus = known_eligible_cpus;
preparation.cpu.strength = CE::Workers::PolicyStrength::Required;
auto group = context.make_worker_group(preparation);
auto result = group.submit([] { return prepare_owned_asset_data(); });
```

The CPU list must come from the pool's advertised eligible set; it does not reserve
those cores. Outside runtime, the owner must quiesce producers and service accepted
platform dependencies before waiting for group drainage.

Worker regression coverage covers caps, weighted shares, capture release before
drain completion, saved-handle rejection, mask switching/revalidation, actual
inherited restrictions and kernel rejection, plus controlled affinity and
thread-start failures. See [architecture-validation.md](../development/architecture-validation.md)
for procedures and host limits.
