# Architecture implementation ledger

Original task base: `e8c9788f63cf4688e144b84feeb8f9aabf62540f`.
This base remains fixed for the continuing work, including later sessions.

The numbered tasks and discovery boundaries are in
[ARCHITECTURE-WORK-ORDER.md](ARCHITECTURE-WORK-ORDER.md).
Local commits use `cppcooper <cppcooper@users.noreply.github.com>`.
Compilation, test execution, and remote writes have not been requested.

## Progress

The entries below record implemented source scope. Earlier uses of "complete"
are not audit sign-off or correctness evidence. The review of tasks 0–6 is partial;
see [ARCHITECTURE-EARLIER-TASK-AUDIT.md](ARCHITECTURE-EARLIER-TASK-AUDIT.md).

- Task 0 baseline established: no applicable AGENTS.md was found; repository
  identity and original base are recorded. Later user-pushed checkpoints are
  reconciled by tree identity before continuing from their current remote head.
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
- Task 7 implemented in source through checkpoint 43: typed definitions, explicit
  native builders/reflection, copied parameter ownership, independent image units,
  full blend/depth/cull state, pass constraints, geometry/domain validation, immutable
  recipe publication/reload, and demo/frame consumers. Acceptance remains unexecuted.
- Task 8 implemented in source through checkpoint 44: resolved packets and retained
  generations/resources, CPU sprite/tile/graphic/text submission, const Font layout,
  typed FFont banks, authored ordering, reusable frame storage, low-level playback,
  and retirement of immediate Graphic/Tile/Font and Draw2D/iDraw/DrawInfo APIs.
- Task 6 complete in source: AssetCacheContext preserves the global loading domain
  and strong residency. Resource ownership is traced through caches/composites/frames
  to move-only native registrations. Both modes service maintenance independently
  of drawing with a 10 ms idle-wait bound, including full backlogs/no first frame
  and accepted-work shutdown. Program adoption/fallback deletion now preserve a
  single owner and actual-context checks. Recording-native, maintenance/failure,
  strong-retention, provider-rebind, and existing reentrant-cache scenarios are
  prepared; compilation and native acceptance remain unexecuted.
- Documentation and prepared regression sources (task 9) accompany each change.

## Discovery additions

- D0: native checkpoints 31–34 are pushed as
  `8f796148e64d3dde6faf29b9333893133eb9cceb`. Its tree matches saved local
  `30c3f5e`; that local series remains on checkpoint branches. Audit delivery
  begins at 35 from the pushed head without changing the original task base.
- D2 / audit A1: a concurrent close could return while another closer or remover
  had detached active listeners. Invalidate under the registry lock before
  removal, retaining callback owners until after unlocking. Fixed in 35.
- D5 / audit A2: polling's max-minus-timestamp span could overflow for a negative
  clock timestamp. Compare against max-minus-spacing instead. Fixed in 36;
  boundary fixtures are prepared and negative-epoch arithmetic reviewed.
- D3 / audit A3: parallel cap isolation and CPU-mask restoration lacked direct
  integration fixtures. Two prepared scenarios were added in 37; native failure
  and startup paths still need further review and execution.

- D0: pipeline foundations 26–30 are pushed as `92eb058254dc195afd11559be0357a3a265b0646`.
  Its tree matches saved local `0acf9c7`. The prior local foundation series remains
  on a checkpoint branch. New numbered delivery starts at 31 from the pushed base.
- D7: shader deletion while attached only marks the stage for deletion. Linking
  does not detach it. Detach every marked stage after linking so the retained
  program does not retain compilation resources; program failure cleanup handles
  stages when an earlier exception prevents reaching the link boundary. Fixed in 7.2.

- D0: residency checkpoints 21–25 are pushed as `87ec70e10d0c3f2498f38fbd00a7194766410547`.
  Its tree matches saved local `a677444`. The earlier local residency series remains
  on its checkpoint branch. Continue from the pushed remote base; numbered delivery
  starts at 26 and the original task base remains unchanged.
