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
  adapters without EventBus header dependencies. Other task 4 work and 5–8 pending.
- Tasks 2–8 pending. Documentation and regression sources (task 9) accompany each change.

## Discovery additions

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
Dispatcher regression sources are prepared, not compiled or executed.
