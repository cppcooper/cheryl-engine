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

These counters describe operations and handle ownership, not GPU allocation bytes,
durability, scheduler guarantees, or race freedom. Native recording tests establish
engine behavior; actual driver/platform acceptance is separate.
