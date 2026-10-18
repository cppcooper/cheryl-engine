# Logging configuration and emission

Cheryl groups engine records into five explicitly named destinations. Their public
names are declared in
[log-names.h](../../../projects/engine/include/cheryl/core/logging/log-names.h), also included by
core/logging.h. Default paths are relative to the configured log directory:

| Name | Default file | Records |
| --- | --- | --- |
| `CE::Logging::Categories::engine` | logs/engine.log | Runtime lifecycle/timing, workers, dispatchers and events |
| `CE::Logging::Categories::platform` | logs/os-platform.log | OS display/window/input services, GLFW and context selection |
| `CE::Logging::Categories::rendering` | logs/rendering.log | Renderer/shader records and native graphics resource/debug observations |
| `CE::Logging::Categories::assets` | logs/assets.log | Asset preparation/upload, cache publication and reload |
| `CE::Logging::Categories::memory` | logs/memory.log | Allocator bookkeeping, preallocation and ownership errors |

Each file retains subsystem/operation/domain fields for finer correlation. Runtime
input capability/focus records go to os-platform, while session records stay in
engine. engine.log is the runtime/execution timeline; other categories contain
their own subsystem details, with no automatic aggregate mirror.

`CE::Logging::Log<name>` owns a facade over compiled Engine logging state;
`CE::Logging::Logger<name>` supplies lazy named access. Below, Log and Logger
refer to these types. Built-in and custom `Logger` categories share one Engine registry
keyed by textual name across libraries. `Logger::get_existing()` borrows the
central `LogHandle*` without constructing or waiting; null also covers a category
not yet published. Published central handles remain until Engine static teardown.
Every engine write selects its category explicitly. `make_default()` affects
Engine's private registry, not an application's independent native logger.
Required native/noexcept/logger failures use bounded stderr independently of any
default or file destination.

`CE::Logging::CherylLog`, a facade derived from
`CE::Logging::Logger<CE::Logging::Categories::cheryl>`, supplies the explicit legacy
cheryl destination. Engine-owned operations do not create cheryl.log; an
application that explicitly uses this destination creates it. Category values and
file destinations remain stable; changing public namespace names does not move,
merge, delete or rename existing files and backups. Normal configured rotation
and retention still apply.

## Process lifetime

The supplied `Startup` owns a `CE::Logging::ProcessLifetime` guard. Keep Startup
alive through its context/game cleanup, or keep an outer guard in `main()`.
For a standalone program or custom factory, construct that outer guard before
logging begins and retain it until all producers and diagnostic readers finish:

```cpp
CE::Logging::ProcessLifetime logging_lifetime;
// Configure categories, run application services, then settle their callers.
```

Nested or overlapping guards share one process lifetime. The last guard closes
live logs and joins Cheryl's logging worker. That shutdown is terminal: later
guards, new logs and reopening reject. Do not construct or destroy a guard from
backend/file callbacks or a DLL entry point. See the
[ProcessLifetime declaration](../../../projects/engine/include/cheryl/core/logging/process-lifetime.h).

## Enable useful output

Configure the category before its first use and before starting writers. For a
build whose logging policy retains DEBUG, this records platform diagnostics in
a chosen directory while leaving the console at WARN:

```cpp
#include <cheryl/core/logging.h>

using PlatformLog = CE::Logging::Logger<CE::Logging::Categories::platform>;
auto config = CE::Logging::Config::for_logger(CE::Logging::Categories::platform);
config.directory = "investigation";
config.logger_level = CE::Logging::Level::debug;
config.file_level = CE::Logging::Level::debug;
config.console_level = CE::Logging::Level::warn;
PlatformLog::initialize(CE::Logging::FileEventHandlers{}, config);
CE_LOG_DEBUG(CE::Logging::Categories::platform, "Platform diagnostics enabled");
```

After the session's producers settle, close each category on the logging owner;
for this category call `PlatformLog::close()`. Then inspect
`investigation/os-platform.log`: the DEBUG record belongs there and is absent from
the WARN console. Submission is asynchronous, so a write or queued flush is not
an immediate file-completion acknowledgement. Configure other categories explicitly
when they should use the same directory. A developer/support compile profile retains
DEBUG; changing runtime thresholds cannot restore it in a build that stripped it.

## Investigate missing or delayed records

