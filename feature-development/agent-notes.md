# Storytelling: first dialogue milestone

U1 implements validated immutable CPU definitions on `feat/storytelling`, from
clean design checkpoint `716ec60984e6764a03ed375ac37826e5d92e6978` (original source
baseline `3542f883391d4225329dab8d5d61a5987e69c8f0`). The definition implementation
and 16 unit cases pass syntax checks; native acceptance is blocked by the missing
fallback font described in [next.md](next.md). Sessions and the console encounter
remain proposed. The companion
[decisions](decisions.md) use the same `ST-001` through `ST-007` IDs;
[next](next.md) selects one subsequent implementation unit.

## Authority, scope and evidence limits

The coordinator's STORYTELLING-U1 assignment authorizes the definition source,
focused checks and updates to these internal records, superseding the earlier
design-only task. The parent `AGENTS.md`,
`coordination/autonomy-policy.md`, `coordination/workers.json` and feature brief
were read from `/mnt/projects/cheryl-features/`. Repository workflow,
investigation, architecture, testing, Git and documentation instructions were
also read, including `docs/AGENTS.md`; the nested-instruction search found only
the root and documentation instruction files. No pre-existing changes or
feature-development records were present at the original design baseline. U1
started with the design records committed and a clean tree. This clone alone is
owned; protected checkouts and sibling clones are outside the work.

**ST-001 — Small reusable boundary.** U1 implements CPU dialogue definition
validation; an explicitly advanced session and console adapter remain later units.
The definition lives in `CE::Dialogue`, under
`projects/engine/include/cheryl/core/game-framework/dialogue.h` and its source
counterpart. The public contract needs no `AbstractGame`, runtime context,
renderer, UI toolkit, event bus, clock, input service or file index. Initial
engine linkage may still carry existing engine dependencies; an independently
packaged library is not promised. No quest model, actor hierarchy, scripting
language, automatic traversal, localization service or sibling feature is needed.

Required first fixture, authored directly in C++ (U1 definition test data only):

| Node | Choice ID / text | Condition key | Effect key | Target |
| --- | --- | --- | --- | --- |
| `approach` | `introduce` / Introduce yourself | none | `introduce` | `gate` |
| `approach` | `silent` / Stay silent | none | none | `gate` |
| `gate` | `pass` / Ask to enter | `introduced` | none | `admitted` |
| `gate` | `leave` / Leave | none | none | `departed` |
| `admitted` | terminal text: You enter | — | — | — |
| `departed` | terminal text: You depart | — | — | — |

In the later console consumer, the application owns exactly one gameplay flag,
`introduced`, initially false.
Its condition reads that flag; its effect sets it true. A callback invocation
counter belongs only to test instrumentation. Both endings are reachable; after
`silent`, the visible `pass` choice is disabled and selecting it is rejected.
The important save point is `gate` after `introduce`, with the flag true.

## U1 compile-and-inspect caller

The small consumer case includes the public dialogue header first, compiles the
four-node fixture, retains the returned handle and inspects ordered values:

```cpp
auto result = CE::Dialogue::compile(gate(), operations()); // test fixture helpers
ASSERT_TRUE(result.has_value());
const CE::Dialogue::DefinitionHandle definition = *result;
EXPECT_EQ(definition->entry().id, CE::Dialogue::NodeId{"approach"});
const auto* choice = definition->find_choice({"gate"}, {"pass"});
ASSERT_NE(choice, nullptr);
EXPECT_EQ(choice->text, "Ask to enter");
```

This caller and the other cases in
[dialogue-definition.cpp](../projects/engine/tests/unit/src/dialogue-definition.cpp)
have passed warning-enabled C++23 syntax checking, not native execution.
[dialogue.h](../projects/engine/include/cheryl/core/game-framework/dialogue.h)
exposes only CPU values, standard-library facilities and Engine visibility macros.
[dialogue.cpp](../projects/engine/src/core/game-framework/dialogue.cpp) supplies
validation and lookup. Existing source globs select both files without CMake edits.
The spelling and ownership choices are engineering decisions, not newly inferred
personal API preferences.

## Proposed caller sequence beyond U1

Compilation below uses U1; session names and behavior remain proposed. `require`, `find_choice`,
`make_gate_definition`, flag handlers and the `GateSave` codec are application
helpers illustrating results, not additional engine APIs. `require` checks a
result and reports its diagnostic before extracting its value. The sketch has
not been compiled or run.

