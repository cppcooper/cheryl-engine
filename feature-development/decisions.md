# Storytelling decisions

Proposed for the first console dialogue milestone at source baseline
`3542f883391d4225329dab8d5d61a5987e69c8f0`. These are agent design choices,
not confirmed personal API preferences. Detailed contracts, alternatives,
provenance and acceptance are in [agent-notes.md](agent-notes.md);
[next.md](next.md) selects the next unit. No feature behavior has been implemented
or accepted by execution.

## ST-001 — Reusable dialogue, console first

Use a small CPU definition validator and explicitly advanced session, exercised
by a four-node console encounter. This meets branching and resume requirements
while keeping game policy in the application. Runtime startup, event delivery,
UI toolkits, quest generation, localization and sibling features add no required
capability here. The tradeoff is an intentionally limited facility. Confidence
is high in the scope, medium in the provisional namespace and file placement.

## ST-002 — Separate authored identity from session progress

Use authored definition/node/choice IDs and an exact definition revision, with
an application-supplied session ID and checked progression step. Saved offsets
or process identities would break when content or runtime instances change.
Manual revision bumps remain an application obligation; hashes and migration
are deferred. Confidence is high that these identities must differ, medium in
their proposed string wrappers, and low that historical code establishes the
user's preferred spelling.

## ST-003 — Retain immutable definitions and copy observations

Each move-only session retains an immutable definition and owns its cursor;
views and snapshots own their data. Borrow application handlers only during
synchronous calls on one serialized owner. This avoids draft and callback
lifetime dependencies and follows current value-lifetime contracts. Shared
ownership and copied text incur small allocation costs. Confidence is high in
the safety requirement and medium in this representation; v3 includes contrary
parent-reference patterns.

## ST-004 — Keep condition and effect meaning in the application

Choices refer to optional condition/effect keys validated against supplied
supported names. The application reads and sets its one flag through synchronous
handlers; the engine owns no world-state language or flag store. This keeps
reuse narrow and avoids treating event notifications as effect completion.
The tradeoff is application responsibility for pure predicates and correct
bindings. Confidence is high in the boundary, medium in key-based dispatch,
with only cautious historical preference support.

## ST-005 — Recheck selections and commit only after effects

Validate session, node, step, membership and current condition before applying
an effect, then commit the cursor without throwing. Stale or unavailable choices
change nothing. An effect exception faults the session and blocks saves and
retries because application changes may be partial. This avoids promising
rollback or durable exactly-once delivery. Confidence is high for the boolean
fixture, medium for general handler usability; the nonthrowing commit needs
implementation proof.

## ST-006 — Validate complete definitions before play

Compile the whole authored graph into a usable definition only after identity,
shape and reference checks succeed. Return owned, location-rich diagnostics
and ordinary selection rejection results; preserve callback/allocation
exceptions. This catches invalid hidden branches without imposing a new global
error system. Extra diagnostic values and eager setup work are the tradeoff.
Confidence is high in early validation, medium in result vocabulary and warning
policy until the focused caller exists.

## ST-007 — Save progress and application flags as one boundary

Snapshot only healthy operation boundaries, pairing logical session values
with application flags in a versioned console JSON envelope. Restore exact
content revisions without running effects. This prevents replay from a coherent
checkpoint while keeping serialization outside core headers. Fresh checkpoint
paths avoid overwrite policy; migrations, atomic replacement and external-effect
crash recovery remain deferred. Confidence is high for paired resume semantics,
medium for the console format and its unimplemented wiring.
