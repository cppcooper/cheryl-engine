# Logging configuration and emission

Cheryl groups engine records into five explicitly named destinations. Their public
names are declared in
[log-names.h](../../projects/engine/include/cheryl/core/logging/log-names.h), also included by
core/logging.h. Default paths are relative to the configured log directory:

| Name | Default file | Records |
| --- | --- | --- |
| CE::enginelog | logs/engine.log | Runtime lifecycle/timing, workers, dispatchers and events |
| CE::platformlog | logs/os-platform.log | OS display/window/input services, GLFW and context selection |
| CE::renderlog | logs/rendering.log | Renderer/shader records and native graphics resource/debug observations |
| CE::assetlog | logs/assets.log | Asset preparation/upload, cache publication and reload |
| CE::memlog | logs/memory.log | Allocator bookkeeping, preallocation and ownership errors |

Each file retains subsystem/operation/domain fields for finer correlation. Runtime
input capability/focus records go to os-platform, while session records stay in
engine. engine.log is the runtime/execution timeline; other categories contain
their own subsystem details, with no automatic aggregate mirror.

Every engine write selects its category explicitly. Changing spdlog's default
logger, including through an application's Log::make_default(), does not redirect
engine records. Logger::get/initialize reads default_logger solely to establish
registry lifetime before singleton storage; emission uses its own named resource.
Required native/noexcept/logger failures use bounded stderr independently of any
default or file destination.

The existing enginelog, memlog and ce_log_name symbols retain their identities and
values. CELog/Logger<CE::ce_log_name> remains an explicit legacy cheryl destination
for source compatibility. Engine-owned operations no longer create cheryl.log;
an application that explicitly uses the legacy destination still creates it.
The destination migration does not move, merge, delete or rename existing files
and backups. Normal configured rotation and retention still apply.

Configure each used logger on its owner before starting writers. Lazy named access
remains available, and explicit singleton initialization rejects a second configuration.
Backend/file callbacks suppress recursive ordinary writes and reject logger
acquisition, initialization, and lifecycle reentry before entering locks.

## Compile policy

The cengine target publicly propagates cengine_logging_config. The test target
consumes the actual library and inherits that same configuration. CMake's
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

## Runtime settings and ownership

LogConfig::for_logger(name, profile) supplies explicit initial thresholds. Default
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

File defaults remain logs/name.log, 10 MiB rotation, five retained backups, and
rotation on open. LogConfig allows directory, rotation size/count, rotation-on-open,
and all three initial levels to be selected before publication. A logger name must
be a nonempty filename component. Invalid levels/rotation limits are rejected
before file/registry side effects. Relative directories, including an empty path
for the working directory, resolve once to an absolute normalized path; reopening
keeps that destination if the application changes its working directory.

Configuration is per category. Settings previously applied only to enginelog do
not configure the new OS-platform, rendering or asset destinations. Configure
them explicitly before first use when selecting a shared directory or overrides:

```cpp
#include <cheryl/core/logging.h>

auto config = CE::LogConfig::for_logger(CE::platformlog);
config.directory = "logs";
CE::Logger<CE::platformlog>::initialize(spdlog::file_event_handlers{}, config);
```

Use the corresponding name for engine, rendering, assets and memory, allowing
each category to retain its own preset or overrides. Categories open lazily;
unused categories need not create files, while explicitly initializing an off
category still opens its file. Stop all producers, then close every used category
on the host's logging owner. Completed close and retained-resource behavior are
unchanged. Additional categories share the same pool/worker and can delay one
another; separate files do not isolate queue saturation or slow destinations.

Owned logs accept Log(handlers, config). Singleton users can call
Logger<name>::initialize(spdlog::file_event_handlers{}, config) before starting
writers; repeated explicit initialization rejects. The returned
initial_configuration reference is immutable and borrowed for that Log's lifetime.
It reports startup settings; live level changes remain on the current resources
and are captured/restored across close/reopen. Pattern setters affect the current
generation; preserving later pattern overrides across reopening is not promised.

LogConfig::overflow_policy selects Block or DiscardNew per Log, independently of
severity profiles. The setting remains immutable and is reapplied after reopen.
All existing presets retain Block as their compatibility default. Configure the
owner before starting producers; future category default changes belong to the
subsystem error-ownership audit. Source consumers must rebuild for the updated
configuration/template layouts.

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

Flush follows the same per-Log policy: a DiscardNew flush request can be lost at
capacity. flush only submits a request, with no completion/durability acknowledgement.
Successful close retains its accepted-owner/file-completion boundary without needing
an extra queued flush request. Neither policy guarantees successful physical I/O or
fsync durability; required failure reports still use the independent emergency path.

Log::shared_queue_stats and Logger::shared_queue_stats return queued_items and
discarded_items across all logs sharing the pool, including flush requests.
queued_items excludes work already taken by the worker. The fields are sampled
independently; they are observations rather than an atomic pair or a completion
barrier. The discard counter is not reset by close/reopen, and no per-Log reset
is exposed. Owned Logs can inspect it while closed; the compatibility facade
retains its normal lazy-initialization behavior. Filtered/closed writes do not
count as queue-capacity discards.

The file sink remains the close-completion boundary. Successful close means accepted
queued owners and retained file owners have released that sink and its close callback
has completed. A retained native logger/file sink can delay close; timed failure
leaves Closing intact. Stop producers before asking for a completed shutdown.
An off file threshold still creates the file. Console-only/custom destinations
require a separate completion contract before being supported.

## Filtering and failure boundaries

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