```cpp
namespace D = CE::Dialogue;                       // proposed
auto definition = require(D::compile(
    make_gate_definition("gate-encounter", 1),
    D::OperationKeys{{"introduced"}, {"introduce"}}));
GateFlags flags{.introduced = false};              // application value
auto test = [&](D::ConditionKey key) { return check_flag(key, flags); };
auto apply = [&](D::EffectKey key) { set_flag(key, flags); };

auto saved = [&] {
    auto session = require(D::Session::start(definition, D::SessionId{"gate-0001"}));
    auto offer = require(session.view(test));
    require(session.choose(find_choice(offer, "introduce").selection, test, apply));
    // Now at gate, step 1, flag true; no effect is pending.
    return GateSave{1, flags, require(session.snapshot())};
}();                                               // first session destroyed

auto loaded = require(decode_gate_save(encode_gate_save(saved)));
auto resumed = require(D::Session::resume(definition, loaded.dialogue));
flags = loaded.flags;                              // adopt together on this owner
auto gate = require(resumed.view(test));             // observes; applies no effects
require(resumed.choose(find_choice(gate, "pass").selection, test, apply));
```

The console later adds `save PATH` / `resume PATH` around this value round trip.
Decode and validate both parts into temporary values before adopting either;
the flag assignment and session move in this fixture must be nonthrowing.
`check_flag` rejects an unknown key, and `set_flag` resolves its known key before
the nonthrowing boolean assignment. A fresh session for the other route selects
`silent`, observes `pass` unavailable, then selects `leave`.

## Identities and retained values

**ST-002 — Authored identity differs from progress.** A draft owns definition ID,
positive authored revision, entry node, nodes, authored choice order and text.
Node IDs are unique within a definition; choice IDs are unique within their node.
Condition/effect keys occupy separate domains. Use distinct lightweight string
ID types, exact case-sensitive nonempty values, with no path normalization or
ordinal identity. Internal lookup indices can be compiled but never saved.
Any content or bound-operation semantic change requires an authored revision
bump. Revision correctness is an application obligation, not verified by a hash.

In the proposed later session unit, the application supplies a nonempty session ID identifying one encounter
instance, independently of the definition and engine runtime session. Start uses
step zero at the entry node; each successful choice increments a checked
`uint64_t` step. Exhaustion rejects before any callback. Restore retains session
ID, step and node. Two sessions over the same definition have different IDs and
independent cursors. The host ensures one active authority per saved session ID;
move-only live sessions discourage accidental copies but cannot police multiple
processes restoring the same file. Deliberate forks require new application IDs.

U1 compiles one `Draft`, so multiple definition IDs cannot occur in one input.
It rejects an empty definition ID and duplicate node IDs within that definition.
There is no cross-compilation registry to reject an already-used definition ID;
independent compilations of the same ID/revision are permitted. Global identity
and revision discipline remain application responsibilities. This interpretation
keeps duplicate checking within the authored domains instead of inventing a
definition registry. Same choice names across nodes and the same spelling across
condition/effect domains are valid, as are definition/node names with equal spelling.

**ST-003 — Ownership and execution.** `compile(const Draft&, const OperationKeys&)`
borrows stable inputs for the call, validates all rows, and deep-copies the draft
into a privately constructed `const Definition`. It never moves caller storage,
even when given `std::move(draft)`. Only compilation can construct a usable frozen
object; copy/move construction and assignment of `Definition` are deleted.
`DefinitionHandle` is a retained `shared_ptr<const Definition>`, not the sole
immutability mechanism. `CompileResult` is a dialogue-local
`std::expected<DefinitionHandle, std::vector<Diagnostic>>`: nonnull success or
nonempty diagnostics, with allocation/copy exceptions propagated. No generic
result facility is introduced.

`nodes()` returns an authored-order `span<const Node>`; ID and entry accessors
also borrow frozen storage. Exact `find_node` and node-scoped `find_choice` use
linear scans, return null on misses and never allocate or normalize names. Retain
a handle through borrowed access. This is sufficient for the tiny graph and avoids
an unneeded retained index; the private entry index preserves authored order.
Stable definition reads can be concurrent; input mutation/destruction must be
serialized with compilation. Owned diagnostic copies outlive their source inputs.

