# Worker pools and groups

WorkerPool owns physical CPU threads. WorkerGroup is a copyable workload handle
sharing that capacity; a group does not create another set of threads. Separate
physical pools are still independently constructible. The default pool has one
worker so an EngineContext need not reserve machine-wide capacity accidentally.

Jobs own their callables and return futures. Callback exceptions reach those
futures, without stopping another job. FIFO selection within a group does not
promise completion order when its concurrency is greater than one. A group's
`max_concurrency` enforces its running-job limit; zero uses the pool's capacity.
Eligible groups receive smooth weighted fair selection. Weight (1..1024) times
priority+1 (priority 0..7) determines job-selection share, not OS thread priority.
A concurrency cap or empty queue removes a group from that selection round.
Preferred previous-worker reuse breaks otherwise equal choices.

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


## CPU policy and capabilities

WorkerGroup owns its CPU/locality policy. The neutral root scheduler enforces the
policy before each job, so groups are more than labels on a shared queue. Explicit
CPU IDs must fit the inherited eligible CPU set; required unavailable CPUs reject.
Preferred eligibility may fall back, and `policy()` describes the requested and
effective set. `capabilities()` exposes unsupported affinity/topology facilities.
Hard cache-domain/NUMA requests reject until a native topology adapter exists;
manual CPU sets can already express a game's known locality domains. CPU affinity
does not promise that data remains in L1/L2, and NUMA memory placement is separate.

The Linux adapter queries inherited pthread eligibility, sets and verifies the
actual mask, and reports errors through job futures. The native mask is limited
to CPU_SETSIZE; query failure disables affinity capability explicitly. Other
platforms expose unsupported affinity rather than pretending to honor a hard
request. Native OS/real-time priority is separate from scheduler priority.

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

## Context ownership

EngineContext accepts ExecutionOptions: owned worker_count (default one) or an
application-supplied shared_pool. Its owned root is created only when
make_worker_group() is first called. Groups obtained there belong to that
context's shutdown domain. Shutdown closes/drains those groups and joins an owned
root; unrelated groups on an injected pool remain open. A saved context group
rejects after context shutdown, while separately owned physical pools remain
independent. Do not destroy a context from one of its jobs. Runtime shutdown pumps platform dispatch while accepted work finishes,
then joins an owned root before game/resource cleanup.

The normal GLFW/OpenGL factory forwards `GlfwOpenGLConfig::execution` to the
context. Set `config.execution.worker_count` or `config.execution.shared_pool`
there before creating the backend graph; owned and borrowed input use the same
execution configuration.

## Executed acceptance

Checkpoint 109 executes two further Linux cases through the production adapter.
The caller temporarily narrows its own nine-CPU inherited mask to CPU 0; a newly
created pool discovers that single eligible CPU. Required outside eligibility
rejects, preferred intersection/fallback uses the inherited CPU, and real worker
mask/CPU observations match. Every child joins before the caller mask restores,
with restoration guarded on exceptional test exits.

An absent CPU ID inside CPU_SETSIZE reaches pthread_setaffinity_np with a valid
nonempty mask. The actual kernel Invalid argument error reaches the accepted job's
future; the next required job on that worker sees its inherited mask. This callback
failure does not increment pre-callback policy failure accounting. Existing
controlled adapter cases separately execute policy suppression/readback/fallback
and startup rollback. All worker regressions execute in the complete 349 normal /
335 sandbox suites without skips. 110 closes bounded 9.5f/9.5 acceptance; see
[ARCHITECTURE-VALIDATION.md](ARCHITECTURE-VALIDATION.md).

## Policy example and coverage history

An application can choose a share and cap independently of CPU eligibility:

```cpp
CE::Engine::WorkerGroupOptions preparation;
preparation.max_concurrency = 2;
preparation.weight = 3;
preparation.cpu.cpus = known_eligible_cpus;
preparation.cpu.strength = CE::Engine::WorkerPolicyStrength::Required;
auto group = context.make_worker_group(preparation);
auto result = group.submit([] { return prepare_owned_asset_data(); });
```

The CPU list must come from the pool's advertised eligible set; it does not reserve
those cores. Quiesce producers before context shutdown. Runtime pumps accepted
platform dependencies while this work finishes, then closes platform requests and
recycles retained frames before application/resource cleanup. Outside runtime,
the owner must service any such dependencies before waiting for group drainage.

The following checkpoint notes retain their source-preparation scope. Current
executed acceptance is recorded above.

The aggregate regression sources prepare held-job cap isolation, weighted share,
saved-handle rejection, same-pool wait rejection, and pinned/inherited/pinned mask
restoration. Checkpoint 50 adds a required-mask revalidation scenario: a job narrows
its own mask behind the adapter's cache, and the next job on that same group must
restore its effective set. Linux mask scenarios explicitly skip unavailable
capabilities. None has been executed in this continuation. Native affinity failure
and partial thread-start rollback still need controlled failure execution; the
current success-path fixtures do not establish either outcome.

Checkpoint 52 prepares capture-deleter reentry into another group before source
drainage completes, and a Linux overlapping-mask scenario on one held worker.
The latter prequeues 3:1 workloads, checks each job's actual mask, and compares
the first eight selections. It tests mask switching together with weighted
scheduling; it does not exercise live OS restriction or native application failure.
These two additional scenarios are also uncompiled and unexecuted.

Checkpoint 62 prepares controlled native-boundary failures through a private,
per-pool adapter. Production still uses pthread get/set and std::thread; the
adapter centralizes the same set-then-readback check, without global overrides or
a new public construction API. The source-only test factory uses the production
scheduler, promise settlement, capture release and constructor rollback paths.

Eight new scenarios cover discovery query failure, effective-mask mismatch,
required set failure with capture-deleter reentry, verified preferred fallback,
failed fallback set/readback with recovery, post-set query failure, cached-mask
query failure and partial thread-start failure. The startup fixture enters one
real thread wrapper before rejecting the second start, then expects closure,
join, original-error preservation and adapter-capture release before return.
Affinity values/errors are synthetic and do not change the test host's CPU policy.
These scenarios have not been compiled or executed; they prepare controlled
failure coverage without proving actual OS rejection or runtime acceptance.
