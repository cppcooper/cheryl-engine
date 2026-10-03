# Logging configuration and emission

Cheryl keeps the existing cheryl, engine, and memory logger names. A subsystem or
operation belongs in the record; it does not automatically create another file.
Configure a logger on its owner before starting writers. Lazy default access remains
available, and explicit singleton initialization rejects a second configuration.
Initialization/file callbacks must not recursively construct the logger being opened.

## Compile policy

The cherylGL target publicly propagates cheryl_logging_config. The test target uses
the same configuration while it still recompiles engine sources. CMake's
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

Owned logs accept Log(handlers, config). Singleton users can call
Logger<name>::initialize(spdlog::file_event_handlers{}, config) before starting
writers; repeated explicit initialization rejects. The returned
initial_configuration reference is immutable and borrowed for that Log's lifetime.
It reports startup settings; live level changes remain on the current resources
and are captured/restored across close/reopen. Pattern setters affect the current
generation; preserving later pattern overrides across reopening is not promised.

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
Mutating the native spdlog sink vector requires external quiescence.

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

The existing async queue is still blocking at this stage. Cheryl-owned queue lifetime,
saturation/drop reporting, and backend failure handling are the following U5 unit.
Broad subsystem integration remains gated on those contracts. Existing close/reopen
and retained file-sink semantics are preserved.

## Acceptance boundaries

Regression sources cover all direct severity paths under the selected profile,
disabled argument side effects, independent sink filtering, enclosing if/else,
lazy argument failures, stream emission failures, and suppressed partial streams.
A first-include translation unit preserves the mask across block.h inclusion.
Runtime configuration sources cover published defaults, independent memory presets,
custom append/rotation settings, level restoration, relative-path stability after a
scoped working-directory change, and invalid settings with zero file-open side effects.
Compile/link execution and the profile matrix remain unexecuted; no compilation
or tests were authorized. Queue saturation, sink failures, and shutdown require
separate acceptance as the remaining lifecycle units land.