In later proposed units, a live move-only
session retains `shared_ptr<const Definition>` plus its own numeric cursor, step
and status; releasing the original definition handle does not invalidate it.
Application flags and callables are neither stored in that definition nor owned
by the session. Each operation borrows its supplied handlers only until return.
Handlers/captures must survive the call; reentering the session is rejected.

`view` returns copied node text and authored-order choices with availability and
selection values. These values and snapshots retain their own strings and IDs;
they borrow neither draft storage, the session, handlers nor application state.
They may be displayed after session destruction, but old selections gain no new
authority. Definition sharing permits stable concurrent reads; operations on a
session and its application flags require one serialized owner. The console
uses its main thread. A later runtime consumer should advance on simulation and
publish copies to presentation, retaining no `TickContext`, input view or frame
writer. No asynchronous producer, shutdown protocol or callback registration is
introduced; destruction performs no effects.

## Application policy and transition failure

**ST-004 — Opaque operations, application policy.** A choice contains at most one
optional condition key and one optional effect key. `OperationKeys` is a supplied
set of supported names for validation, not a global service or executable
registry. U1 validates each domain independently, rejects empty/duplicate
supported names, and retains used names in its copied choices. Validation accepts
no application code. In later units, per-call handlers would implement pure
`bool(ConditionKey)` and synchronous `void(EffectKey)` operations;
the application must honor the declared key contract. No engine flag store,
expression evaluator, inventory policy or serialized callable is proposed.
Missing condition means true; missing effect means no action. A view can evaluate
conditions repeatedly, so predicates cannot consume randomness or mutate flags.

**ST-005 — Selection and commit (proposed U2, absent from U1).** Each selection carries session ID, source node,
step and choice ID. These values scope a command; they are not a security token.
`choose` checks those fields, terminal/fault/busy state, membership and step
capacity, then reevaluates the condition against current application state.
A cached enabled row is insufficient. Rejections are explicit results such as
`wrong_session`, `stale_selection`, `unknown_choice`, `unavailable`, `finished`
or `step_exhausted`, leaving progress and effects untouched.

Complete all lookup, validation, allocation and success-result preparation before
calling an effect. Once its handler returns, installing the numeric target cursor,
incrementing the step and returning success must not allocate or throw. An effect
runs once for that accepted choice occurrence. Start, view, snapshot, resume and
destruction never invoke it. Cycles are permitted because every transition
requires a new explicit choice; revisiting an edge is a new occurrence, not a
save replay. There are no entry/exit hooks or implicit chains.

| Failure | Required outcome |
| --- | --- |
| Validation, guard exception or allocation before effect entry | Report/rethrow with context; no session change and no effect invocation. Predicates must uphold their purity contract. |
| Effect handler throws | Preserve/rethrow the failure and mark the session faulted. Progress remains at the source; application side effects may be partial. Reject further choices and snapshots. Do not retry automatically. |
| Snapshot/view copying or encoding fails | No progression or effect; retain the live state and any older valid save. |
| Decode/restore fails | Return diagnostic; adopt neither staged flags nor session; retain any active encounter. No callbacks run during restore. |
| Console output fails after a committed choice | Choice stays committed; displaying or saving it again cannot replay the effect. |

Mark the operation busy before invoking any handler and enforce this for view,
choose, snapshot and restore/adoption, including callbacks attempting to save.
The boolean fixture's successful effect cannot throw after assignment. General
effects have no rollback guarantee: recovery from a fault requires the host to
discard/reconcile its application state and load a previously coherent pair.
An external non-idempotent side effect would need an application transaction or
durable idempotency protocol in a later scope. This design does not supply one.

## Validation and saves

**ST-006 — Validate before use; preserve actionable failures.** `compile` returns
no usable definition if it finds empty/duplicate identities, zero revision,
missing entry, unknown targets, unknown condition/effect references, duplicate
declared operation keys, or malformed node shape. A node is either terminal with
no choices, or a prompt with at least one choice. Validate the entire draft,
including unreachable nodes, without invoking application handlers. U1 performs
no reachability analysis or warnings; a graph cycle is not itself invalid. Statically valid
conditions need not be satisfiable, and conditional dead ends remain host policy.

