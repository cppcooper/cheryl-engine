# Worker cache-locality implementation plan

This plan owns the selected cache-topology and worker-placement work, called
NUCA/cache locality in the [roadmap](../develop-review-and-development-plan.md#numa-and-nuca).
The design decisions below are settled; source implementation has not begun.
The source baseline inspected for this handoff is `66ca603`. Existing behavior is
documented in the [worker guide](../../runtime/worker-execution.md).

## Resume here

Read [AGENTS.md](../../../AGENTS.md), this plan and the relevant source before
modifying code. The next engineering phase is implementation when the project
owner requests it. This documentation handoff does not authorize builds, tests,
benchmarks, dependency installation or automatic progression into another mode.
Keep test design, test implementation, execution and documentation maintenance
separate under the repository's progression rules. Batch questions at the end of
the work turn; resolve routine implementation choices within the settled design.

Inspect the shared working tree first. At this handoff, the only pre-existing
change is a move of `assets/graphics/definitions/misc/tileset.dat` to
`assets/graphics/definitions/tilesets/tileset.dat` inside the assets submodule.
Preserve it and do not include assets or its gitlink in this work's commits.
Recheck rather than assuming that state remains unchanged in the next session.

## Selected scope

Implement general, best-effort cache-aware worker placement. Discover which
logical CPUs belong to each physical core and which CPUs share each reported
cache, then use that information to place related or independent jobs sensibly.
Support all reported cache levels and types, including L1 data/instruction,
L2 and L3; do not restrict the facility to shared L3 caches.

The application identifies related work through worker groups and owns its data
and working-set layout. The Engine cannot infer those relationships from arbitrary
callable captures. It does not inspect live cache contents, guarantee that data
remains cached, or select internal cache banks. Discovery supplies structure,
not a performance guarantee; [hwloc's structural-model explanation](https://www.open-mpi.org/projects/hwloc/doc/v2.14.0/faq.html)
distinguishes topology from measured latency and throughput.

The project owner selected this general facility without requiring an advance
speedup demonstration on the current machine. The earlier local-benefit gate in
`66ca603` is superseded. Measurements remain necessary before claiming a speedup.
NUMA discovery as an application facility, memory allocation/binding, first-touch
and migration remain [indefinitely deferred](../long-term/README.md#other-engine-extensions).
Do not enable memory placement as a side effect of CPU binding.

## Existing implementation and contracts

| Source | What to preserve or extend |
| --- | --- |
| [worker-pool.h](../../../projects/engine/include/cheryl/core/engine/worker-pool.h) | Public pool/group options, futures, capabilities and owned policy snapshots. The existing constructor defaults to one worker. |
| [worker-pool.cpp](../../../projects/engine/src/core/engine/worker-pool.cpp) | Weighted fair group selection, concurrency caps, FIFO selection, closure/drainage, thread-start cleanup and native-policy failures delivered through futures. |
| [worker-affinity.cpp](../../../projects/engine/src/core/engine/worker-affinity.cpp) | Existing Linux inherited eligibility, set/read-back verification and preferred fallback. Its fixed `CPU_SETSIZE` limit belongs to this existing adapter. |
| [worker-pool-internal.h](../../../projects/engine/src/core/engine/worker-pool-internal.h) | Per-pool native-operation injection for controlled fixtures; no global overrides. |
| [engine-context.h](../../../projects/engine/include/cheryl/core/engine/engine-context.h) | Existing `ExecutionOptions::shared_pool` composition and context-owned group shutdown. |
| [Engine CMake](../../../projects/engine/CMakeLists.txt), [options](../../../cmake/CherylOptions.cmake), [target metadata](../../../cmake/CherylTargets.cmake), [linkage](../../../cmake/CherylLinkage.cmake) and [outputs](../../../cmake/CherylOutputs.cmake) | One Engine library, explicit support selection and centralized build metadata. |
| [worker regressions](../../../projects/engine/tests/all-tests/src/core/worker-pool.cpp) | Existing behavioral coverage and controlled native adapter patterns; new tests need their own authorized phase. |

Current previous-worker reuse only breaks scheduler ties. A worker thread can
migrate within its eligible CPU mask, so reusing its thread index alone does not
establish physical-core/cache reuse. Required cache-domain/NUMA requests currently
reject; preferred requests report fallback. Automatic discovery remains absent.

Preserve ordinary submission and completion semantics. Accepted work must settle,
captures must be released before drainage completes, and user callbacks/destructors
must stay outside scheduler locks. Retain required-policy verification and existing
failure safety: a job does not run after binding and its verified fallback both
fail. Do not opportunistically move/refactor the existing Linux adapter or remove
the low-level interfaces because their callers are few.

## Ownership and composition

The selected implementation belongs under **`projects/engine/support/`**, rather
than a new platform module. Use a cohesive `support/worker-topology/` owner for the
optional provider and configuration helpers. Concrete target/header names are
implementation details; that directory and those targets do not exist yet.

Keep neutral topology descriptions, C++ options, the backend attachment contract
and generic scheduling in `Cheryl::Engine`. A separately linked support target
owns hwloc discovery and thread binding. A JSON-loader support target supplies
Engine-owned configuration loading without requiring hwloc. Keep both helpers
under the same owner unless a concrete dependency or testing need justifies a
different directory layout.

Dependency direction is application → selected support → Engine. Engine must not
link back to the support targets, and plain Engine consumers must acquire neither
hwloc nor the JSON helper. Keep SDK types and JSON implementation types out of
neutral/public C++ options. hwloc remains a private SDK dependency, while its
necessary final link requirements still propagate to consumers of a static
support library.

The application explicitly creates/supplies a provider instance to a new pool
options/constructor path. The pool owns or shares its provider until all workers
join. There is no global registrar, hidden discovery or entry-point hook.
Without that provider, ordinary workers and existing explicit affinity remain
available; optional cache preferences report fallback and unavailable hard
requirements reject.

Compose an application-owned topology-enabled pool through the existing
`ExecutionOptions::shared_pool`. EngineContext closes/drains its own groups without
closing unrelated groups on the injected root. The application still owns root
closure and lifetime. No new EngineContext global or reverse dependency is needed.

## Topology and placement contract

Retain an owned snapshot for each pool: eligible OS CPU IDs, physical-core and
SMT relationships, and reported cache domains with level, type, capacity, line
size and member CPUs. Domain identifiers are scoped to that snapshot. Intersect
the OS-allowed set with the creating thread's inherited affinity; do not expand
the application's eligibility when creating or restoring workers.

Discovery is performed when constructing the provider/pool, not for every job.
Copy advertised values and keep native SDK state alive for binding operations.
Record missing information explicitly. Capability snapshots are not live promises
about later OS restrictions; binding read-back supplies the execution-time check.

Do not infer locality from consecutive CPU IDs or assume equal cache sizes,
identical SMT widths, homogeneous cores or one processor package. The hwloc path
must use its own CPU-set representation without inheriting the old Linux
adapter's `CPU_SETSIZE` limit. This matters for large server configurations.

Query thread-specific binding support. Bind the executing worker thread, not the
whole process, and verify the effective mask. Use operations that do not alter
memory binding; hwloc exposes `HWLOC_CPUBIND_NOMEMBIND` for this purpose and may
report reduced binding support when it is required. See the
[CPU-binding contract](https://www.open-mpi.org/projects/hwloc/doc/v2.14.0/group__hwlocality__cpubinding.html).
Explicitly retain instruction-cache objects if advertising their inventory;
hwloc filters them out by default. Avoid redundant native binding calls when a
verified mask remains suitable, preserving required-policy revalidation.

## Configuration and caller behavior

Provide **both typed C++ options and a versioned JSON schema/loader**. Both paths
produce the same validated configuration. The application chooses when to load a
file or construct options; those settings apply when pools/groups are created.
Loading a file does not imply live resizing or mutation of existing groups.

Expose workload-oriented choices that require no knowledge of CPU/cache IDs:

| Policy | Intended behavior |
| --- | --- |
| Automatic | Choose a reasonable placement from eligible topology and concurrency, preferring physical cores before SMT siblings where possible. |
| Reuse | Prefer an actual CPU/core recently used by the related group, increasing the opportunity to reuse private caches. Keep the preference soft so locality alone does not stall available work. |
| Spread | Distribute independent concurrent work across physical cores before sharing SMT siblings where possible. |

Keep fairness and concurrency limits authoritative. Report requested/effective
policy, selected eligibility, missing capabilities and fallback reasons. Advanced
callers may use advertised cache domains or explicit CPU sets. A CPU mask is
eligibility, not exclusive ownership of a core.

Keep the existing numeric constructor and its one-worker default. Automatic pool
sizing is explicit through new options/configuration, using eligible physical
cores when known and a documented fallback when they are not. Group automatic
concurrency resolves to pool capacity; it does not create another physical pool.

The following is a proposed first-version shape, not an implemented format or API:

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

Resolve exact field spelling and schema placement during implementation. Validate
versions, names, types, numeric ranges and contradictory hard requirements.
Use readable errors that identify the field/file. Keep JSON and C++ validation
consistent; loading settings must not silently create groups, execute jobs or
change process-wide affinity.

## Build platforms and dependency selection

CPU vendor is not a build selection: AMD and Intel consumer/server CPUs are in
scope, and the provider follows reported capabilities on other architectures.
[hwloc's portability reference](https://www.open-mpi.org/projects/hwloc/#portability-and-support)
includes x86, ARM, RISC-V and POWER, with discovery largely dependent on OS
information. That SDK breadth does not establish Cheryl acceptance on every
architecture or operating system. Linux is the first native acceptance platform;
other platforms keep honest capability/fallback reporting.

Gate hwloc selection with an explicit CMake option, initially off. A disabled
selection must not discover, download or link hwloc. `EXCLUDE_FROM_ALL` alone
does not meet that requirement because the directory still configures. When
selected, reuse a compatible supplied target or discover an installed target SDK;
report a missing dependency clearly. Do not add implicit network fetching or
package installation. JSON loading must remain usable with hwloc omitted.

Discover executing-machine topology at runtime. Do not execute `lstopo` during
configuration, bake the build host's CPU/cache layout into artifacts, or add
`-march=native`. Cross builds must resolve headers/libraries for the target through
the toolchain, not accidentally use host hwloc libraries; see
[CMake's cross-compilation guidance](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling).

The current consumer machine exposes eight physical cores/sixteen logical CPUs,
per-core 32 KiB L1 data and instruction caches, per-core 1 MiB L2, and one shared
16 MiB L3. hwloc is already installed locally; no package change was made. This
is a development fixture, not a required layout or evidence of performance gains.
Use only CPUs permitted to the process/thread, including on containers and CI.

## Ordered development and boundaries

Keep one macro-task checklist here; the roadmap links to it.

- [x] Settle general best-effort scope, NUMA deferral and application responsibility.
- [x] Select typed C++ plus JSON configuration and optional Engine support ownership.
- [ ] Add neutral owned topology/provider contracts and new pool options, preserving
  existing constructors and the injected native-fixture path.
- [ ] Add optional hwloc discovery/binding support, explicit CMake selection and
  an independently linkable consumer boundary. Establish ownership, eligibility,
  partial-capability and startup-failure behavior before policy code depends on it.
- [ ] Implement automatic/reuse/spread CPU targeting, preserving fairness,
  concurrency limits, futures, closure/drainage and required/preferred semantics.
- [ ] Add the JSON schema/loader and shared validation; retain direct C++ use and
  JSON loading without hwloc.
- [ ] Provide a small application composition example using `shared_pool` and
  named groups, without expanding unrelated demo or UI behavior.
- [ ] Complete separately authorized test design, test implementation, execution
  and verification; reconcile subject guides and testing requests during authorized
  documentation maintenance.

Commit coherent implementation units independently. Stop if implementation
requires a changed architectural contract or depends on unfinished user work;
ordinary naming and internal algorithm choices do not need new approvals.
Render batching follows this feature and still needs its own design discussion.
[Build/release automation](../develop-review-and-development-plan.md#build-and-release-automation)
is the intended follow-up after batching.

## Acceptance to preserve for later phases

These are coverage requirements for future test work, not ready commands or new
entries in the testing queue. No executable acceptance is claimed by this plan.

- Controlled topology fixtures cover private/shared caches, SMT, sparse CPU IDs,
  multiple packages, asymmetric layouts and missing cache/core information.
- Behavioral checks cover actual core targeting and reuse/spread decisions,
  fairness/caps/progress, eligibility intersection, required rejection, preferred
  fallback, binding/read-back failures and provider lifetime through worker joins.
- JSON and C++ produce equivalent effective configuration; invalid/unknown versions,
  fields, ranges and conflicting requirements fail clearly without partial startup.
- Engine and JSON-only consumers configure/link with hwloc disabled or absent.
  A selected support consumer proves dependency propagation and neutral headers.
- Native Linux checks verify allowed CPUs and actual thread masks without changing
  memory binding. Missing binding support must be reported; skipped native checks
  do not establish that behavior. Other platforms remain unaccepted until tested.
- Representative reuse and independent parallel workloads compare policy behavior
  and overhead. Correct discovery/binding is distinct from a measured speedup;
  no hardware-specific gain is required to make the general facility usable.
