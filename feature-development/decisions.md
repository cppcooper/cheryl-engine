# Storytelling decisions

Definition choices implemented in U1 from design checkpoint
`716ec60984e6764a03ed375ac37826e5d92e6978`; U2 implements sessions and value resume.
These are agent engineering choices,
not confirmed personal API preferences. Detailed contracts, alternatives,
provenance and acceptance are in [agent-notes.md](agent-notes.md);
[next.md](next.md) records executed checks and the next action. U1V completed definition
acceptance with all 16 focused Debug ASan/UBSan cases passing; leak coverage is
unavailable under ST-008. U2 passed 18 session cases, the 16 definition regressions
and its isolated allocation probe. U3's 26 new cases and all 34 regressions passed
(60/60), including ten actual child consoles. Its evidence/limits are in next.md.
No new preference evidence is inferred.

## ST-001 — Reusable dialogue, console first

CPU definitions and sessions live in CE::Dialogue; a default-OFF private headless
console now owns the concrete gate, flag, commands and paired adoption. It links
Engine and privately includes pinned JSON headers, without optional modules.
Numbered presentations select only issued tokens and reject old commands after
progress/resume. This trades a second command argument for visible scope.
Confidence is high in separation, medium in command spelling; changing commands
is local and establishes no personal API taste.

## ST-002 — Separate authored identity from session progress

Keep authored IDs/revision separate from logical SessionId and checked uint64
progress. Selection additionally retains a private shared incarnation: start/resume
allocate a fresh marker, moves preserve it, snapshots omit it. This rejects old
commands after identical-field rewind without a global ID service or address reuse
while tokens survive. The tradeoff is one retained marker allocation per
incarnation. Confidence is high in identity separation, medium in API spelling;
global revision/identity discipline remains application-owned.

## ST-003 — Freeze definitions and retain their owners

Compilation validates and deep-copies private frozen storage; const shared
ownership alone is insufficient. Sessions retain definitions, while views/snapshots
own strings and tokens retain only a small marker. Handlers are borrowed
synchronously. Idle moves are nonthrowing for paired adoption; active
moves/replacement/destruction terminate to prevent callback invalidation. This
trades recoverable lifecycle misuse for a firm storage boundary. Confidence is
high in ownership semantics, medium in that lifecycle policy; caller serialization
remains required.

## ST-004 — Keep condition and effect meaning in the application

Definition compilation checks optional keys against independent supported-name
lists and accepts no executable application code. Session operations borrow
std::function handlers synchronously and pass stable keys by const reference;
missing handlers reject when needed. Applications own flag meaning and pure
predicates, with no engine flag store or serialized callable. The tradeoff is
caller-managed captures and possible callable-conversion allocation before entry.
Confidence is high in this boundary, medium in callable spelling; historical
preference evidence remains limited.

## ST-005 — Recheck selections and commit only after effects

Check live incarnation, logical session/node/step, membership and current condition
before effects. Prepare target, borrowed keys and expected<void,SessionError>
success first; afterward commit only numeric cursor/step and return without
allocation. Guard exceptions preserve healthy progress; effect exceptions rethrow
and fault the source cursor because application state may be partial. Reentrancy
returns Busy. This trades rollback/retry for an explicit failure boundary.
Confidence is high in the control flow; native cases and the isolated allocation
probe establish its focused evidence.

## ST-006 — Validate complete definitions before play

Validate every authored row, including unreachable errors, before constructing a
usable definition. A dialogue-local std::expected returns either a retained
definition or owned diagnostics with code, authored positions, field and value.
Deterministic traversal makes multiple errors actionable; cycles are valid and
reachability warnings are excluded. Allocation failures remain exceptions. The
tradeoff is eager validation and diagnostic allocation. Confidence is high from
source review and 16 passing native cases; leak checking remains unavailable (ST-008).

## ST-007 — Save progress and application flags as one boundary

Capture idle snapshot and introduced flag on one serialized Encounter owner. Decode
fresh values, then resume against its retained definition before nonthrowing idle
session/boolean adoption. Failed loads preserve both values and live tokens;
successful loads renew authority and discard presented choices. Counts remain
unsaved test instrumentation. This avoids replay from a coherent pair, without
authenticating edited history or guaranteeing external effects across rollback.
Confidence is high in ownership; schema changes require versioning, adapter changes
remain local.

## ST-008 — Bound the native verification accommodation

Use invocation-only ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1 for this traced native runner. Unadjusted discovery
reproduced LeakSanitizer's fatal ptrace limitation; adjusted focused checks run
with ASan/UBSan instrumentation intact. This trades unavailable leak
coverage for executable definition checks, without changing global settings or
build flags. Confidence is high from this clone's captured diagnostics. Remove
only detect_leaks=0 on an LSan-compatible executor; leak detection is unverified.

## ST-009 — Decode a bounded complete envelope strictly

Use pinned nlohmann SAX parsing with two fixed object frames, decoded-key duplicate
bits and exact field/type/schema checks. Scan every byte for raw NUL first; read
at most 64 KiB plus one detection byte. Strict parsing rejects trailing documents.
This trades a small fixture-specific decoder for bounded nesting and no silent
coercion/replacement. Confidence is high in the boundary, medium in format longevity;
replacing the private codec is cheap before external saves accumulate.

## ST-010 — Save exclusively to a fresh chosen path

Create the destination with exclusive C11 fopen x mode, then check write, flush
and close. Track ownership only after create; remove incomplete owned output,
reporting path and cleanup failure if removal fails. Existing files/leaf links
remain untouched. This authorized narrowing replaces temporary publication and
trades overwrite convenience for a small auditable boundary. Confidence is high
on verified Linux, portability remains unverified; replacement/durability would
require a separately designed policy.