| Symptom | Inspect | Next action |
| --- | --- | --- |
| DEBUG/TRACE never appears | Compiled mask, logger level and the relevant destination level | Retain the severity in one target-consistent build policy, then admit it at both runtime gates. |
| A file has records but the terminal does not | Console threshold and optional terminal attachment | Lower the console gate when needed; the terminal captures process output independently of file configuration. |
| Logs appear in an unexpected directory | `initial_configuration()` or `get_file_path()` on the category's handle | Configure that category before first use. Relative directories resolve once; changing the working directory later does not move the destination. |
| Accepted output is delayed or new records disappear | Shared queue statistics and the Log's backend degradation statistics | Identify slow/failed destinations. Block waits for capacity; DiscardNew can lose new records and flush requests. Stop producers before close/reopen recovery. |
| Close waits or times out | Active producers, retained backend work and callbacks/native I/O | Settle producers and dependencies before waiting for completion. A timeout leaves Closing intact; do not call lifecycle operations from a backend/file callback. |
| A required native/cleanup failure is absent from files | Process stderr and the original exception/future | Use the bounded emergency report and owning operation's failure. File thresholds or degradation do not suppress that independent reporting path. |

[Subsystem diagnostics](subsystem-diagnostics.md#investigate-a-runtime-symptom)
connects those records to runtime, resource and work-queue behavior. The remaining
sections define the exact filtering, queue, lifecycle and failure guarantees.

Configure each used logger on its owner before starting writers. Lazy named access
remains available, and explicit named initialization rejects a second configuration.
Backend/file callbacks suppress recursive ordinary writes and reject logger
acquisition, initialization, and lifecycle reentry before entering locks.

## Native output terminal

The optional [Linux Debug terminal module](../modules/debug-terminal-linux.md)
captures process stdout/stderr, including console sinks and emergency writes,
independently of logger profiles. Configured file sinks keep their own destinations.
Actual Debug configuration controls automatic inclusion/display; explicit CMake
inclusion can permit other configurations, which require a runtime enable argument.
Engine does not depend on the module, and without its attachment output uses the
existing inherited destinations. The owner guide defines stream/report separation,
manual closure, fallback and abnormal-exit retention.

## Compile policy

Engine propagates the usage policy declared in
[`projects/engine/cmake/Logging.cmake`](../../../projects/engine/cmake/Logging.cmake).
This is build policy rather than another public runtime library. Consumers
and tests inherit the same definitions. CMake's
CHERYL_LOG_PROFILE selects auto, developer, support, or release. Auto selects
developer for Debug, support for RelWithDebInfo, and release for other configurations.

| Profile | Least severe compiled level | Mask |
| --- | --- | --- |
| Developer | TRACE | 0x3f |
| Support | DEBUG | 0x1f |
| Release | INFO | 0x0f |
| Explicit minimal override | WARN | 0x07 |
| Explicit disabled override | none | 0 |

CHERYL_LOG_COMPILED_MASK is an optional target-wide override. CTWriteMask remains
a legacy alias/override and must agree with it. Definitions must match across all
translation units sharing Cheryl templates; defining a different mask in one file
is unsupported. Header-only use without CMake defaults to developer without NDEBUG
and release with NDEBUG. Runtime levels cannot restore a compiled-out severity.
The include-order override in block.h is removed.
`CE::Logging::Compile` owns the compile-policy helpers; the configuration and
emission macros remain global preprocessor names.

## Runtime settings and ownership

### Runtime thresholds

`CE::Logging::Config::for_logger(name, profile)` supplies explicit initial thresholds.
`CE::Logging::Profile` selects Developer, Support or Release presets. Default
construction uses the selected compile profile's runtime preset; an explicitly
supplied profile/settings object can choose different runtime levels.

| Profile | Logger | File | Console |
| --- | --- | --- | --- |
| Developer | DEBUG | DEBUG | INFO |
| Support | DEBUG | DEBUG | WARN |
| Release | INFO | INFO | WARN |

The memory logger starts at INFO for developer/support and WARN for release, with
the console threshold from the same preset. DEBUG statistics/TRACE require an
explicit level change and a compile profile that retained them. Setters remain
independent; setting a destination to DEBUG also requires a logger gate at DEBUG
or TRACE. Off is an explicit runtime filter, independent of the compile mask.

### Destinations and initial configuration

File defaults remain logs/name.log, 10 MiB rotation, five retained backups, and
rotation on open. Config allows directory, rotation size/count, rotation-on-open,
and all three initial levels to be selected before publication. A logger name must
be a nonempty filename component. Invalid levels/rotation limits are rejected
before file/registry side effects. Relative directories, including an empty path
for the working directory, resolve once to an absolute normalized path; reopening
keeps that destination if the application changes its working directory.

Configuration is per category. Settings applied only to the engine category do
not configure the OS-platform, rendering or asset destinations. Configure
them explicitly before first use when selecting a shared directory or overrides;
[Enable useful output](#enable-useful-output) shows that initialization sequence.
Use the corresponding name for engine, rendering, assets and memory, allowing
each category to retain its own preset or overrides. Categories open lazily;
unused categories need not create files, while explicitly initializing an off
category still opens its file. Stop all producers, then close every used category
on the host's logging owner. Completed close and retained-resource behavior are
unchanged. Additional categories share the same pool/worker and can delay one
another; separate files do not isolate queue saturation or slow destinations.

Owned logs accept Log(handlers, config). Named logger users can call
Logger<name>::initialize(CE::Logging::FileEventHandlers{}, config) before starting
writers; repeated explicit initialization rejects. The returned
initial_configuration reference is immutable and borrowed for that Log's lifetime.
It reports startup settings; live level changes remain on the current resources
and are captured/restored across close/reopen. Pattern setters affect the current
generation; preserving later pattern overrides across reopening is not promised.

### Shared queue and overflow

`CE::Logging::Config::overflow_policy` selects `CE::Logging::OverflowPolicy::Block`
or `DiscardNew` per Log, independently of
severity profiles. The setting remains immutable and is reapplied after reopen.
All existing presets retain Block as their compatibility default. Configure the
owner before starting producers. Consumers must use compatible configuration
and template definitions across libraries.

| Overflow policy | When the shared queue is full |
| --- | --- |
| Block | Wait for capacity, then enqueue the new item. |
| DiscardNew | Count and reject the new item; leave accepted items intact. |

Both use the same shared 8192-item pool and can coexist across logger instances.
spdlog already implements both paths; Cheryl passes each Log's setting to its
native async logger. A Block write returns after submission, without waiting for
backend output. DiscardNew avoids waiting for capacity, but formatting, allocation,
and mutex acquisition can still take time. Queue saturation frequency has not been
measured; bursts and slow or blocked sinks are relevant even when normal traffic
is small.

### Flush and close completion

Flush follows the same per-Log policy: a DiscardNew flush request can be lost at
capacity. flush only submits a request, with no completion/durability acknowledgement.
Successful close retains its accepted-owner/file-completion boundary without needing
an extra queued flush request. Neither policy guarantees successful physical I/O or
fsync durability; required failure reports still use the independent emergency path.

### Queue observations

Log::shared_queue_stats and Logger::shared_queue_stats return queued_items and
discarded_items across all logs sharing the pool, including flush requests.
queued_items excludes work already taken by the worker. The fields are sampled
independently; they are observations rather than an atomic pair or a completion
barrier. The discard counter is not reset by close/reopen, and no per-Log reset
is exposed. Owned Logs can inspect it while closed; the compatibility facade
retains its normal lazy-initialization behavior. Filtered/closed writes do not
count as queue-capacity discards.

### Retained file ownership

The file sink remains the close-completion boundary. Successful close means accepted
queued owners and private retained file owners have released that sink and its close callback
has completed. Retained backend work can delay close; timed failure
leaves Closing intact. Stop producers before asking for a completed shutdown.
An off file threshold still creates the file. Console-only/custom destinations
require a separate completion contract before being supported.

## Filtering and failure boundaries

### Filtering and guarded arguments

Log::should_log checks compile policy, the logger gate, and whether at least one
actual sink admits the level. A file DEBUG threshold can therefore remain useful
while the console is WARN. The logger gate must admit the least severe destination
record required. A concurrent close or level change can race the later write;
filtering is a best-effort probe, not a transaction with configuration changes.
The native sink vector is fixed. Replacement/addition/removal is unsupported and
submission rejects a changed graph. Use the Log's level/pattern operations; custom
destination graphs require a separate ownership/completion design.

UTRACE/UDEBUG/UINFO/UWARN/UERROR/UFATAL preserve streaming syntax. Their compile
and runtime guards run before stream construction and argument evaluation, and
they preserve an enclosing if/else. Acquisition and destructor emission failures
use the emergency reporter. Stream construction and caller insertion expressions
can still throw; a partial stream is not emitted during that unwinding.

CE_LOG_TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL(name, format, args...) provide guarded
formatted emission. Logger::write_lazy<severity>(callback) is the underlying API:
the callback runs only when compiled and admitted. Initialization, argument
preparation, and submission exceptions use the emergency path and do not change
the engine operation. A callback must submit the severity it declares.

Direct Log/Logger trace/debug/info/warn/error/critical functions obey compile and
runtime filtering, but C++ evaluates their supplied arguments before entering
the function. Use the guarded formatted macros or write_lazy for expensive or
potentially throwing diagnostic preparation. Critical stack capture requires
both compiled TRACE and a destination admitting TRACE.

Ordinary logging belongs outside memory/scheduler/application locks. Native
callbacks, final deleters, and noexcept cleanup use the bounded emergency reporter
described in [failure-reporting.md](failure-reporting.md). Split/neighbor memory
leaf operations are quiet, including normal misses and byte-aligned remainders;
future aggregate diagnostics must use a copied snapshot after releasing locks.

### Backend queue ownership

The shared async pool belongs to Cheryl and does not replace spdlog's process-global
pool. Each Log retains it through its own destruction, independently of the
internal pool-initialization object's destruction order. The current default remains
8192 queued items and one worker. The bundled pool creates threads in a loop
without unwinding already-started threads if a later thread creation fails. The
single-worker default avoids that partial-startup path and parallel backend
reordering; it does not order producer work before submission or establish a
throughput claim. Accepted queue entries retain their native
logger/sinks; those loggers borrow the pool. Do not make sinks/queued loggers retain
the pool, which can create a queue-owner cycle and final destruction on a worker.

### Destination failure and recovery

The per-Log saturation policy is configurable; blocking remains the default.
Owned file and console operations now pass through guards. Every exception from
delegate log/flush, including non-standard formatter/rotation failures, is caught
after its delegate lock unwinds. The failed destination is marked degraded and
reports once for that failed operation through the emergency path. Already queued
operations subsequently skip that destination; the other destination continues.
This prevents writes/flushes into a file that a failed rotation may have closed.
Ordinary reopen while Open is still a no-op: recovery requires completed close
followed by reopen, creating new destination states and formatters.

### File callbacks

Each before_close/after_close handler has its own nonthrowing guard, so one failing
handler cannot prevent physical close, the other handler, or Cheryl's completion
signal. Failures are collected in bounded slots and reported after delegate locks
unwind or file destruction finishes. Repeated failures in the same callback slot
before reporting are counted and coalesced. Opening handlers retain their throwing
contract; constructor/rotation errors remain the primary failure, even if cleanup
handlers also fail. Startup cleanup does not replace the original exception.

### Failure observations

Log::backend_stats exposes cumulative failed_operations, callback_failures, and
suppressed_operations for that Log, including failed openings. Its degradation
flags describe the last published generation, including while Closed. Counts
survive close/reopen; fields are independent observations, not a completion barrier.
The logger's owned graph uses forwarding guards, whose actual delegate gates and
degradation participate in should_log. File/console level restoration still uses
the actual destinations. Retained native file owners still delay completed close.
Direct delegate log/flush bypasses these guards and is unsupported.

The compiled Engine backend owns guarded native loggers and destinations.
Public `Level`, `FileEventHandlers`, configuration and thin formatting/filtering
frontends contain no spdlog types or native sink handles. Applications cannot
retain/clone native loggers or replace their sink graph. Internal queued owners
borrow the pool to avoid a queue-owner lifetime cycle. Pattern operations retain
the callback reentry guard.

### Callback reentry

A shared thread-local backend scope covers both destination operations and file
callbacks, including startup on the logging owner. Recursive writes through owned,
facade, stream and guarded formatted paths are suppressed and
counted on the active callback's Log. Guarded argument expressions are not evaluated
and unopened categories are not initialized. Lifecycle and category acquisition/
initialization reject with bad_request before waiting. Engine centralizes registry
and backend ownership; templates do not create private native registries in each
consumer library.
Callbacks must not synchronously wait for work on another thread that itself needs
the logging backend: a thread-local guard cannot resolve application wait cycles.

### Shutdown and retained owners

The final release of an owned Log or private file destination
must occur outside backend/file callbacks. Destruction cannot return a rejection
or preserve already-destroyed storage; violating this contract terminates through
an emergency report before a worker can wait/join itself. Isolated acceptance
sources exercise this fatal contract; native delegates are private implementation.

backend_stats also exposes recursive_submissions and rejected_reentry. report_diagnostics
emits bounded coalesced cumulative counters through stdio outside lifecycle locks;
completed close calls it after callback completion. Shared capacity losses are
reported once per newly observed range across categories, including flush loss.
The final pool deleter drains/joins accepted work and reports any outstanding loss
range, without ordinary logger use or queued pool ownership. flush releases its
lifecycle lock before a potentially blocking queue submission.
Use the [logging acceptance procedures](../development/logging-acceptance.md) for
compile-profile, callback/failure and teardown coverage limits.
Stop producers before closing. A close timeout bounds the sink-completion wait,
not arbitrary user callbacks, native I/O, or the final pool's thread joins.
Final facade destruction belongs to the logging owner, outside the async backend;
sink/file callbacks must not close, destroy, or wait for their logger/pool.

## Validation

The [logging acceptance guide](../development/logging-acceptance.md) owns the
compile-profile matrix and fault/queue/teardown procedures. It runs already-built
focused cases and isolated processes with bounded timeouts, including named routing,
recursive submission, retained owners and fatal callback destruction. These checks
cover defined scenarios; they do not prove every concurrent interleaving. Sanitizer
configurations supplement that scope.
