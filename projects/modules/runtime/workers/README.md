# Workers module

Workers supplies optional configuration loading and machine-topology integration
for Engine's CPU pools. It is one selectable compiled capability with two
independent features; it does not move core pool scheduling/execution out of
Engine or create threads merely because configuration was loaded.

## Selection and dependency

Select `CHERYL_BUILD_WORKERS=ON` and link `Cheryl::Workers`. At least one feature
must be enabled:

| Selection | Capability | Public header |
| --- | --- | --- |
| `CHERYL_BUILD_WORKER_TOPOLOGY_HWLOC` | Native CPU/core/cache topology and affinity provider. | `<cheryl/core/engine/worker-topology/hwloc.h>` |
| `CHERYL_BUILD_WORKER_TOPOLOGY_JSON` | Versioned worker/group configuration loading. | `<cheryl/core/engine/worker-topology/configuration.h>` |

Both are enabled by default. JSON loading works without hwloc. To omit the
module entirely use `CHERYL_BUILD_WORKERS=OFF`; Engine pools/groups remain
available. The module umbrella is `<cheryl/modules/workers.h>`.

```cmake
target_link_libraries(application PRIVATE Cheryl::Workers)
```

The module depends publicly on Engine. hwloc and its native adaptation belong to
this module's private implementation; JSON parsing exposes no upstream JSON
types. hwloc uses a shared library on Linux and Windows, including with static
Cheryl; ordinary development and SDK builds use the same dependency policy.
Disabling its feature omits hwloc discovery and linkage entirely. The JSON-only
module keeps JSON parsing inside its own implementation.

## Load configuration without starting execution

```cpp
#include <cheryl/core/engine/worker-topology/configuration.h>

auto configuration = CE::Workers::Json::load_configuration("workers.json");
```

The returned `CE::Workers::Configuration` owns pool options and named group
settings. Loading validates structure but performs no machine discovery, thread
creation, CPU binding or live pool reconfiguration. The
[version 1.0 schema](./schema/worker-configuration-v1.schema.json) and
[example](./examples/workers.json) define accepted fields. Unknown/duplicate fields,
invalid names/types/ranges and unsupported versions reject with source/field
context.

## Compose discovered topology with a pool

With hwloc enabled:

```cpp
#include <cheryl/core/engine/worker-pool.h>
#include <cheryl/core/engine/worker-topology/hwloc.h>
#include <utility>

CE::Workers::PoolOptions options;
options.worker_count = 0;
options.topology_provider = CE::Workers::Hwloc::make_topology_provider();
CE::Workers::Pool pool(std::move(options));
auto preparation = pool.make_group({.max_concurrency = 2});
```

Zero explicitly requests automatic capacity in `PoolOptions`; the ordinary
numeric pool constructor still requires a positive count. The pool retains its
provider through worker joins. A topology snapshot describes eligible OS CPUs,
cores and reported caches, not a portable set of persistent CPU/domain IDs.
Discovery does not change the creator's affinity or place memory.

Applications can attach the provider to loaded options before pool construction.
The [shared-pool example](./examples/shared-pool.cpp) composes named configuration
with a supplied pool and runtime context; it is an example target rather than a
production module or global registrar.

## Ownership, execution and limits

Core `Pool`, `Group`, options and scheduling remain `Cheryl::Engine` contracts.
Groups share physical capacity, own submission policy and return futures.
Context-created groups belong to that runtime's shutdown domain; unrelated
groups in an injected shared pool remain open. Required unavailable CPU policy
rejects and preferred policy may report fallback. CPU masks describe eligibility,
not exclusive core reservation or proof of better performance.

Close acceptance before drainage and settle producer/dispatch dependencies
before destroying borrowed targets. Worker jobs do not gain graphics affinity;
use platform dispatch for resource upload. NUMA memory placement, automatic live
resizing and forced worker termination are not supplied by this module.

## Compatibility and further reading

`Cheryl::WorkerTopology::Hwloc` and `Cheryl::WorkerTopology::Json` remain
capability-dependent compatibility names for the same Workers implementation.
Their existing `CHERYL_HAS_SUPPORT_WORKER_TOPOLOGY_HWLOC` and
`CHERYL_HAS_SUPPORT_WORKER_TOPOLOGY_JSON` feature definitions remain available
only when the corresponding feature is selected. Do not infer two runtime
libraries from the two compatibility target names.

Read [worker execution](../../../../docs/runtime/worker-execution.md),
[Engine Systems: Workers](../../../../docs/engine-systems/workers.md),
[module authoring](../../../../docs/development/modules.md) and
[runtime shutdown](../../../../docs/runtime/runtime-frame-boundary.md#shutdown).
