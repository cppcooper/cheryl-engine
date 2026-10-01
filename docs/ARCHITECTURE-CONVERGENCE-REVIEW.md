# Architecture convergence review

1 October 2026. Source preparation for tasks 1–8 is implemented. This checkpoint
reviews their shared startup, update, reload, and shutdown paths; it does not close
the partial audit or establish executed acceptance. The continuation starts from
the user-pushed `7f042e9d91e28a9addb8d66284e9cdb3ff6938fd`, whose tree matches the
previously delivered 39–45 series. The fixed original task base is unchanged.

## Cross-task paths

| Path | Source review and prepared evidence | Remaining acceptance |
| --- | --- | --- |
| Startup and execution ownership | Context lazy/shared pool ownership, factory forwarding, dispatcher owner binding, initialization rollback, and worker construction cleanup reviewed. Aggregate `core/runtime-adapter.cpp` and `core/worker-pool.cpp` contain lifecycle scenarios. | Compile both configurations; execute partial startup failures. Controlled partial thread-start failure is not covered by a success-path fixture. |
| Update, input, and frame handoff | Scheduler bounds, observation/simulation clocks, mailbox-before-backlog ordering, bounded batches, frame slot transitions, stop/join, and platform-only recycling reviewed. Scheduler/input/frame regression sources use explicit clocks or coordinated handoffs. | Execute both runtime modes and timing policies, including expensive callbacks and presentation pacing. |
| Event closure and delivery | Registry/invocation/posting lock order, ticket failure ownership, dispatcher cancellation, worker stream acceptance, and independent streams reviewed. A7 fixes first-pump publication; event sources prepare closure/reentry scenarios. | Execute concurrent lifecycle and rejection scenarios. Existing fixtures do not force the old fallible pump-publication window. |
| Worker scheduling and CPU policy | Share selection, caps, capture release before completion, close/drain/join, required-mask readback, preferred fallback, and unsupported topology reviewed. Checkpoint 50 adds a mask changed by a previous job; 52 adds capture-deleter reentry and overlapping weighted CPU masks. | Execute affinity success/failure on supported hosts, live restriction changes, and overlapping affinity workloads. Native failure injection remains open. |
| Cache publication and reload | Candidate construction outside locks, strong generations, replacement/clear, and provider domain reviewed. A8 retains a candidate owner through fallible insertion; generic-cache fixtures cover duplicate reentry and hash rejection. | Execute cache/material tests and controlled allocation/rehash failure. Hash rejection does not reproduce every allocation failure. |
| Native construction and binding | Texture/program/stage/geometry ownership, lifetime adoption/retirement, actual-context guards, reflection, parameter/resource preflight, and complete draw state reviewed. A9 guards explicit-unit texture unbinding. Recording GLAD fixtures use synthetic IDs. Checkpoint 53 adds failed-shutdown recovery and abandonment with pending/live handles of all kinds. | Execute real-context construction/reload/draw, context loss, idle collection, and failure cleanup. Recording calls cannot prove driver behavior. |
| Retained frame generation and cleanup | Checkpoint 49 prepares successful material replacement, preparation failure after packet insertion, and render failure in both modes. Old geometry/material/pipeline/image handles remain in packets until platform-thread recycling, before game cleanup. Original failure must survive a later cleanup failure. | Compile and execute all six scenarios; exercise the corresponding real renderer paths. |
| Shutdown dependency ordering | Simulation closes on its owner before join; game quiesces while targets exist; platform pumps accepted CPU dependencies; workers finish before platform closure, frame recycling, game cleanup, and renderer teardown. Existing runtime fixtures cover accepted work and cleanup failure. | Execute producer/failure combinations. Quiesce must not block on a dependency requiring platform service. |

Tests remain in the existing aggregate target: its recursive GTEST_SOURCES discovery
includes these files without adding executables. Held callbacks/jobs and promises
establish ordering; bounded waits support failure cleanup, not race proof.

## Recovery and additional coverage

The resumed workspace contained checkpoint 30 even though checkpoints 46–51 had
been saved. Recovery materialized the saved cumulative mailbox and replayed all
51 commits from the original base into a separate checkout. Its exact tree matched
`02912f09341950be00dfc142203c8cf67bddf0c9`. The remote remains at the checkpoint-45
base above; saved individual patches 46–51 restore the same source tree there.
Old local and recovered histories are retained on checkpoint branches. This is
recovery evidence; it does not identify why workspace or UI state diverged.

Checkpoint 52 prepares capture destruction which inspects/posts to another group
before source drainage returns, plus actual-mask and 3:1 scheduling checks for
overlapping eligible sets on one worker. Held-worker fixtures now release and
join before their captured recording state can unwind on setup failure.
Checkpoint 53 prepares missing-context shutdown followed by successful recovery,
and failed recovery followed by abandonment and late worker release. Both pending
and live registrations are covered; synthetic native IDs cannot prove driver cleanup.
These four additional scenarios are uncompiled and unexecuted. No additional
production defect was found in the worker/native paths reviewed during recovery.

The work window is 20 minutes with up to 10 minutes reserved for a clean checkpoint.
New work uses patches 52 onward; existing delivered numbers are retained.

## Preparation checks and open actions

Changed C++ files receive syntax-tree inspection, whitespace, include/API, and
ownership/lock review. The recording fixture's GLAD calling-convention token is
normalized only in the parser input. Formatting is checked against the repository
style manually; clang-format is unavailable. Syntax parsing does not type-check,
link, execute code, or prove race freedom.

Before delivery, combined continuation, ordered numbered patches, and the
original-base cumulative mailbox must replay to the exact source tree while
preserving author/date/message order. Patch application is a packaging check.
No CMake configuration, build, regression execution, real-context acceptance, or
assistant remote write occurs in this continuation.

Task 9.5 remains open for explicitly requested execution and recorded results.
Task 9.7 remains open for separately requested PR metadata publication; a concrete
[PR description draft](PR-9-DESCRIPTION-DRAFT.md) is prepared locally. Review
findings A1–A9 and their evidentiary limits remain in the
[earlier-task audit](ARCHITECTURE-EARLIER-TASK-AUDIT.md).