The shared async pool belongs to Cheryl and does not replace spdlog's process-global
pool. Each Log retains it through its own destruction, independently of the
compatibility TPInit singleton's destruction order. The current default remains
8192 queued items and one worker. The bundled pool creates threads in a loop
without unwinding already-started threads if a later thread creation fails. The
single-worker default avoids that partial-startup path and parallel backend
reordering; it does not order producer work before submission or establish a
throughput claim. Accepted queue entries retain their native
logger/sinks; those loggers borrow the pool. Do not make sinks/queued loggers retain
the pool, which can create a queue-owner cycle and final destruction on a worker.

The per-Log saturation policy is configurable; blocking remains the default.
Owned file and console operations now pass through guards. Every exception from
delegate log/flush, including non-standard formatter/rotation failures, is caught
after its delegate lock unwinds. The failed destination is marked degraded and
reports once for that failed operation through the emergency path. Already queued
operations subsequently skip that destination; the other destination continues.
This prevents writes/flushes into a file that a failed rotation may have closed.
Ordinary reopen while Open is still a no-op: recovery requires completed close
followed by reopen, creating new destination states and formatters.

Each before_close/after_close handler has its own nonthrowing guard, so one failing
handler cannot prevent physical close, the other handler, or Cheryl's completion
signal. Failures are collected in bounded slots and reported after delegate locks
unwind or file destruction finishes. Repeated failures in the same callback slot
before reporting are counted and coalesced. Opening handlers retain their throwing
contract; constructor/rotation errors remain the primary failure, even if cleanup
handlers also fail. Startup cleanup does not replace the original exception.

Log::backend_stats exposes cumulative failed_operations, callback_failures, and
suppressed_operations for that Log, including failed openings. Its degradation
flags describe the last published generation, including while Closed. Counts
survive close/reopen; fields are independent observations, not a completion barrier.
The logger's owned graph uses forwarding guards, whose actual delegate gates and
degradation participate in should_log. File/console level restoration still uses
the actual destinations. Retained native file owners still delay completed close.
Direct delegate log/flush bypasses these guards and is unsupported.

Published spdlog owners now retain a guarded frontend and a private asynchronous
backend with the same guarded destinations. The backend borrows the shared pool;
no queued owner acquires a strong pool reference. Native submissions and clones
preserve queue/retained-file ownership and pass through the recursion check.
Changing their concrete type to spdlog::async_logger is unsupported. Native setters
must not replace error handlers or the fixed sink graph. Pattern/formatter setters
on guarded destinations reject callback reentry before delegate locks.

A shared thread-local backend scope covers both destination operations and file
callbacks, including startup on the logging owner. Recursive writes through owned,
facade, stream, guarded formatted, and published native paths are suppressed and
counted on the active callback's Log. Guarded argument expressions are not evaluated
and unopened categories are not initialized. Lifecycle, native flush/clone, and
singleton acquisition/initialization reject with bad_request before waiting.
Facade acquisition initializes the native registry before its singleton storage,
so supported static teardown closes facade logs before destroying that registry.
Callbacks must not synchronously wait for work on another thread that itself needs
the logging backend: a thread-local guard cannot resolve application wait cycles.

The final release of an owned Log, published native logger, or file destination
must occur outside backend/file callbacks. Destruction cannot return a rejection
or preserve already-destroyed storage; violating this contract terminates through
an emergency report before a worker can wait/join itself. Isolated acceptance
sources must exercise this fatal contract. Raw native delegates and explicit casts
to the singleton base are outside the guarded API.

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
External native owners must not continue producing after facade teardown.
Final facade destruction belongs to the logging owner, outside the async backend;
sink/file callbacks must not close, destroy, or wait for their logger/pool.

## Acceptance boundaries

Regression sources cover all direct severity paths under the selected profile,
disabled argument side effects, independent sink filtering, enclosing if/else,
lazy argument failures, stream emission failures, and suppressed partial streams.
A first-include translation unit preserves the mask across block.h inclusion.
Runtime configuration sources cover published defaults, independent memory presets,
custom append/rotation settings, level restoration, relative-path stability after a
scoped working-directory change, and invalid settings with zero file-open side effects.
The host-pool source case constructs a separate pool owner even if the singleton
already exists, catching the previous replacement of the host's global pool.
Saturation sources hold a backend operation while another logger fills the shared
queue. They cover DiscardNew returns/loss counts, lost flush requests, accepted
record ordering, mixed per-Log policies, retained selection after reopen, and Block
waiting for capacity. Invalid policy values reject before file/registry effects.
The held-backend release guard runs before producer joins and logger cleanup on
assertion failure. The blocking case includes a bounded scheduling observation;
isolated processes provide the timeout and fault-injection acceptance boundary.
Additional sources cover independently throwing close handlers, startup exceptions
with failing cleanup, non-standard file formatters, standard console formatters,
continued healthy-destination output, cumulative failure counts, and close/reopen
recovery. The [logging acceptance guide](../development/logging-acceptance.md)
defines the profile/sanitizer procedures and retained coverage limits. Further build/test runs require the
authorization specified by AGENTS.md.

The standalone `acceptance-logging` target and
[logging.py](../../projects/engine/tests/logging-acceptance/logging.py) run fault, failed rotation, full-queue
native reentry, discarded record/flush, retained clone, named category routing,
static teardown, and fatal
callback destruction scenarios in independent processes with a 30-second timeout.
The runner uses already-built binaries, runs logging.* regressions serially in
temporary working directories, verifies final file content/order and loss reports,
and expects the defined fatal ownership case to exit 86 through a test terminate
handler. It performs no configuration/build. See
[logging-acceptance.md](../development/logging-acceptance.md) for the profile matrix
and the authorization gate. Named-routing acceptance replaces the application's
default logger, verifies formatted/stream/direct engine writes stay in their own
files, and checks independent filtering and closed/reopened category behavior.
