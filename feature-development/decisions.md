# Storytelling decisions

Definition choices implemented in U1 from design checkpoint
`716ec60984e6764a03ed375ac37826e5d92e6978`; session/save choices remain proposed.
These are agent engineering choices,
not confirmed personal API preferences. Detailed contracts, alternatives,
provenance and acceptance are in [agent-notes.md](agent-notes.md);
[next.md](next.md) records U1 checks and the next unit. U1V completed definition
acceptance with all 16 focused Debug ASan/UBSan cases passing; leak coverage is
unavailable under ST-008. The console/session milestone remains proposed.

## ST-001 — Reusable dialogue, console first

U1 adds CPU definition compilation in CE::Dialogue; the four-node gate is test
data. Sessions and the console consumer remain later units. This keeps game
policy outside reusable validation and requires no runtime, UI, JSON or sibling
feature in the header. The tradeoff is an intentionally limited facility within
the existing Engine target. Confidence is high in this boundary, medium in the
interface spelling; current source compatibility does not establish personal taste.

## ST-002 — Separate authored identity from session progress

Use distinct owned string types for definition, node, choice, condition and
effect identities, with a positive authored revision. Each draft represents one
definition; node names are unique there and choice names within a node. Equal
names across domains are valid. Global duplicate-definition detection and
revision discipline remain application responsibilities, trading registry policy
for independent compilation. Confidence is high in domain separation, medium in
the wrappers. Session identities and steps remain deferred.

## ST-003 — Freeze definitions and retain their owners

Compilation borrows stable input, then deep-copies it into a privately constructed
const Definition; even std::move cannot transfer caller aliases into frozen
storage. Definition copying/moving is disabled, handles retain it, and ordered
observations borrow its lifetime. This closes the ownership gap left by
shared_ptr<const> alone. Copies cost allocation; linear lookup avoids premature
index machinery. Confidence is high from the native ownership checks; leak
coverage remains unavailable (ST-008). Session ownership remains a later choice.

## ST-004 — Keep condition and effect meaning in the application

Choices contain optional condition/effect keys, checked against separate supplied
name lists. U1 rejects empty or repeated names within each list and stores only
used names in copied choices. It accepts no executable application policy, so
validation cannot dispatch callbacks. Runtime meaning remains application-owned.
The tradeoff is later responsibility for correct bindings and pure predicates.
Confidence is high in this boundary, medium in eventual dispatch spelling;
historical preference evidence remains limited.

## ST-005 — Recheck selections and commit only after effects

Validate session, node, step, membership and current condition before applying
an effect, then commit the cursor without throwing. Stale or unavailable choices
change nothing. An effect exception faults the session and blocks saves and
retries because application changes may be partial. This avoids promising
rollback or durable exactly-once delivery. Confidence is high for the boolean
fixture, medium for general handler usability; the nonthrowing commit needs
implementation proof.

## ST-006 — Validate complete definitions before play

Validate every authored row, including unreachable errors, before constructing a
usable definition. A dialogue-local std::expected returns either a retained
definition or owned diagnostics with code, authored positions, field and value.
Deterministic traversal makes multiple errors actionable; cycles are valid and
reachability warnings are excluded. Allocation failures remain exceptions. The
tradeoff is eager validation and diagnostic allocation. Confidence is high from
source review and 16 passing native cases; leak checking remains unavailable (ST-008).

## ST-007 — Save progress and application flags as one boundary

Snapshot only healthy operation boundaries, pairing logical session values
with application flags in a versioned console JSON envelope. Restore exact
content revisions without running effects. This prevents replay from a coherent
checkpoint while keeping serialization outside core headers. Fresh checkpoint
paths avoid overwrite policy; migrations, atomic replacement and external-effect
crash recovery remain deferred. Confidence is high for paired resume semantics,
medium for the console format and its unimplemented wiring.

## ST-008 — Bound the native verification accommodation

Use invocation-only ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1 for this traced native runner. Unadjusted discovery
reproduced LeakSanitizer's fatal ptrace limitation; the adjusted build and all 16
cases pass with ASan/UBSan instrumentation intact. This trades unavailable leak
coverage for executable definition checks, without changing global settings or
build flags. Confidence is high from this clone's captured diagnostics. Remove
only detect_leaks=0 on an LSan-compatible executor; leak detection is unverified.
