# Architecture convergence review

Checkpoint 95 adds current native evidence: real Mesa llvmpipe context/lifetime
checks and eight finite sequential/concurrent demo runs pass. Full normal/sandbox
suites now pass 334/330 cases without skips under requested native execution.
Checkpoint 96 records the user's desktop checklist pass: input/text focus, resize,
successful/failed/recovered reload and normal window close. Further timing/native/
font/OS acceptance remains open;
[ARCHITECTURE-VALIDATION.md](ARCHITECTURE-VALIDATION.md) and
[NATIVE-DESKTOP-CHECKS.md](NATIVE-DESKTOP-CHECKS.md) record scope and reported results.

1 October 2026. Source preparation for tasks 1–8 is implemented. The explicit
completed-task source audit closes at checkpoint 90, including batch-ending claims
and identified startup/update/reload/shutdown dependencies. The latest confirmed
remote remains checkpoint 57, `5e4adfdc94cec7590916282c876762ada4d0e123`; pending
58–90 preserve the fixed original base and ordered history. Source closure is not
exhaustive correctness or executed acceptance. Task-9 source work is unblocked.
[ARCHITECTURE-SUBTASK-AUDIT.md](ARCHITECTURE-SUBTASK-AUDIT.md) records all requirement
rows; findings A1–A20 and remaining 9.5/9.7 gates are explicit.

Task-9 execution now builds both Release configurations and passes all 332 normal
and 330 sandbox aggregate tests, with no skips. Checkpoint 91 corrects two startup
fixture cancellation expectations without changing production behavior.
[ARCHITECTURE-VALIDATION.md](ARCHITECTURE-VALIDATION.md) records current evidence and
remaining native gates. Confirmed remote 90 is `a785c0e35e97a904df67b19af0d65a17d5ee1170`.
Preparation-only descriptions below are retained as audit history.

## Cross-task paths

| Path | Source review and prepared evidence | Remaining acceptance |
| --- | --- | --- |
| Startup and execution ownership | Context lazy/shared pool ownership, factory forwarding, dispatcher owner binding, initialization rollback, and worker construction cleanup reviewed. Aggregate `core/runtime-adapter.cpp` and `core/worker-pool.cpp` contain lifecycle scenarios. | Compile both configurations and execute partial startup failures. 62/81/82 now prepare isolated and composed controlled startup rejection. |
| Update, input, and frame handoff | Scheduler bounds, observation/simulation clocks, mailbox-before-backlog ordering, bounded batches, frame slot transitions, stop/join, and platform-only recycling reviewed. Scheduler/input/frame regression sources use explicit clocks or coordinated handoffs. | Execute both runtime modes and timing policies, including expensive callbacks and presentation pacing. |
| Event closure and delivery | Registry/invocation/posting lock order, ticket failure ownership, dispatcher cancellation, worker stream acceptance, and independent streams reviewed. A7 fixes initial submission serialization; A10 fixes early accepted-pump cancellation ownership. Checkpoints 56–57 prepare controlled before/after-publication loss and throwing-submit scenarios. | Execute concurrent lifecycle and rejection scenarios. 56–58/62–63 prepare controlled publication/rejection and concurrent payload scenarios; every source remains unexecuted. |
| Worker scheduling and CPU policy | Share selection, caps, capture release before completion, close/drain/join, required-mask readback, preferred fallback, and unsupported topology reviewed. Checkpoint 50 adds a mask changed by a previous job; 52 adds capture-deleter reentry and overlapping weighted CPU masks. | Execute affinity success/failure on supported hosts, live restriction changes, and overlapping affinity workloads. 62/79/81 prepare controlled native-boundary failures; actual OS acceptance remains open. |
| Cache publication and reload | Candidate construction outside locks, strong generations, replacement/clear, and provider domain reviewed. A8 retains a candidate owner through fallible insertion; generic-cache fixtures cover duplicate reentry and hash rejection. | Execute cache/material tests and controlled allocation/rehash failure. 67/70/74/77 prepare scoped real allocation rejection, with allocator ownership and partial-constructor unwinding; every fixture remains unexecuted. |
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
`02912f09341950be00dfc142203c8cf67bddf0c9`. At that recovery checkpoint the remote was still at checkpoint 45,
`7f042e9d91e28a9addb8d66284e9cdb3ff6938fd`; saved individual patches 46–51 restore the same source tree there.
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
The recovered series through 54 is now pushed; current work uses patches 55 onward.
Existing delivered numbers remain unchanged.

## Preparation checks and open actions

Changed C++ files receive syntax-tree inspection, whitespace, include/API, and
ownership/lock review. The recording fixture's GLAD calling-convention token is
normalized only in the parser input. Formatting is checked against the repository
root style using clang-format 23.1.2 over all 104 surviving task-touched C++ files.
The declaration inventory covers 153 classes/structs with no remaining late fields. Syntax parsing does not type-check,
link, execute code, or prove race freedom.

Before delivery, combined continuation, ordered numbered patches, and the
original-base cumulative mailbox must replay to the exact source tree while
preserving author/date/message order. Patch application is a packaging check.
No CMake configuration, build, regression execution, real-context acceptance, or
assistant remote write occurs in this continuation.

Task 9.5 remains open for explicitly requested execution and recorded results.
Task 9.7 remains open for separately requested PR metadata publication; a concrete
[PR description draft](PR-9-DESCRIPTION-DRAFT.md) is prepared locally. Review
findings A1–A20 and their evidentiary limits remain in the
[earlier-task audit](ARCHITECTURE-EARLIER-TASK-AUDIT.md).