- D7: optional custom values cannot mean "skip a native write" when a uniform is
  active, because that would retain another draw's data. Add explicit native builder
  default/reset validation under 7.2a/7.3a before integrating the value resolver.
  Generic contract resolution intentionally represents missing optional values as absent.
- D7: the shader2d sprite and alpha-atlas font use the same six roles, while the
  prepared two-image effect uses time/color/intensity and distinct sampler units.
  Native mappings must be supplied explicitly; old custom programs must not inherit
  the sprite schema. Fixed-state/pass constraints and recipe publication remain open.

- D6: the shader-link guard retained a native ID after lifetime registration.
  Later logical-program allocation failure could cause both immediate deletion
  and deferred retirement. Transfer native ownership to an OpenGLHandle before
  constructing GLSLProgram. Texture/VAO/program/stage fallback deletion also needs
  owner/current-context verification; unavailable contexts leave cleanup to destruction.

- D0: timing checkpoints 16–20 are now pushed as `14d5a818386582a0c22332cbfdb5161135e92093`.
  Its tree matches saved local `ba18b50`. Continue from the remote; preserve the
  earlier timing commits on their checkpoint branch. New incremental delivery starts
  at this pushed base, while the original task base remains unchanged.

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

- D0: user-pushed checkpoints 39–45 are `7f042e9d91e28a9addb8d66284e9cdb3ff6938fd`.
  Their tree matches local `e0c6bfbb34aed5a3cdfdf6360471cee0786aa39c`.
  That local series remains on its checkpoint branch; continuation starts at 46.
- D2 / audit A7: a worker stream advertised acceptance before fallible initial pump
  submission completed. Serialize submission/publication and release rejected
  captures outside the stream lock. Fixed in 46; native fault execution stays open.
- D6 / audit A8: fallible cache insertion could destroy its last moved candidate
  owner while holding the write lock. Keep the local strong owner until unlocking.
  Fixed in 47; duplicate/hash-rejection sources and ownership review are prepared.
- D6 / audit A9: static texture unbinding bypassed owner/context guards and used an
  inherited active unit. Use guarded instance unbind(unit). Fixed in 48 with
  recording sources; no real-context acceptance is claimed.

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

This 40-minute continuation starts at user-pushed
`7f042e9d91e28a9addb8d66284e9cdb3ff6938fd`. Its tree matches saved local 39–45.
The original task base remains unchanged; the partial audit remains open.

- 46 fixes worker event pump publication and closure ordering (A7).
- 47 preserves cache candidate ownership through fallible publication (A8).
- 48 guards texture unbinding and selects its explicit unit (A9).
- 49 prepares retained frame/reload and preparation/render failure cleanup in both modes.
- 50 prepares required CPU-mask revalidation after a previous job changes native state.
- 51 updates source-convergence evidence, acceptance gates, and a local PR description draft.

Numbered patches **46–51** continue from the pushed base; the next unused filename
is **cheryl-engine-52.patch**. Apply `cheryl-engine-continuation.patch` there, or apply
46–51 individually in order. `cheryl-engine.patch` starts at the fixed original base
and repeats earlier commits. Use one application route. Commit order/authorship
remains intact.

Tasks 1–8 and task-9 source preparation are implemented. Compilation, aggregate
regression execution, real sequential/concurrent rendering/affinity/reload/retirement,
and shutdown/failure acceptance remain open under 9.5. PR metadata publication
remains a separately requested action under 9.7; a concrete local draft is ready.
The partial audit and controlled failure-coverage gaps remain explicit in
[ARCHITECTURE-CONVERGENCE-REVIEW.md](ARCHITECTURE-CONVERGENCE-REVIEW.md).

Changed C++ syntax trees and whitespace are checked alongside include/API/ownership
review; formatting follows the repository style manually. All three mailbox replay
routes must reproduce the exact tree and ordered author/date/messages before
final delivery. No configuration, compilation, tests, native acceptance, or assistant
remote write occurs in this continuation.
