# Architecture implementation ledger

Original task base: `e8c9788f63cf4688e144b84feeb8f9aabf62540f`.
This base remains fixed for the continuing work, including later sessions.

The numbered tasks and discovery boundaries are in
[ARCHITECTURE-WORK-ORDER.md](ARCHITECTURE-WORK-ORDER.md).
Local commits use `cppcooper <cppcooper@users.noreply.github.com>`.
Compilation, test execution, and remote writes have not been requested.

## Progress

- Task 0 complete: source matches the planning snapshot; no applicable AGENTS.md
  was found; repository identity and original base are recorded.
- Task 1.1 complete: PlatformDispatcher, platform_dispatcher(), source filenames,
  demo, regression sources, and active documentation use the same name.
- Task 1.2 complete; 1.5 platform portion complete: safe saved endpoints,
  owner checks, FIFO detached drains, cancellation, and nested-drain exclusion.
- Task 1.3 dispatcher implementation complete: owned no-argument work,
  initialization-time acceptance, explicit owner binding, futures, and cancellation.
- Task 1 complete: both modes drain simulation work before whole-backlog transfer.
  Pending simulation captures cancel on their owner; initialization failure closes
  unbound queues before game cleanup. Five simulation regression scenarios prepared.
- Task 2.1–2.2 complete: isolated owned buses behind the singleton, persistent
  registration IDs, and immediate producer-thread delivery.
- Task 2.3–2.4 complete: entry/invalidation handshake, nonblocking removal,
  in-flight completion barrier, nested self-wait rejection, and bus close.
- Task 2.5–2.6 complete: copied payloads, per-listener enqueue serialization,
  deferred targets, cancellation tickets, and mandatory observable error sinks.
  Platform/simulation composition adapters cover the corresponding part of 4.2.
- Task 2 complete: worker delivery uses an owned serial stream on shared group
  capacity; copies share ordering, separate streams retain concurrency.
- Task 3.1–3.2 and 3.7 complete: owned threads/groups, futures, group caps,
  explicit close/drain/join, startup rollback, and self-wait rejection.
- Task 3 complete in source: weighted priority, caps, previous-worker preference,
  Linux affinity with readback, effective policy, unsupported hard topology paths,
  and native failure futures. Native acceptance remains unexecuted.
- Task 4.2 complete: all three execution targets are optional composition
  adapters without EventBus header dependencies. Task 4.1 complete: lazy configured owned root or injected shared capacity,
  context-scoped groups, and no shutdown of unrelated injected groups.
- Task 4 complete in source: WorkerGroup preparation example, quiesce hook,
  dependency-aware platform pumping through simulation/CPU shutdown, cancellation
  of remaining requests, injected ownership isolation, and original-error retention.
- Task 5 complete in source: explicit scheduler clock inputs, independent variable
  pacing/fixed steps, bounded fixed batches, direct/hybrid VariableCatchUp, cap/drop
  accounting, and matching runtime policies. Each actual update drains its mailbox
  then consumes the whole current input backlog; no-update cycles retain it.
  TickContext separates simulation delta from observed input durations and reports
  discarded time. Demo movement explicitly maps down-time proportion to simulation
  time. Frame preparation occurs once per useful batch; rendering retains the latest
  complete frame. Regression scenarios and timing/suspension/optimization documentation
  are prepared; compilation, regression execution, and real acceptance remain open.
- Tasks 6–8 pending. Task 9 acceptance remains unexecuted.
- Documentation and prepared regression sources (task 9) accompany each change.

## Discovery additions

- D0: the user applied/pushed checkpoints 01–15. Remote `ef4ec51402ebadd2f1319dfd03bb0ed3a0b6007d`
  has an identical source tree to local `b2686ce`, with reapplied commit IDs. Continue
  from the remote HEAD; retain the original base and earlier local checkpoint branch.
  New incremental patches start at the pushed HEAD. A continuation-only mailbox is
  provided for that base, alongside the preserved original-base cumulative artifact.
- D5: sample the clock once to select a bounded recovery batch. Time spent executing
  its updates enters the next scheduler cycle, preventing an ever-extending recovery
  loop. Deadline arithmetic saturates; invalid/unrepresentable intervals are rejected.
- D5: later steps in one recovery batch can have a zero observation interval.
  Derived simulation movement uses held State in that case; it does not replay a
  released tap. Raw durations, one-shot records, and relative motion retain their
  existing consumption contract. Prepared synthetic recovery coverage verifies this.
- D5: retaining a latest frame occupies one concurrent slot until replacement.
  Slot ownership stays platform-only for recycling; the remaining slots can carry
  preparation/latest publication, and simulation skips publication rather than waiting.

