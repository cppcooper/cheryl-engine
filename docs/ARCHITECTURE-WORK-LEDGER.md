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
- Task 1 active: platform rename, safe submission handles, simulation delivery.
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

## Patch protocol

Generate a new numbered mailbox patch at every coherent checkpoint, using the
previous delivered checkpoint as its base. Never reuse a delivered number.
Keep the exact commit series. At a stopping checkpoint generate
`cheryl-engine.patch` from the original task base to the current HEAD.
Apply that cumulative patch at the original base, or apply the incremental
sequence in order; do not apply both series to the same checkout.

Delivery bases and the next unused number are recorded in the session checkpoint
outside the repository, avoiding a commit that refers to its own hash.
