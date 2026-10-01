# Earlier-task audit checkpoint

1 October 2026. This is a partial review of tasks 0–6 against the second-pass work
order, starting from the user-pushed `8f796148e64d3dde6faf29b9333893133eb9cceb`.
Earlier completion labels describe implemented source scope. They do not establish
that the implementation is correct, that every path was reviewed, or that the
acceptance criteria passed. The broader audit remains open alongside implementation, as subsequently requested.

## Findings and prepared changes

### A1: event invalidation was published after registry removal

Previously, close detached the registry under its mutex and invalidated listeners
after unlocking. Another close could see an empty registry and return before the
first closer closed the invocation gates. A queued task could then enter after a
close had returned. Concurrent unregister also detached an entry before closing
its gate, letting close miss that entry. The earlier duplicate-unregister fix did
not solve either interaction.

Checkpoint 35 invalidates before removal while holding the registry mutex. The
lock order is registry then listener. Invocation and completion release the
listener mutex before user code and never acquire the registry while holding it.
Local/detached owners retain callbacks until both locks have been released, so
destructors can reenter. Already-running callbacks still finish without making
close wait for them.

Two prepared scenarios cover concurrent closers with an in-flight callback and a
queued callback, and callback-capture destruction which reenters close. They
exercise the public lifecycle contract; they do not force the old intermediate
detach/invalidate window deterministically. The race finding rests on the lock
and interleaving review, not a claim of an executed reproduction.

### A2: polling saturation could overflow before making its comparison

PollingBacklog computed `time_point::max() - completed_at` before checking spacing.
A timestamp before the epoch can make that subtraction exceed the signed clock
duration range. A steady clock's epoch is unspecified; nonnegative timestamps
are not an API guarantee.

Checkpoint 36 compares completion against `max - nonnegative_spacing`, then adds
only when representable. Prepared scenarios cover near-limit completion,
maximum spacing, zero spacing, and retained observations after saturation. The
current InputBindings baseline comes from the real clock, so these fixtures do
not provide a synthetic negative-epoch observation. That branch was checked by
arithmetic review; execution and a suitable controlled-input fixture remain open.

### A3: worker integration coverage was narrower than the implemented policies

Existing sources covered a concurrency cap of one and one required CPU, but not
parallel cap isolation or effective-mask restoration between groups. Checkpoint
37 adds a cap-of-two scenario with held jobs and independent-group progress, and
a Linux pinned/inherited/pinned mask sequence on one physical worker. A bounded
future wait limits failure cleanup; latches establish the cap scenario's state.
These are coverage additions, not evidence of a discovered scheduler failure.

### A4: status documentation lagged behind implementation

Dispatch documentation still said timing configuration was future work. The
residency table described pipeline/material contracts as wholly pending even
though their strong ownership and native builders now exist. Checkpoint 38
corrects those boundaries and distinguishes source scope from audit sign-off.
The attached-stage retention issue found during task 7 was already fixed in
checkpoint 32; it is further evidence that the earlier task-6 review was incomplete.

### A5: queued payload preparation could report cancellation and propagate separately

Checkpoint 39 moves ticket allocation and owned-payload construction into the same
failure boundary as posting. A payload-copy failure records the original exception
on an existing ticket before its destructor can report cancellation; allocation
failure without a ticket reports through the active listener's sink after unlocking.
Prepared throwing-copy/target scenarios cover sink reentry and recovery. These have
not been executed, and the broader queued lifecycle combinations remain open.

### A6: fixed-state integration exposed depth-clear mask dependence

Pipeline draws can disable depth writes. OpenGL depth clearing obeys that retained
mask, so checkpoint 40 enables writes before clearing; the next draw reapplies its
own complete policy. This is a state/ownership review finding, not an observed GPU
failure. Native acceptance remains open.

## Coverage at this checkpoint

| Task | Paths reviewed in this pass | Assessment and remaining review |
| --- | --- | --- |
| 0: baseline | Pushed/local tree reconciliation, original base, identity, patch numbering, aggregate regression discovery. | Remote and previous local checkpoint trees match. Original base retained; patches 35–38 continue from the pushed head. |
| 1: dispatch | Platform/simulation queue state, saved endpoints, detached drain and cancellation; both runtime update boundaries. | No new defect found in these paths. Recheck owner-bound capture release and producer/closure interactions during the full convergence review. |
| 2: events | Registration ownership, invocation/removal/wait lock order, concurrent close, cancellation tickets, posting serialization, worker stream. | A1/A5 fixed in source. Nested concurrent dispatch, and target rejection/cancellation combinations still need focused review and executed acceptance. |
| 3: workers | Job ownership/accounting, weighted eligibility selection, caps, close/drain/join, self-wait rejection, affinity readback/fallback. | A3 prepared. Dynamic restriction changes, native failure paths, startup rollback, and overlapping weighted affinity workloads are not comprehensively validated. |
| 4: integration | Context-owned/injected groups, factory forwarding, documented preparation/upload handoff, both shutdown paths and platform pumping. | No new defect found in these paths. Quiesce runs after simulation joins and must not wait on work needing platform service. Reentrant producer/failure combinations remain open. |
| 5: timing | Scheduler arithmetic/recovery, simulation versus observation time, input consumption, frame slots/publication, polling deadlines. | A2 fixed. Reviewed bounded batch and full-backlog ordering; runtime clock, expensive callbacks, and presentation behavior still need acceptance. |
| 6: residency | Strong cache publication/clear/rebind, provider release, move-only lifetime retirement, native creation guards, idle/shutdown maintenance, resource ownership declarations. | No new deletion defect found in these paths. Full composite-construction failure review, retained resources across context loss, and real GPU behavior remain open. Checkpoints 40–44 implement pipeline/frame migration; native acceptance stays open. |

## Validation and continuation

All six changed C++ files receive syntax-tree inspection, and the continuation
receives whitespace and combined/incremental/original-base mailbox replay checks.
These checks do not establish C++ type correctness, linking, native execution, or
race freedom. No configuration, compilation, regression execution, real-context
acceptance, or remote write occurred in this batch.

Continue the task-2 failure/lifecycle audit first, then worker native/startup failure
and execution shutdown combinations, followed by the remaining resource failure
paths. Review exact fixtures and implementation together. Keep tasks 0–6 source
scope recorded, but leave audit sign-off and all executed acceptance open. Continue selective review alongside task-9 convergence; do not infer completeness
from batch duration.
