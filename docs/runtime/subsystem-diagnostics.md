# Subsystem diagnostics

Engine operational records use the existing engine category and target-consistent
logging profile. The application owns early explicit configuration, producer stop,
and final logger close. Closed/degraded sinks and stripped levels do not change
engine exceptions or futures. Required failure context uses bounded emergency
output independently of logging.

Records identify subsystem, operation/phase, outcome, and a monotonic process-local
domain. IDs are neither addresses nor native handles and are not persistent keys.
Zero indicates identity exhaustion. Session failures keep the original exception
and phase while distinct cleanup failures retain their own emergency context.

RuntimeStats is published after the simulation worker joins. Callers synchronize
with run start/return and may inspect an unstarted/completed session; live sampling
rejects. Counts describe update attempts, completed polls, published/rendered frames,
supersession, skipped publication, dropped timing batches/duration, peak polling
backlog, and resize observations. Normal ticks/frames emit no INFO records. Session
start/capabilities/end and one final DEBUG summary are bounded by session lifecycle;
lag drops produce an aggregate warning rather than per-update records.

NativeResourceStats counts registrations, live/pending native handles, collection,
shutdown deletion, and abandonment for one resource lifetime. Taking the snapshot
performs no GL calls or logging. Final release only queues retirement. Abandonment
and failed untracked cleanup use emergency records after the lifetime lock releases;
they never try to recover by issuing GL calls on an unavailable context. Explicit
renderer shutdown reports lifecycle totals, while destructor recovery has no ordinary
logger dependency.

WorkerGroupStatus correlates group and physical pool domains and adds callback
failure, peak pending, and cumulative queue-wait nanosecond observations. Job
exceptions still reach only their futures. Explicit pool/group report_diagnostics
observers run without application/collection locks: pool capacity is announced once,
preferred fallback once, and native policy failures as newly observed count ranges.
EngineContext invokes them after group publication and after drain; standalone
pool owners choose their own observation boundary. Constructors and pool destruction
do not log inside a containing context lock. Unsupported required policy/startup
is reported at the context operation owner before the original error propagates.

DispatchStats covers accepted, completed, cancelled, failed, queued and detached
batch counts. Callback failures are counted before settling their own futures.
Opening/binding/closing uses DEBUG; queue peaks above 1024 produce a single shutdown
warning. This is an observation threshold, not a new admission limit. Close during
a callback can leave detached work executing, so the final joined snapshot is more
complete than that close record. No per-request ordinary emission occurs.

EventStats counts registration, dispatch, invocation, explicit discard and active
queued-delivery failures. Registration IDs remain bus-qualified, with bus_id for
correlation. The listener retains a separate counter owner without retaining the
bus/registry. ErrorHandler remains the sole queued-error owner, including cancelled
active work; explicit unregister/close discards do not generate additional errors.
report_diagnostics is explicit because event operations may run inside a platform
callback or a containing delivery lock. It samples no names or payloads.

PreparedAssets carries a batch identity through copies, worker preparation and
platform upload. ResourceProvider identities identify adapter instances, while cache
snapshots count cumulative publications/replacements separately from current entries.
Loader records batch begin/end, counts and elapsed time. Its last UploadStats includes
completed image/manifest calls and actual newly published entries, even when failure
occurs inside a manifest. Diagnostic counter sampling is best effort and cannot
replace an upload error; unavailable counts are explicit. Reused keys are completed
calls, not new publications. Neither counts nor records promise batch rollback.
Per-key program/material reload records preserve the prior generation on failure;
keys/paths and shader source are omitted from operational records.

Explicit GLSLProgram print_active_* calls retain the existing reflection/error
contract and emit DEBUG count summaries and TRACE name/type/size/location records.
No direct stdout tables remain. Query APIs already serve reflection consumers; the
selected consumer does not require an additional event transport. Successful shader
compiler/linker diagnostics are WARN presence/byte-count records, without copying
driver text/source into the ordinary log.

MemoryStats samples the shared byte-domain collections before formatting/emission.
It counts total/available bytes, owners, partitions and pending release owners.
Normal lookup/reuse/split/merge misses remain quiet. Explicit preallocation reports
completed blocks/width or a partial attempt; report_diagnostics is a host-triggered
DEBUG snapshot, so the host chooses the sampling period. These are bookkeeping
observations, not allocation-pressure/eviction budgets.

These counters describe operations and handle ownership, not GPU allocation bytes,
durability, scheduler guarantees, or race freedom. Native recording tests establish
engine behavior; actual driver/platform acceptance is separate.
