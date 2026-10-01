# Earlier-task audit checkpoint

1 October 2026. This is a partial review of tasks 0–6 and their integration with
tasks 7–8 against the second-pass work order, starting from the user-pushed `8f796148e64d3dde6faf29b9333893133eb9cceb`.
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

### A7: a worker stream advertised a pump before submission succeeded

Checkpoint 46 serializes the first pump's pool submission with stream publication.
Previously another producer could return accepted while the initiating submit was
still fallible. Rejection then destroyed both requests on the initiating producer,
including another listener's cancellation sink under the initiating listener's
posting lock. Reentry could block on that lock. Checking group closure under the
stream mutex also excludes the draining pump's final empty-queue/completion step.
The fix releases only the failed initiating capture after unlocking and keeps the
stream-to-pool lock order. Prepared sources cover close with queued accepted work,
reentrant rejection reporting, and independent stream progress; they do not force
the old submission interleaving. Native pump-rejection acceptance remains open.

### A8: cache insertion moved its final local candidate owner into fallible work

Checkpoint 47 keeps a local strong candidate owner through try_emplace. Previously
node construction could move away that last owner, then insertion/rehash failure
could destroy the node under the cache write lock. A final deleter inspecting that
cache could deadlock. Keeping the argument's owner makes final cleanup occur after
lock unwinding, including duplicate and failed insertion. Replacement already
inserts an empty slot before exchanging ownership, and clear detaches the map.
Prepared generic-cache sources cover duplicate-candidate deleter reentry and a
controlled hash-rejection failure. Allocation/rehash failure was checked by owner
and exception-scope review; those fixtures do not force allocation failure.

### A9: static texture unbinding bypassed native ownership and inherited a unit

Checkpoint 48 replaces Texture::unbind() with instance-bound unbind(unit), checking
the owning live/current context and unit limit before selecting and clearing that
unit. Constructor cleanup supplies unit zero explicitly. Other binding paths already
check their owners/domains. Prepared native recording sources cover unrelated
active-unit state and rejection on foreign/missing/closed contexts or invalid units.
No real OpenGL execution has occurred.

## Coverage at this checkpoint

The 46–51 continuation starts from pushed `7f042e9d91e28a9addb8d66284e9cdb3ff6938fd`,
which matches the previous local 39–45 tree. Source review spans both runtime modes,
dispatcher queue/capture ownership, event lifecycle and lock order, worker scheduling
and native policy, scheduler/input arithmetic, cache publication, native construction
and binding, material resolution, CPU submission, and frame retention/recycling.
A7–A9 are source fixes found by this review. No additional handoff defect was found
in the inspected frame state transitions; that is a limited review result.

Checkpoint 49 adds coordinated frame/reload/failure cleanup sources in both modes.
Checkpoint 50 adds required CPU-mask revalidation after a job changes its own mask.
The detailed path/evidence/remaining-acceptance map is in
[ARCHITECTURE-CONVERGENCE-REVIEW.md](ARCHITECTURE-CONVERGENCE-REVIEW.md).

## Validation and continuation

Changed C++ files receive syntax-tree inspection; whitespace, API/include, and
ownership/lock review accompany the changes. Combined continuation, numbered
increments, and original-base mailbox replay check exact source trees and
ordered author/date/messages before delivery. These checks do not establish type
correctness, linking, native execution, or race freedom. No configuration, build,
regression execution, real-context acceptance, or assistant remote write occurs.

Keep the partial audit open. Controlled native affinity/thread-start failure,
allocation/rehash failure, the old pump-submission window, real context loss, and
cross-task producer/failure combinations remain acceptance or coverage gaps.
Recovery checkpoints 52–53 prepare overlapping weighted affinity, capture-deleter
reentry, failed-shutdown recovery, and abandonment/late-release coverage. Those
sources also remain unexecuted. Do not infer exhaustive review or correctness from batch duration.
