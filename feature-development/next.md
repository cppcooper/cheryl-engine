# Next unit: U1 — validated CPU dialogue definitions

This is the single selected next implementation unit, awaiting a subsequent
assignment. The current task stops after design records and their local commit.
Contracts and evidence: [agent-notes.md](agent-notes.md); compact choices:
[decisions.md](decisions.md), ST-001/002/003/004/006. ST-005/007 constrain what U1
must leave possible but their session/save implementation belongs to later units.

Implement the smallest definition layer in the proposed
`projects/engine/include/cheryl/core/game-framework/dialogue.h` and source
counterpart, with focused cases under `projects/engine/tests/unit/src/`.
Recheck status/diffs and applicable instructions, including the implementation
guide and any newly added nested instructions, before source edits. Preserve any
new work. Keep definitions CPU-only and JSON-free; reuse the existing unit suite
registration rather than creating another test runner.

Deliver owned draft and distinct ID/operation-key values, node/choice shape,
owned diagnostics, and compilation into a retained immutable definition.
Validate entry, identities, authored ordering, target references, supported keys
and terminal/prompt shape without calling application handlers. The four-node
gate fixture is test data in this unit. Do not add live sessions, callbacks,
serialization, console commands, runtime integration or sibling dependencies.

Required checks for this unit, when assigned and permitted:

- Valid fixture compiles, preserves authored order/text and resolves stable IDs;
  caller draft mutation/destruction cannot change the compiled definition.
- Empty/duplicate identities, zero revision, missing entry/target, duplicate
  choice IDs within a node, unknown condition/effect keys and invalid node shape
  reject with code, location and offending value. Check an unreachable bad node
  too; no usable partial definition is returned.
- Same choice name in different nodes is valid, condition/effect domains remain
  distinct, and a valid explicit-choice cycle is accepted. Diagnostics survive
  draft destruction and appear in deterministic authored order.
- Review public includes for runtime/UI/JSON dependencies. Inspect the selected
  unit runner and existing build configuration before any compilation; execute
  only the focused definition cases if a later assignment authorizes native
  checks. No build, dependency fetch or native execution is authorized by this
  design handoff itself.

Gate before U2: review a tiny compile-and-inspect caller and its actual check
results; reconcile these records if ownership, identity or diagnostics change.
Then record the session unit as the next action in a subsequent task. Do not
advance automatically. Current acceptance remains proposed and unexecuted.