Diagnostics own code, definition ID/revision, optional zero-based node, choice and
supported-name indices, field and offending value. For example, a missing target
has code `MissingReference`, definition `gate-encounter@1`, node index 1, choice
index 0, field `target`, value `admittted`. Ordering is definition id/revision,
supported conditions then effects, entry, then each authored node's id/shape and
each choice's id/target/condition/effect. Membership sets are never enumerated to
produce diagnostics. Empty text is allowed; node shape checks also reject unknown
`NodeKind` values. The error variant exposes no partial definition. Use structured
results for expected authoring failures; selection rejections are still proposed.
Allocation/copy failures propagate as exceptions; callback failure rules await U2.
This is an additive local result contract, not an engine-wide error
framework. Exception reporting must preserve the original failure even if
enriching a message cannot allocate.

**ST-007 — Snapshot a completed boundary and the application state together (proposed, absent from U1).**
`snapshot()` succeeds only between operations on a healthy session. Version 1 is
an owned value of: snapshot schema, definition ID/revision, session ID, current
node ID and step. Terminal status is derived from the referenced node. An initial
snapshot records the entry; after a choice it records the destination after any
applied effect, never a pending edge. The flag belongs in the application's
versioned envelope alongside that snapshot; a cursor
alone cannot reconstruct application state. Neither process-local event types,
addresses, callbacks, indices, shared pointers nor an engine runtime ID are saved.

Concrete proposed envelope at the important save point:

```json
{
  "save_schema": 1,
  "flags": {"introduced": true},
  "dialogue": {
    "snapshot_schema": 1,
    "definition": "gate-encounter",
    "definition_revision": 1,
    "session": "gate-0001",
    "node": "gate",
    "step": 1
  }
}
```

Keep JSON in the console adapter, using the repository's existing nlohmann parser
when that unit is authorized; public core headers expose values only. Its include
is currently private to the engine, so the console needs explicit local wiring,
not an assumed transitive SDK dependency. This introduces no new package/pin.
The console decoder checks exact schema, required/unknown fields, types, boolean
flag, nonempty IDs and integer ranges without coercion. Bound fixture input to
64 KiB; reject duplicate object keys rather than accepting silent replacement.
`resume` independently verifies snapshot schema, exact definition ID/revision,
known node and valid ID/step fields before creating a session; it never evaluates
guards or replays transitions. A checked but externally supplied step is not
proof of an actual path. Save integrity and correspondence of flags to progress
are host responsibilities; arbitrary edited saves are not authenticated.

Save/capture and load/adoption run on the same owner as the flag and session, with
no operation in flight. Decode into fresh values and validate all parts before a
nonthrowing adoption. The console writes to its own temporary file, checks write
and close success, then publishes a fresh, explicitly chosen checkpoint path;
it refuses to overwrite an existing checkpoint. Failed writes/publish operations
are reported, never promoted as saves, and the owned temporary file is discarded.
Older checkpoints remain untouched. Atomic replacement of an existing save,
concurrent writers and power-loss durability are outside the first milestone.
Read/write errors never advance the session.

Loading the paired `gate` snapshot shows the enabled choices with the flag true,
and zero additional effect calls. No applied-effect ledger is needed because a
resumable boundary never splits effect application from cursor commit. Loading
an older checkpoint intentionally rolls back both values. Crash recovery between an effect
and capturing/saving a pair, or restoring progress alongside independently changed
world state, has no exactly-once guarantee. Exact revision matching rejects old
content; migration and hot reload wait for a demonstrated need.

## Source evidence and confidence

All current file locations below describe baseline `3542f883391d4225329dab8d5d61a5987e69c8f0`.
Inspection establishes code paths and test intent, not passing execution. Existing
documentation explains intended use and was checked against the cited sources.
Current interfaces, including recent agent-developed work, constrain integration;
their existence or Git author identity does not establish the user's API taste.

