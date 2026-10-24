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
U4's private startup-only authored input passed 23 new plus all 60 retained cases
(83/83), with 32 child-console outcomes checked. Full evidence/limits are in next.md.
U5 adds private advisory inspection under ST-013/014; all 15 new and 83 retained
cases passed (98/98), with 54 expected child statuses. Full evidence is in next.md.
These remain engineering choices, not personal taste evidence.
U6 exposes Engine inspection under ST-013/014/015; next.md distinguishes its
executed boundary checks and retained regressions. U7 preserves the caller with
ST-016 indices; all 121 checks (112 retained plus nine new) passed, with 54 unchanged
child transcripts and a verified allocation-denial probe. U8 adds ST-017/018; all
138 checks passed, including 17 new cases and all 121 retained names. Its 54
complete child transcripts remain byte-identical and public shared export executes.
U9 executes ST-019/020 with 154/154 passes (all 138 retained), five preparatory
allocation failures before effects, allocation-free result return and unchanged
54 child transcripts. Full evidence is in next.md.

## ST-001 — Reusable dialogue, console first

CPU definitions and sessions live in CE::Dialogue; a default-OFF private headless
console owns the built-in or explicitly loaded content, flag, commands and paired
adoption. It links Engine and privately includes pinned JSON headers without modules.
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

Compilation validates/deep-copies, then builds private ordinal indices (ST-016);
const shared ownership alone is insufficient. Sessions retain definitions, while views/snapshots/transition observations
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

Validate live incarnation, logical session/node/step, membership, capacity and
current condition before effects. Prepare target, borrowed keys and complete
success first; then commit numeric cursor/step and return allocation-free. Legacy
choose stays void; optional observations use ST-019/020. Guard exceptions preserve
healthy progress; effect exceptions rethrow and fault the source cursor because
application mutations may be partial. Reentrancy returns Busy. Confidence is high;
this trades rollback/retry for explicit failure boundaries.

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

## ST-011 — Compile one chosen authored definition before startup

Load an explicit --definition path before constructing Encounter, or keep the
built-in gate when omitted. Private definition_schema 1 decodes ordered owned
Draft values, then uses the existing compiler and fixed introduced/introduce
allowlist. Owned origin/field/row context and every semantic diagnostic survive
failure; no fallback or partial definition starts. Usage errors exit 2, rejected
input 3. This trades live replacement for predictable startup. Confidence is high
in separation; private format spelling remains medium-confidence and locally reversible.

## ST-012 — Bound authored decoding before freezing

Keep 64 KiB input, 256 nodes, 2048 total choices, 128-byte IDs/keys, 4096-byte
individual text, 32768-byte aggregate text and five container frames. Strict SAX
decoding rejects duplicates, coercion, excess nesting and raw NUL before compilation.
These limits bound lexer storage, draft growth and freeze copies. The byte cap
precludes 2048 complete choices; a lower-only private seam tests its defensive
guard. Confidence is high in bounds, medium in chosen text caps; changing caps is local.

## ST-013 — Inspect topology without constructing an encounter

Engine inspect returns independently owned topology in authored order using the
compiled entry and terminal kind. Every choice is an edge regardless of conditions;
cycles and terminal-free components remain valid. The console loads and formats
outside Engine, without constructing sessions or processing commands. This trades
gameplay conclusions for reusable structural facts with no callbacks. Confidence
is high in semantics, medium in public spelling; no personal API taste is inferred.

## ST-014 — Bound graph work before copying report values

Use explicit Engine node/edge, repeated-ID-byte, payload-storage and work budgets
with typed refusals. Defaults cover the console; hard ceilings permit larger CPU
definitions. Preflight counts/ID minima, checked arithmetic and exact payload
estimates bound allocation. Charge actual comparisons, graph visits and copies;
allocator exceptions propagate. This trades fixed private arrays for one budgeted
scratch allocation. Confidence is high in mechanics, medium in chosen ceilings;
tuning requires contract/test review.

## ST-015 — Keep consumer policy outside the exported analysis

Own inspection in the real Engine source glob with a normal CE_API declaration.
The console aliases its owned report, delegates traversal, and retains only legacy
cap/message checks and formatting. A separate public-header-first consumer links
Cheryl::Engine, checks returned values/refusals/lifetimes and verifies shared symbol
ownership. This trades a bounded compatibility preflight for unchanged CLI bytes
without duplicate production algorithms. Confidence is high on verified Linux;
SDK/platform ABI coverage remains deferred.


## ST-016 — Compile immutable ordinal lookup indices

Build sorted node ordinals and node-scoped choice ordinals after validation and
private deep copy. Binary search returns original authored pointers without
allocation or mutation, comparing exact strings including binary IDs. This trades
retained index memory and compile-time sorting for logarithmic comparison counts;
long prefixes still cost byte comparisons. Confidence is high in ownership and
semantics, medium in the per-node-vector layout; changing that private layout is
local, with matching Engine/consumer rebuilds.