- D1: a saved delivery adapter must not borrow a dispatcher object. Add shared
  submission state under 1.5; closing invalidates acceptance before releasing
  captured requests, and rejection must also be safe after dispatcher destruction.
  Completion evidence: source review and prepared lifetime/cancellation regressions.
- D1: a producer can copy the wake callback immediately before queue closure.
  Wake callbacks must own their scheduler state; they must not borrow the runtime.
  Retain the current shared scheduler capture when introducing simulation delivery.
- D1: posting during a detached-batch drain must remain pending for the next
  boundary. A recursive drain must not turn that promise into inline delivery.
  Add a reentrancy guard under 1.2/1.3 and a corresponding regression scenario.

- D2: duplicate concurrent unregister calls can observe removal before the first
  remover invalidates. Every matching remover must perform invalidation before
  returning, even when it reports no new removal. Recorded under 2.3.

- D2: cancellation of an accepted target task must settle event delivery even
  when its target future is discarded. Owned delivery tickets report cancellation
  outside enqueue locks; explicit invalidation suppresses intentional discards.
  Error sinks have an independent owned-lifetime requirement. Added under 2.6.

- D3: Linux may silently restrict a successful affinity request. The native
  adapter reads back eligibility before invoking work; hard requirements reject
  unavailable CPUs/topology, and policy failure cannot silently execute a job.
  Fixed CPU_SETSIZE limits and unsupported targets are explicit capabilities.

- D4: joining CPU workers before servicing their accepted platform futures
  deadlocks shutdown. Pump the platform while waiting for simulation/CPU completion,
  then cancel remaining platform work before resource teardown. Added under 4.5.
- D4: persistent global subscriptions cannot be blindly removed by one context.
  Add AbstractGame::quiesce() to stop application producers and explicitly invalidate
  borrowed registrations while targets remain alive. Final teardown stays in deinit().

- D0/4.7: the GLFW factory also needs to forward execution configuration; constructor
  injection alone leaves the normal application bootstrap unable to select capacity.
  Added GlfwOpenGLConfig::execution and forwarded it for owned/borrowed input. Complete.

## Patch protocol

Generate a new numbered mailbox patch at every coherent checkpoint, using the
previous delivered checkpoint as its base. Never reuse a delivered number.
Keep the exact commit series. At a stopping checkpoint generate
`cheryl-engine.patch` from the original task base to the current HEAD.
Apply that cumulative patch at the original base, or apply the incremental
sequence in order; do not apply both series to the same checkout.

Delivery bases and the next unused number are recorded in the session checkpoint
outside the repository, avoiding a commit that refers to its own hash.

## Validation so far

Source/diff review and `git diff --check` completed at each checkpoint.
Dispatcher/event/worker/shutdown regression sources are prepared, not compiled
or executed. Current C++ files undergo syntax-tree inspection; type/link/native
behavior remains unverified. Mailbox replay checks source tree and author/message
ordering without compiling or running regression programs.

The timing continuation statically parsed all eleven changed C++ headers/sources
without syntax-tree errors and passed `git diff --check`. Combined and incremental
mailbox replay checks reproduce the source tree and author/date/message ordering.
These preparation checks do not establish C++ type/link correctness or runtime behavior.


## Stopping checkpoint and continuation

Completed source tasks: 0–5, including discovery addition 4.7 (factory forwarding).
Task 9 documentation/regression preparation accompanies those changes; executed
acceptance remains open. Continue at task 6 (cache context/residency and idle GPU
retirement), then tasks 7–8. Keep the established timing/input consumption contracts
when adding platform maintenance and preserve strong cache residency.

This continuation reserves numbered patches **16–20**; the next unused filename
is **cheryl-engine-21.patch**. The user has already applied/pushed checkpoints
01–15. Apply **cheryl-engine-continuation.patch** to that pushed base,
`ef4ec51402ebadd2f1319dfd03bb0ed3a0b6007d`, or apply 16–20 individually in order.
The separately retained **cheryl-engine.patch** is cumulative from the fixed
original base and includes the fifteen earlier commits again. Use that file only
for a fresh checkout at the original base. Do not combine these application routes.
Earlier local commits remain preserved on the checkpoint branch; current commits
continue from the identical user-pushed tree, with their individual authors/messages.

Task 6's initial review confirms strong cache ownership and collection only from
OpenGL clear/render. Its remaining idle-maintenance work needs a neutral operation
plus a wake or bounded wait, including loops with no published frame or a full
polling backlog. No residency or material implementation changed in this batch.

No commits/branches were pushed by the assistant. No compilation, CMake configuration, regression
execution, or real GLFW/OpenGL acceptance was performed in this continuation.
Unsupported native affinity/topology capabilities are explicit; the Linux adapter
and scheduling policies still need executed acceptance when authorized.