| Evidence | Observed contract / consequence |
| --- | --- |
| C1: [AbstractGame](../projects/engine/include/cheryl/core/game-framework/abstract-game.h), lines 8–29, 91–110; [TickContext](../projects/engine/include/cheryl/core/game-framework/tick-context.h), lines 15–33; [runtime implementation](../projects/engine/src/core/game-framework/game-runtime.cpp), lines 565–588, 610–650, 862–882 | Game organization is unconstrained; simulation owns mutation, borrowed tick/frame data expires, concurrent frame preparation can be skipped, and hook failure leads through cleanup. Keep effects out of presentation and keep the console independent. **High** compatibility confidence; ST-001/003/005. |
| C2: [StateMachine](../projects/engine/include/cheryl/templates/state-machine.h), lines 11–20, 47–69; [StateTracker](../projects/engine/include/cheryl/templates/state-tracker.h), lines 8–26; [VersionedVariable](../projects/engine/include/cheryl/templates/versioned-variable.h), lines 19–25, 40–68 | Machine order is exit → assignment → action → enter without rollback. Tracker references borrow mutable storage; versioned snapshots copy one value, can coalesce changes and permit revision wrap. These are neither a durable save nor the proposed effect-before-cursor commit. **High**; ST-003/005/007. |
| C3: [state test source](../projects/engine/tests/all-tests/src/templates/state.cpp), lines 10–35, 48–74; [test routing](../projects/engine/tests/CMakeLists.txt), lines 4–16 | Concrete callers compose two application state domains through a guard and assert ordered hooks/coherent samples. These cases route to unit sources; they contain no dialogue or persisted-effect acceptance. Read, not run. **High** for existing usage only; ST-004/005. |
| C4: [EventChannel](../projects/engine/include/cheryl/core/subsystems/event-channel.h), lines 10–26; [EventBus](../projects/engine/include/cheryl/core/subsystems/event-bus.h), lines 34–54, 124–164; [dispatch/notify](../projects/engine/src/core/subsystems/event-bus.cpp), lines 67–82, 109–136; [typed-event consumer test](../projects/engine/tests/acceptance/src/typed-events.cpp), lines 62–84 | Channel type identity is in-process, payload callbacks borrow invocation copies, dropping a registration does not unsubscribe, dispatch can propagate listener failure, and notify isolates failures. Queued copies/discards are tested in source. An event notification cannot authorize or transactionally persist a gameplay effect. **High**; ST-001/004/007. |
| C5: [sprite declarations](../projects/engine/include/cheryl/assets/types/2d/sprite.h), lines 16–49, 56–74; [sprite implementation](../projects/engine/src/assets/types/2d/sprite.cpp), lines 67–90, 111–129; [playback consumer test](../projects/engine/tests/all-tests/src/assets/render-frame.cpp), lines 72–88 | Current sprites retain immutable definitions and produce independently mutable cursors, including after asset release. Source supplies an ownership analogy, not a personal preference or save protocol. **High** source confidence; **medium** transfer to ST-002/003. |
| C6: [tutorial game](../projects/apps/tutorial-game/src/game.h), lines 27–44; [implementation](../projects/apps/tutorial-game/src/game.cpp), lines 38–55, 65–95, 100–138; [Character](../projects/apps/tutorial-game/src/character.h), lines 17–44 | Concrete application members own entities, controllers, score and animation; update mutates them, rendering submits values, cleanup releases them. Supports application-owned policy and explicit lifetimes as a compatible composition. Does not prove a preferred dialogue API. **High** observation; ST-001/003/004. |
| C7: [FileRegistry](../projects/engine/include/cheryl/core/resources/asset-management/file-registry.h), lines 16–23, 31–48; [manifest diagnostic helpers](../projects/engine/src/core/resources/asset-management/manifest-loader.cpp), lines 31–70; [engine target](../projects/engine/CMakeLists.txt), lines 21–29 | File indexing owns paths, not save contents. Manifest diagnostics retain source/field context. JSON already has a private include path; it is not a public dependency promise. **High** observations, **medium** choice of a console JSON codec; ST-006/007. |

### Intentional v3 examples, provenance and counterevidence