## ST-017 — Find owned structural routes without live authority

Return owned definition/revision, ordered nodes and source-scoped choices from
iterative BFS. FIFO first discovery and authored choices resolve shortest-edge
ties; equal known endpoints have zero edges. Conditions are ignored and no session
or handler runs. This trades gameplay conclusions for structural advice with
independent lifetime. Confidence is high in semantics, medium in spelling;
changing this new result affects a small consumer without migrating saves.

## ST-018 — Bound route input, lookup reservations and output

Preflight authored counts, reserve indexed comparison/byte work, and use one
3*N scratch vector. Check path length, repeated ID bytes, payload and copy work
before result allocation; typed refusals publish nothing. Defaults cover the
console-sized graphs and hard ceilings permit larger CPU inputs. This trades
conservative work accounting for predictable bounds. Confidence is high in
mechanics, medium in ceilings; tuning requires public budget/usage and boundary
review, without persistence changes.


## ST-019 — Return owned successful observations without authority

Opt-in choose_with_transition returns owned definition/revision, logical session,
source/target, source-scoped choice and old/new steps. Rejects/exceptions return no
observation. This preserves legacy source calls and keeps receipts independent of
live owners, while charging opt-in string copies. Values grant no selection, replay,
authenticated history, durable delivery or external rollback. Confidence is high
in ownership, medium in spelling; changing this new shape affects a small consumer
without save migration.

## ST-020 — Share checks and prepare success before effects

Use two private compile-time variants of one checked choice path. Prepare the
complete selected expected result before effects; afterward commit numeric cursor
and step, then move without throwing. Legacy choose constructs no observation.
This trades two small instantiations for matching rejection/fault ordering without
wrapper preparation gaps. Confidence is high in sequencing and move guarantees,
medium in private spelling; changes require allocation, exception and authority
checks, not persistence migration.

## ST-021 — Accept manual creative proposals under host authority

Retain privately constructed, deeply copied const candidate/catalogue/context
values after bounded validation. Two explicitly manual ferry sets reference typed
host outcomes; unknown vocabulary, broken references and conflicting claims
reject before play. Prose grants no reward authority. This trades provider/format
generality for an executable creative boundary. Confidence is high in ownership,
medium in the small vocabulary; replacing the private shape is local, not evidence
of personal API taste.

## ST-022 — Project mechanics separately from flavour

Compile ordinary fixed-target definitions from accepted actions and one wording/
response flavour. Paying and repairing rejoin after distinct response nodes while
retaining different costs and relationships; decline stays ashore. Flavours change
prose without multiplying mechanical choices. This trades dynamic routing for
deterministic host-approved plans. Confidence is high in separation, medium in the
six-node presentation; topology changes need private identity/revision review.

## ST-023 — Stage facts and observer state on one owner

Keep authoritative passage/resources separate from observer knowledge, mistaken
belief and attitude. Pure eligibility rechecks precede staging and nonthrowing
state adoption inside the checked effect. Pair an idle snapshot with owned state;
validate/resume before adopting either. This trades global reputation/durable
transactions for a completed-boundary reference owner. Confidence is high in tested
sequencing, medium in observer policy; changing policy is private but affects
retained-content interpretation.
U11 reuses this state for checked watch clarification.

## ST-024 — Bound the private proof and execute its consumer

Apply lower-only count, text, payload-storage and work allowances before copies
or projection. Return one owned typed refusal; allocation exceptions propagate.
Build a default-OFF headless executable against actual Engine with finite scenarios
and literal state/transcript checks. Accounting excludes compiler internals and
allocator/RSS overhead. Confidence is high in tested boundaries, medium in caps;
changing caps requires boundary review, while play quality and provider usefulness
remain untested hypotheses.

## ST-025 — Compose a private continuation from shared fragments

Select one of three bounded recipes against an explicit completed ferry context.
Combine owned greeting, wording, response and typed consequence ingredients into
an immutable continuation and ordinary definition. Flavours preserve mechanics;
prose cannot register operations. This trades provider/general story machinery for
three useful playable combinations. Confidence is high in authority and ownership,
medium in recipe shape; changing the private library is cheap with revision review.

## ST-026 — Let evidence change knowledge while facts remain fixed

Offer fare records, Mara's testimony or an ashore explanation alongside withholding.
One host-approved clarification updates only Ivo's knowledge, belief and attitude;
a later explicit question rechecks that knowledge. All targets are frozen before
play, and Lena stays unaware. This trades social simulation for observable
continuity. Confidence is high in tested separation, medium in pacing/rules;
replacing private dialogue needs content and participant review.

## ST-027 — Retain origin and adopt only checked paired state

Validate a terminal ferry snapshot and compatible world context before constructing
a separately owned watch encounter. Retain source meaning independently of producers.
Resume a healthy idle watch snapshot with coherent state before nonthrowing pair
adoption; failures preserve prior values/tokens. This trades authenticated history
and durable transactions for an explicit serialized handoff. Confidence is high
in checked ownership, medium in strict context policy; reversal requires private
context/resume review.

## ST-028 — Bound composition and verify the real continuation

Limit fragment/recipe/candidate counts, text, requested payload, work and the five-node,
four-choice graph before copies. Validate all ingredients, then scan at most three
recipes without regeneration. Normal allocation exceptions propagate; compiler
internals and previously owned ferry storage remain separate allowances. Execute
the actual private console and retained regressions. Confidence is high in tested
boundaries, medium in caps; expansion requires resource review, with fun and wider
platforms still unverified.

## ST-029 — Compose a finite bargain with substantive decisions

Combine shared manual request, intent, information, response and approved-meaning
ingredients into eight acyclic nodes. Request, terms and proposal each offer
meaningful alternatives; learning terms enables a smaller promise. Ramp/shelter
rules change mechanics while flavours change prose only. This trades a planner
for explicit reusable composition. Confidence is high in tested agency, medium
in pacing/shape; private changes require identity/revision review, not inferred
personal preference.

## ST-030 — Record obligations without granting fulfillment

Stage bounded resources, learned terms, resolution, pending plank quantity and
requester observation/attitude on one serialized owner. Immediate help, payment,
ordinary/negotiated promises and refusal remain distinct after closing. Promises
grant no fulfillment reward; earlier observers/facts stay separate. This trades
social simulation for inspectable consequences. Confidence is high in tested
sequencing, medium in costs/reactions; later payoff requires its own selected
contract, not callback routing or silent completion.

## ST-031 — Own meaning and resume completed boundaries

Deep-copy sets, catalogue, pick and context into a privately constructed const
artifact retaining its frozen definition and completed watch origin. Validate
location and source state; resume paired bargain values before nonthrowing adoption.
Failures preserve existing artifacts/tokens. This trades authenticated/durable
history for a small owned handoff. Confidence is high in exercised ownership,
medium in strict source policy; reversal needs private context/identity checks
without changing existing snapshot formats.

## ST-032 — Bound explicit composition and prove the real caller

Apply lower-only ingredient, candidate, text, requested-payload, work and graph
allowances before growth or charged operations. Return owned typed refusals;
allocation exceptions propagate. The actual headless consumer and retained tests
exercise literal state and output. Accounting excludes compiler, allocator/transient
overhead and previous shared storage. Confidence is high in tested boundaries,
medium in caps; expansion needs resource review. Seeded selection, fulfillment and
participant fun remain outside this unit.

## ST-033 — Recombine approved ingredients under useful constraints

Enumerate request, independently approved plan, offer bundle and flavour dimensions;
filter typed goal/role/location compatibility and initial availability. Cost/pending
ceilings and required initial intentions constrain actual play, while an explain-first
bundle changes decision placement. This trades a solver for a small inspectable
family. Confidence is high in exercised compatibility, medium in creative range;
expansion requires private policy/identity review, not an inferred user preference.

## ST-034 — Version seed replay and retain exact selected meaning

Use version-one SplitMix64 first output modulo the completely enumerated compatible
rows, in authored dimension order. Own the exact accepted artifact and selected
IDs/revisions/descriptor; seeded definition identities also distinguish plans.
This trades uniformity over meanings and content authentication for bounded same-input
replay. Confidence is high in literal seeds/ownership, medium in long-term recipe
stability; policy changes require version/revision review and retained artifacts.

## ST-035 — Compare finite mechanics separately from presentation

Return owned structured costs, obligations, resolutions, observation/attitude and
ordinal beat/action/condition/effect relationships separately from displayed text
and IDs. Cosmetic renames and paraphrases compare mechanically equal; cost and
offer-structure changes differ. This trades general graph equivalence for the
validated eight-beat family. Confidence is high in tested normalization, medium
in future sufficiency; adding mechanics requires updating this private comparison
and its independent expectations.

## ST-036 — Charge bounded selection and execute the real consumer

Check capped Cartesian products before storage, charge every rejected/accepted row,
reserve nested composition/scratch allowances and charge the single draw. Distinguish
invalid input, incomplete resource refusal and proven empty compatibility. Ordinary
allocation exceptions propagate. This trades tight resource estimates for conservative
quotas with explicit exclusions. Confidence is high in exact boundary/child checks,
medium in caps; broader platforms, leaks, participant fun and durable history remain
unverified, with U14 selected separately.