Historical locator `V3:path:lines` below means
`git show eb3f81c219e779b0200c1e82e5bc7239386528e8:path`, with one-based lines.
That commit is `V3.0a (#12)`, authored as cppcooper and committed by GitHub on
2026-10-14. Its squash message contains explicit incomplete/WIP subjects and
does not expose original per-change authorship or chronology. Inspected
`include/`, `src/` and `tests/` content has no diff against merge
`75bfca596c7246539c604d51d2a0d36a2577f40b` (`v3-incomplete`, PR #13).
`8854ef84f360fafa3572b58ee5440b5aabc0e67c` then explicitly replaces the archived
v3 snapshot with the modern baseline. Import date and membership prove neither
new v3 intent nor present preference.

| Historical evidence | Inference and limiting evidence |
| --- | --- |
| H1: `V3:include/core/game.h:55–65,76–78`; `V3:src/core/game.cpp:5–30,100–124` | Explicit description and actual module dispatch support intentional reusable facilities with application-composed systems. **Medium** confidence for that broad preference. Counterevidence: the header identifies v1.0.2/2018 at lines 5/10, stale includes remain, supplied raw module pointers have no transfer/destruction contract, and the concrete caller is framework code rather than a complete game. Does not justify recreating `GameModule` or copying its ownership. ST-001/003. |
| H2: `V3:include/core/modules/input-controls/control-module.h:13–14,29–46`; `V3:src/core/modules/input-controls/control-module.cpp:33–38,65–86`; concrete binding `V3:src/core/game.cpp:69–77` | Bind/store/invoke paths support caller-supplied behavior. **Medium** inference for ST-004, **low** for today's callback API spelling. Counterevidence: invocation holds mutexes, no unsubscribe/lifetime token is supplied, and `V3:include/core/modules/input-controls-2/input-system.h:11,23–24` proposes a replacement with empty queue methods. No settled event delivery or ownership preference follows. |
| H3: `V3:src/asset-types/sprite.cpp:41–61,80–105,133–160`; `V3:include/types/sprite.h:11–30,36–53` | Authored frame data and explicit runtime selection/reset operations are deliberate reusable asset behavior. **Medium** support for content plus caller-controlled progress. Counterevidence against a stronger ST-002/003/007 taste claim: one mutable sprite combines both, frames retain parent/resource references, and Reset releases resources rather than restoring a durable snapshot. Immutable definition/session separation is a new design inference. |
| H4: `V3:include/abstracts/singleton.h:7–10,21–27,82–107`; concrete but tentative consumer `V3:include/core/resources.h:12–27` | Comments explicitly delegate construction arguments and implementations expose missing/repeated construction failures. **Medium** support for explicit setup/failure reporting, **low** for dialogue ownership implications. Counterevidence: lazy defaults, pervasive singleton access and resource-policy TODOs. This does not support prescribing a dialogue singleton. ST-003/006. |
| H5: `V3:include/interfaces/actor.h:6–12` | Reset/Save/Load are declarations; the bounded historical investigation found no concrete implementation/consumer. **Low** evidence for any save preference. No historical exactly-once effects, authored/session IDs or compatible schema protocol was established. ST-002/007 remain requirements-driven design choices. |

### Alternatives and reversal cost

| Decision | Alternative considered | Reason for present proposal / reversal consequence |
| --- | --- | --- |
| ST-001 | Put dialogue entirely in the console; or create a narrative/quest framework | The former proves a fixture without testing reuse, the latter invents policy beyond the brief. Keep only validation/cursor reusable. Namespace/header location is provisional and cheap to change before callers accumulate. |
| ST-002 | Serialize vector offsets, addresses, runtime IDs or a content hash | Stable authored strings and exact manual revision are understandable and portable. They require disciplined revision bumps. A hash/migration layer can be added later, with a schema change; no current save compatibility is promised. |
| ST-003 | Borrow a mutable draft or store callback closures in every session | Borrowing makes draft edits/destruction observable; retained closures expand lifetime and save ambiguity. Frozen data requires an owned object and shared control block; copied views would add small allocations. Review copy cost only after the tiny consumer exists. |
| ST-004 | Engine-owned flags/expression language; event bus effect dispatch | Applications already own meaning, and notification failure/delivery is not a commit. Explicit synchronous keys keep policy outside. More operations or composed predicates require a later demonstrated consumer, not speculative variants now. |
| ST-005 | Wrap StateMachine hooks; introduce plan/ack transactions and durable effect receipts | Hook assignment/failure order does not fit the intended save boundary. A dedicated numeric cursor permits nonthrowing commit after the fixture's boolean effect; a transaction protocol is excessive for it. External effects would reopen this decision rather than silently inherit its limited guarantee. |
| ST-006 | Lazy missing-reference errors during play; throw for every ordinary rejection | Eager diagnostics catch hidden branches and structured rejections distinguish unavailable choices from broken handlers. This adds a small diagnostic value type without rewriting existing engine error policy. |
| ST-007 | Save cursor only; replay from entry; serialize arbitrary callbacks; general persistence service | A paired snapshot preserves application state without replay. Console JSON is replaceable without changing the core snapshot, but changing its value schema needs a new version. File overwrite/durability and save migration remain independent future work. |

## Integration overlaps and implementation units

The workspace briefs, rather than sibling implementation files, identify overlaps.
Map storage and settings also need versioning, diagnostics and persistence; this
feature owns only dialogue snapshot values and its console envelope. Do not merge
their branches or extract a shared serializer to start this work. A future world
save must capture flags and dialogue on one owner; a map ID or settings revision
cannot substitute for the definition/session identities. Tilemap has no first
consumer dependency. Runtime/state/events overlap only at optional composition
boundaries described above; no changes to those contracts are planned. A future
UI would consume copied views and send scoped selections to simulation.

| Unit, in dependency order | Coherent deliverable and gate |
| --- | --- |
| U1: validated CPU definition | Source and 16 focused cases implemented; all pass warning-enabled syntax checking. Private construction and a deep copy close the mutable-alias boundary. Native verification remains blocked on the baseline fallback-font input; see next.md before claiming U1 accepted. |
| U2: explicit session | View, scoped selection, application handlers, checked progression, failure state and value snapshot/resume. Gate: both routes, rejection without effects, independent lifetimes, stale selections, throwing handlers and paired resume/no replay cases. Reassess the nonthrowing post-effect path before any extra API callers. |
| U3: console acceptance | Branch-local console app/fixture, versioned JSON envelope, save/resume commands and focused codec/transcript cases. Wire only needed targets/dependencies, then run permitted checks when this unit is assigned. Gate: actual console outcomes and resumed effect count, malformed/mismatched saves, safe adoption and explicit I/O failures. |

Only U1 is assigned now. U2/U3 await later coordinator assignments.
[next.md](next.md) owns U1's verification gap and the next useful unit.

## Acceptance and present verification

The feature milestone remains unexecuted. Required observable acceptance:

1. `introduce → pass` ends at `admitted`; `silent → leave` ends at `departed`.
   One gameplay flag and no UI/runtime startup are used by the console.
2. `silent → pass` rejects as unavailable, preserving node, step and flag with
   zero effect calls. Changing the flag after presenting a row is checked again
   when selecting; wrong-session, stale and repeated selections cannot apply.
3. Missing/duplicate entry/node/choice/operation references reject at compilation
   with definition, location and offending ID. Bad restore schema/revision/node
   yields a useful diagnostic before flags/session adoption or any callback.
4. Save at `gate` after `introduce`; destroy session and original application
   state; decode into fresh values, restore and display the same progress and
   available choices. A retained test counter stays at one across snapshot,
   decode, resume, repeated views and subsequent `pass`. Also test pre-effect
   and terminal snapshots. An old selection remains stale after progression.
5. Two differently identified sessions share one immutable definition without
   sharing progress. Destroy draft/original definition handles and retain views
   past session destruction without dangling data. Guard/copy failure preserves
   progress; effect failure faults and prohibits saving/retrying; busy/reentrant
   calls and step overflow reject before effects.
6. Console codec rejects malformed/truncated/oversized JSON, duplicate/unknown
   fields, missing fields, wrong types, out-of-range steps and unknown versions.
   Existing active state survives a failed load; an existing checkpoint is never
   overwritten, and failed new-file writes do not change encounter progress.

Performed during the initial design investigation: clean Git/index inspection, source
and local-history reads, concrete-consumer/test-registration tracing, and review
of these records against those sources. A bounded read-only subagent assisted
historical evidence collection. That design task ran only text/static record
checks. It ran no compiler, build, native test, console consumer, dependency
retrieval, installation, documentation generation or external service acceptance.

Material uncertainties: modern API taste is largely unproven; manual revision
discipline cannot detect same-ID/revision content changes; generic effects can
partially mutate before throwing; JSON/console target wiring and the nonthrowing
commit/adoption path still need implementation evidence. U1's source immutability
and diagnostic cases are syntax-checked but not accepted by native execution:
the prescribed configuration fails because `assets/fonts/DejaVuSans.ttf` is absent,
and CTest selected zero tests (error exit 8). No asset substitution, dependency pin
change or alternate native runner was used. Actual commands, source registration,
coverage and the prerequisite recovery action are recorded in [next.md](next.md).
