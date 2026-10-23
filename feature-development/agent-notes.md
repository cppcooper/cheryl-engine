# Storytelling: first dialogue milestone

U1 implements validated immutable CPU definitions on `feat/storytelling`, from
clean design checkpoint `716ec60984e6764a03ed375ac37826e5d92e6978` (original source
baseline `3542f883391d4225329dab8d5d61a5987e69c8f0`). U1V verified the unchanged
definition source at `25836ffb48fb25e9ed5e89d2805daf6e093ed5fc`: all 16 focused
native cases passed under Debug ASan/UBSan. LeakSanitizer cannot run under this
executor's tracing; command-local leak disabling is recorded in ST-008, with no
passing leak claim. U1 definition acceptance is complete; see [next.md](next.md).
U2 implements checked sessions and owned value resume, with 18 session cases,
16 definition regressions and the isolated allocation probe passing; see
[next.md](next.md). U3 implements the private console and paired JSON boundary:
26 new cases plus all 34 accepted regressions passed (60/60), including ten actual
child-console transcripts. Detailed execution evidence is recorded in next.md. The companion
[decisions](decisions.md) use the same `ST-001` through `ST-010` IDs;
[next](next.md) records the next coordinator review action.

## Authority, scope and evidence limits

The coordinator's STORYTELLING-U1 assignment authorized the definition source;
STORYTELLING-U1V completed definition verification. STORYTELLING-U2 authorizes the
session implementation, focused checks and these records. STORYTELLING-U3 authorizes
the private console, strict paired codec, exclusive fresh-path persistence and
focused native/transcript acceptance. These assignments supersede the
earlier design-only task. The parent `AGENTS.md`,
`coordination/autonomy-policy.md`, `coordination/workers.json` and feature brief
were read from `/mnt/projects/cheryl-features/`. Repository workflow,
investigation, architecture, testing, Git and documentation instructions were
also read, including `docs/AGENTS.md`; the nested-instruction search found only
the root and documentation instruction files. No pre-existing changes or
feature-development records were present at the original design baseline. U1
started with the design records committed and a clean tree. This clone alone is
owned; protected checkouts and sibling clones are outside the work.

**ST-001 — Small reusable boundary.** U1/U2 implement CPU dialogue definition
validation and an explicitly advanced session; U3 adds the private console adapter
under `projects/apps/dialogue-console/`, with a default-OFF
`CHERYL_BUILD_DIALOGUE_CONSOLE` option. It links only Engine; its state library and
test cases explicitly include pinned JSON 3.11.3 privately. Root options/registration,
the tutorial-game's `cheryl_add_test_suite` pattern and Engine's helper were inspected
before wiring. No optional modules, Workers, public header changes or SDK export
are needed. The app owns its own gate fixture, not an import from a sibling.
The definition lives in `CE::Dialogue`, under
`projects/engine/include/cheryl/core/game-framework/dialogue.h` and its source
counterpart. The public contract needs no `AbstractGame`, runtime context,
renderer, UI toolkit, event bus, clock, input service or file index. Initial
engine linkage may still carry existing engine dependencies; an independently
packaged library is not promised. No quest model, actor hierarchy, scripting
language, automatic traversal, localization service or sibling feature is needed.

Concrete fixture, authored directly in C++ (U1 test data, now also the private app):

| Node | Choice ID / text | Condition key | Effect key | Target |
| --- | --- | --- | --- | --- |
| `approach` | `introduce` / Introduce yourself | none | `introduce` | `gate` |
| `approach` | `silent` / Stay silent | none | none | `gate` |
| `gate` | `pass` / Ask to enter | `introduced` | none | `admitted` |
| `gate` | `leave` / Leave | none | none | `departed` |
| `admitted` | terminal text: You enter | — | — | — |
| `departed` | terminal text: You depart | — | — | — |

In the console consumer, the application owns exactly one gameplay flag,
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
have passed warning-enabled C++23 syntax checking and native compilation/linking.
U1V executed all 16 successfully, including this header-first caller and its
construction/type assertions; leak coverage remains unavailable (ST-008).
[dialogue.h](../projects/engine/include/cheryl/core/game-framework/dialogue.h)
exposes only CPU values, standard-library facilities and Engine visibility macros.
[dialogue.cpp](../projects/engine/src/core/game-framework/dialogue.cpp) supplies
validation and lookup. Existing source globs select both files without CMake edits.
The spelling and ownership choices are engineering decisions, not newly inferred
personal API preferences.

## U2 small caller and paired value resume

The header-first `dialogue_session.consumer` case compiles the same gate definition
and uses this sequence. `definition`, `start`, `choice`, `predicate` and `effect`
are fixture helpers in the test source, not additional engine APIs. One application
flag owns meaning; `calls` is only effect-count instrumentation. See next.md for
executed results, rather than treating the source sketch as acceptance by itself.

```cpp
namespace D = CE::Dialogue;
const auto content = definition();
Flags flags;
int calls = 0;
auto session = start(content);
const auto offer = session.view(predicate(flags)).value();
session.choose(choice(offer, "introduce").selection,
               predicate(flags), effect(flags, calls)).value();
const Flags saved_flags = flags;
const auto saved = session.snapshot().value();       // gate, step 1, flag true
auto loaded = D::Session::resume(content, saved);
ASSERT_TRUE(loaded);
flags = {};
session = std::move(*loaded);                       // both sessions idle; nonthrowing
flags = saved_flags;                               // same serialized owner
const auto resumed = session.view(predicate(flags)).value();
session.choose(choice(resumed, "pass").selection,
               predicate(flags), effect(flags, calls)).value();
EXPECT_EQ(calls, 1);                               // introduce was not replayed
```

The U3 console adds encoding and `save PATH` / `resume PATH` around this value
round trip. Decode and validate both parts into temporary values before adopting
either; the flag assignment and idle session move in this fixture are nonthrowing.
The fixture predicate/effect reject an unknown key before any flag assignment;
the known effect's boolean assignment is nonthrowing. The other route selects
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

In U2, the application supplies a nonempty session ID identifying one encounter
instance, independently of the definition and engine runtime session. Start uses
step zero at the entry node; each successful choice increments a checked
`uint64_t` step. Exhaustion rejects before any callback. Restore retains session
ID, step and node. Each start/resume also allocates a fresh private shared
incarnation marker. Selections retain it; snapshots never contain it. Idle moves
preserve the marker. Retaining old tokens prevents marker-address reuse, so even
an identical-field resume/rewind rejects their old authority. This is a small local
ownership mechanism, not a global ID service. Public token fields remain checked
logical values, not security credentials. The coordinator explicitly required this
correction; it is not inferred personal API taste.

Two sessions have independent cursors, even if the host repeats a logical ID;
that host should give independent encounters different IDs. It ensures one active authority per saved session ID;
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
U1V's native ownership and lookup cases passed after draft mutation/destruction
and through retained-handle release; this supplies executable evidence for the
observed lifetime boundary, subject to ST-008's leak-coverage limit.

In U2, a live move-only
session retains `shared_ptr<const Definition>` plus its own numeric cursor, step
and status; releasing the original definition handle does not invalidate it.
Application flags and callables are neither stored in that definition nor owned
by the session. Each operation borrows its supplied handlers only until return.
Handlers/captures must survive the call; reentering view/choose/snapshot returns
`Busy`. Idle move construction/assignment preserve incarnation and are noexcept;
moved-from operations return `MovedFrom`. Active moves, replacement or destruction
terminate before invalidating storage beneath a callback. This hard lifecycle
precondition makes paired idle adoption nonthrowing, at the cost of unrecoverable
programming misuse; confidence in that policy is medium and reversal affects only
these initial callers. No operation supplies cross-thread synchronization.

`view` returns copied node text and authored-order choices with availability and
selection values. These values and snapshots retain their own strings and IDs;
they borrow neither draft storage, the session, handlers nor application state.
They may be displayed after session destruction, but old selections gain no new
authority. Definition sharing permits stable concurrent reads; operations on a
session and its application flags require one serialized owner. The private console
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
no application code. U2 borrows per-call `ConditionHandler` and `EffectHandler`
std::function objects with signatures `bool(const ConditionKey&)` and
`void(const EffectKey&)`. It never copies or retains those objects. Keys borrow
stable retained definition storage for the invocation. Callable conversion at the
caller can allocate before operation entry; the engine performs no owning key copy
after an effect. Missing required handlers yield explicit errors;
the application must honor the declared key contract. No engine flag store,
expression evaluator, inventory policy or serialized callable is proposed.
Missing condition means true; missing effect means no action. A view can evaluate
conditions repeatedly, so predicates cannot consume randomness or mutate flags.

**ST-005 — Selection and commit (implemented U2).** Each selection carries private
live incarnation plus session ID, source node, step and choice ID. These values
scope a command; they are not a security token. `choose` checks those fields,
terminal/fault/busy/moved-from state, membership and step
capacity, then reevaluates the condition against current application state.
A cached enabled row is insufficient. Rejections are explicit results such as
`WrongSession`, `WrongIncarnation`, `StaleSelection`, `UnknownChoice`, `Unavailable`,
`Finished` or `StepExhausted`, leaving progress and effects untouched. Terminal
views/snapshots and maximum-step snapshots remain valid; choosing cannot advance
a terminal or overflowed cursor.

Complete all lookup, validation, allocation and success-result preparation before
calling an effect. Success is `expected<void, SessionError>`, prepared before
entry, with a statically checked nonthrowing copy and no owned result payload.
The selected effect key borrows retained storage, so argument preparation cannot
introduce a later string copy. Once its handler returns, installing the numeric target cursor,
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

An allocation-free RAII busy guard covers view/choose/snapshot and clears the flag
without throwing on every exit, including callback and owned-copy exceptions.
Factories return a separate fresh session without modifying existing values;
adoption must wait for an idle owner. Callback moves/replacement/destruction are
hard lifecycle violations, while callback operations predictably return `Busy`.
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
results for expected authoring failures; U2 selection rejections use a separate
dialogue-local `SessionError` enum. Allocation/copy failures propagate as exceptions;
the U2 callback failure rules above preserve the original exception.
This is an additive local result contract, not an engine-wide error
framework. Exception reporting must preserve the original failure even if
enriching a message cannot allocate.

**ST-007 — Snapshot a completed boundary and the application state together (U2 values, U3 paired adapter).**
`snapshot()` succeeds only between operations on a healthy session. Version 1 is
an owned value of: snapshot schema, definition ID/revision, session ID, current
node ID and step. Terminal status is derived from the referenced node. An initial
snapshot records the entry; after a choice it records the destination after any
applied effect, never a pending edge. The flag belongs in the application's
versioned envelope alongside that snapshot; a cursor
alone cannot reconstruct application state. Neither process-local event types,
addresses, callbacks, indices, shared pointers, live incarnation nor an engine runtime ID are saved.
`Snapshot::schema` defaults to `current_schema == 1`; resume requires exact schema,
definition/revision, nonempty session ID and a known node. A uint64 maximum step
can be represented/restored, but choose rejects before any predicate/effect.
Malformed/mismatched factories preserve any existing live session by construction.

Concrete implemented envelope at the important save point:

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

JSON stays in the private console adapter, with explicit private include wiring
to the existing pinned nlohmann headers, never assumed Engine transitivity.
Public core headers remain CPU/JSON-free. The ST-009 decoder checks exact schema,
required/unknown fields, types, boolean flag, nonempty IDs and integer ranges
without coercion. This introduces no new package/pin.
`resume` independently verifies snapshot schema, exact definition ID/revision,
known node and valid ID/step fields before creating a session; it never evaluates
guards or replays transitions. A checked but externally supplied step is not
proof of an actual path. Save integrity and correspondence of flags to progress
are host responsibilities; arbitrary edited saves are not authenticated.

Save/capture and load/adoption run on the same owner as the flag and session, with
no operation in flight. `Encounter::capture` takes the owned idle snapshot and
boolean together. `restore_bytes` decodes fresh values, then `Encounter::restore`
calls `Session::resume` against its retained gate definition before touching either
live value. Successful idle move assignment and boolean adoption cannot throw;
failure preserves the whole pair and its old live selections. No handler runs.
The instrumented effect count is borrowed, unsaved and unchanged by restore.
ST-010 below replaces the earlier temporary-publication proposal with the explicitly
authorized narrower exclusive fresh-path policy. Read/write errors never advance
or replace either live value.

Loading the paired `gate` snapshot shows the enabled choices with the flag true,
and zero additional effect calls. No applied-effect ledger is needed because a
resumable boundary never splits effect application from cursor commit. Loading
an older checkpoint intentionally rolls back both values. Crash recovery between an effect
and capturing/saving a pair, or restoring progress alongside independently changed
world state, has no exactly-once guarantee. Exact revision matching rejects old
content; migration and hot reload wait for a demonstrated need.

## U3 private caller, strict codec and files

The executable starts an `Encounter`, shows an owning view, then accepts `show`,
`choose OFFER CHOICE`, `save PATH`, `resume PATH`, `quit`, or EOF. For example:

```text
Offer 1: A guard waits.                 # actual output uses separate lines
choose 1 introduce
Offer 2: The gate is closed.
save /chosen/fresh/gate.json
resume /chosen/fresh/gate.json
Offer 3: The gate is closed.            # fresh incarnation and presentation
choose 3 pass
You enter
quit
```

The console holds only the latest owning view and issues monotonically numbered
presentations. It passes the actual row's Selection to `Encounter::choose`, never
reconstructing private authority. Prior presentation numbers reject after show,
progression or successful resume; failed save/read/decode/resume leaves the current
presentation usable. The Engine still rechecks its current condition. Successful
resume discards the old view before presenting new choices. View/show, invalid
input, save/load, factories and EOF/quit perform no effects. Opt-in
`--trace-effects` prints a process-local counter only for executable transcripts;
it is not serialized, a second gameplay flag or a durable receipt. Command spelling
is a medium-confidence application choice, locally reversible without Engine changes.

**ST-009 — Bounded complete envelope, no coercion.** Reads allocate at most
64 KiB plus one detection byte before acquiring the stream. The decoder first
checks size and scans **every** byte for raw NUL, including an otherwise valid
JSON document's suffix. The pinned parser can treat NUL as EOF, so strict parser
mode alone is insufficient. Nlohmann SAX callbacks fill fresh owned values using
two fixed frames, admitting only envelope and flags/dialogue objects. Any deeper
object/array rejects immediately; no arbitrary DOM or recursive app structure is
constructed. Decoded property names map to per-object bit sets, catching duplicate
keys and escaped-equivalent names. Required masks reject missing fields; unknown
names, wrong shapes/types, numeric booleans, negative/out-of-range integers,
floating schemas and unsupported schemas reject. Strict parsing rejects malformed,
truncated, trailing junk and second documents. Exact definition/revision/node
validation remains independently in Session::resume. Edited steps are representable
values, not proof of history; flags are adopted as supplied, never inferred.
Read/decode diagnostics do not rewrite input. Confidence is high in bounds and
rejection semantics from fixed-shape source and passing native cases in next.md. A private
DOM-with-callback alternative was rejected because pruning need not stop deep
parsing. Replacing this fixture-specific decoder is cheap while saves are private;
changing schema semantics requires a new explicit version.

**ST-010 — Exclusive fresh paths, checked completion.** `save_fresh` validates a
nonempty NUL-free path, captures/encodes before opening, and uses C11 `fopen("wbx")`
for exclusive creation. No existence precheck, truncation, rename or replacement
is used. File ownership starts only after open succeeds. All writes, flush and
close are checked; a short/failed write or flush/close failure removes only the
newly owned incomplete destination. After open, no throwing C++ work precedes
close/cleanup. If cleanup fails, the diagnostic includes the owned path and cleanup
failure, and never reports a successful checkpoint. Existing files, leaf symlinks
and dangling leaf links are refused without touching contents/referents. This is
verified only on the native Linux/C-library configuration; other libraries are
unverified. A private five-boolean seam simulates open, partial write, flush, close
and owned cleanup failure without exhausting memory, permission assumptions or
device access. It is not a general I/O abstraction. Atomic replacement, fsync or
power-loss durability, hostile directory races, multiwriter coordination and durable
external-effect guarantees are excluded. Confidence is high for this bounded policy;
adding replacement would reopen its design rather than silently widen it.

## Verification environment

**ST-008 — Keep the LSan accommodation local.** The unchanged generated discovery
script reproduced `LeakSanitizer has encountered a fatal error` and
`LeakSanitizer does not work under ptrace (strace, gdb, etc)`, with result 1.
Its complete output and exit status are preserved in clone-local U1V logs.
The coordinator authorized this specific tracing limitation's accommodation:
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1` on the build/discovery and focused CTest commands.
No sanitizer build flag, timeout, source behavior or global setting changed.
Both dialogue object rules retain `-fsanitize=address,undefined`; the incremental
target and all 16 cases pass. Leak detection is unavailable, not passing.
Confidence in this environment diagnosis is high from this clone's reproduced
failure and adjusted success. Removing only `detect_leaks=0` on an LSan-compatible
executor reverses the accommodation while retaining both halt-on-error settings,
without code or build changes. Exact commands,
logs, selected names and remaining acceptance limits belong to [next.md](next.md).

## Source evidence and confidence

All current file locations below describe baseline `3542f883391d4225329dab8d5d61a5987e69c8f0`.
Inspection establishes code paths and test intent, not passing execution. Existing
documentation explains intended use and was checked against the cited sources.
Current interfaces, including recent agent-developed work, constrain integration;
their existence or Git author identity does not establish the user's API taste.

| Evidence | Observed contract / consequence |
| --- | --- |
| C1: [AbstractGame](../projects/engine/include/cheryl/core/game-framework/abstract-game.h), lines 8–29, 91–110; [TickContext](../projects/engine/include/cheryl/core/game-framework/tick-context.h), lines 15–33; [runtime implementation](../projects/engine/src/core/game-framework/game-runtime.cpp), lines 565–588, 610–650, 862–882 | Game organization is unconstrained; simulation owns mutation, borrowed tick/frame data expires, concurrent frame preparation can be skipped, and hook failure leads through cleanup. Keep effects out of presentation and keep the console independent. **High** compatibility confidence; ST-001/003/005. |
| C2: [StateMachine](../projects/engine/include/cheryl/templates/state-machine.h), lines 11–20, 47–69; [StateTracker](../projects/engine/include/cheryl/templates/state-tracker.h), lines 8–26; [VersionedVariable](../projects/engine/include/cheryl/templates/versioned-variable.h), lines 19–25, 40–68 | Machine order is exit → assignment → action → enter without rollback. Tracker references borrow mutable storage; versioned snapshots copy one value, can coalesce changes and permit revision wrap. These are neither a durable save nor U2's effect-before-cursor commit. **High**; ST-003/005/007. |
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

| Decision | Alternative considered | Reason for present choice / reversal consequence |
| --- | --- | --- |
| ST-001 | Put dialogue entirely in the console; or create a narrative/quest framework | The former proves a fixture without testing reuse, the latter invents policy beyond the brief. Keep only validation/cursor reusable. Namespace/header location is provisional and cheap to change before callers accumulate. |
| ST-002 | Serialize vector offsets, addresses, runtime IDs or a content hash; use only logical fields or a global incarnation counter | Authored strings/manual revision remain portable. A private retained marker rejects equal-field rewind tokens without a registry or address reuse while tokens survive; it costs one small allocation plus shared references. A content hash/migration remains a separate schema change. |
| ST-003 | Borrow mutable drafts, retain closures, or throw on active moves | Private copying retains stable definition data; independent views cost string/vector copies. Borrowed handlers keep ownership local. Nonthrowing idle moves permit paired adoption; active lifecycle misuse terminates before invalidating callback storage. Changing that medium-confidence policy would affect only the initial callers, not persisted schema. |
| ST-004 | Engine-owned flags/expression language; event bus effect dispatch | Applications already own meaning, and notification failure/delivery is not a commit. Explicit synchronous keys keep policy outside. More operations or composed predicates require a later demonstrated consumer, not speculative variants now. |
| ST-005 | Wrap StateMachine hooks; introduce plan/ack transactions and durable effect receipts | Hook assignment/failure order does not fit the intended save boundary. A dedicated numeric cursor permits nonthrowing commit after the fixture's boolean effect; a transaction protocol is excessive for it. External effects would reopen this decision rather than silently inherit its limited guarantee. |
| ST-006 | Lazy missing-reference errors during play; throw for every ordinary rejection | Eager diagnostics catch hidden branches and structured rejections distinguish unavailable choices from broken handlers. This adds a small diagnostic value type without rewriting existing engine error policy. |
| ST-007 | Save cursor only; replay from entry; serialize arbitrary callbacks; general persistence service | A paired snapshot preserves application state without replay. Console JSON is replaceable without changing the core snapshot, but changing its value schema needs a new version. File overwrite/durability and save migration remain independent future work. |
| ST-008 | Change sanitizer build/global settings or leave native checks unexecuted | A reproduced LSan tracing failure justifies only the authorized command-local leak accommodation. ASan/UBSan and halt-on-error remain active; leak coverage is unavailable. Reversal removes only detect_leaks=0 on a compatible executor, with no source or configuration migration. |
| ST-009 | DOM parsing with a pruning callback; permissive field conversion | Fixed SAX frames stop excess nesting immediately and detect decoded-key duplicates before replacement. This adds fixture-specific code; replacing the private codec is local, while changed schema meaning needs a new version. |
| ST-010 | Temporary publication/rename; overwriting existing saves | The assigned scope deliberately selects exclusive fresh destinations and checked completion. Existing bytes and leaf links stay untouched. The C-library spelling is locally replaceable; atomic replacement or durability needs another explicit design, not an implicit extension. |

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
| U1: validated CPU definition | Accepted for the assigned definition scope: all 16 focused native cases passed under Debug ASan/UBSan, including private construction, independent copied storage, retained ownership, lookup and eager owning diagnostics. Leak checking is unavailable under ST-008; see next.md for complete logs and commands. |
| U2: explicit session | Implemented and verified: all 18 session cases plus the 16 definition regressions passed under Debug ASan/UBSan. Incarnation freshness, owning views, checked moves/lifecycle, borrowed handlers, exceptions, reentrancy, overflow and value resume are covered. A separate allocation-rejection probe passed for owned-copy failures and the post-effect path. |
| U3: console acceptance | Private headless gate, numbered selections, complete versioned JSON envelope, exclusive fresh-path save/resume and focused state/codec/file/transcript cases are implemented. Exact executed counts/results and remaining limits are in next.md; build success alone does not establish these behaviors. |

U1V completed U1 verification; U2 completed CPU sessions/values. The current U3
assignment completes only the console milestone; later scope requires coordinator selection.
[next.md](next.md) owns the acceptance evidence, limits and next useful unit.

## Acceptance and present verification

Required observable acceptance for the console milestone (execution status in next.md):

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
partially mutate before throwing; cross-platform console/C-library behavior remains
unverified, and saves intentionally offer neither replacement nor durability.
U1V completed the assigned
definition acceptance: the incremental `tests-engine` target succeeded, CTest
registration selected exactly 16 cases, and all 16 actually ran and passed, with
no skipped or unrun cases. The header-first caller, ownership, lookup, invalid
unreachable data and retained deterministic diagnostics have executable evidence.
LSan's reproduced tracing failure required the command-local ST-008 accommodation;
ASan/UBSan instrumentation and halt-on-error remained active. Leak detection is
unavailable. That accepted U1 only; console/JSON was pending at that checkpoint.
No U1 source, repository build configuration,
dependency pin or global setting changed during U1V. Actual commands, logs,
coverage and the next coordinator-selected unit are recorded in [next.md](next.md).

U2's [session implementation](../projects/engine/src/core/game-framework/dialogue-session.cpp)
and header-first [session cases](../projects/engine/tests/unit/src/dialogue-session.cpp)
now provide actual CPU session evidence: 18 session cases and all 16 definition
regressions ran and passed, with no skipped/unrun cases. The separate
[allocation probe](../projects/engine/tests/probes/dialogue-session-allocation.cpp)
also compiled and ran successfully under ASan/UBSan. It verifies its scalar-new
rejection hook, uses long strings to expose hidden copies, checks view/snapshot
copy failures leave healthy unchanged state, and rejects allocations from effect
return through successful cursor/step commit. This is a small isolated executable,
not a runner-wide allocator override or general failure-injection framework.
The source catch path rethrows before cursor assignment, retaining the source
when faulted; native cases verify original exception identity and subsequent
fault rejection. Leaks remain unverified under ST-008. U2 did not implement console,
JSON or files; U3 now owns that private boundary. Runtime coupling and a durable
external-effect protocol remain outside the milestone.

U3's [private caller and gate](../projects/apps/dialogue-console/src/gate.cpp),
[strict codec](../projects/apps/dialogue-console/src/codec.cpp),
[file boundary](../projects/apps/dialogue-console/src/files.cpp) and
[console](../projects/apps/dialogue-console/src/console.cpp) now have executed
acceptance: 4 state, 9 codec, 6 file and 7 transcript cases passed alongside all
34 definition/session regressions, 60/60 with no skips. All ten spawned console
processes exited 0 inside the wrapped native envelope. The same-owner resumed
gate/pass transcript reports one total introduce effect; a fresh process restoring
that save/pass reports zero introduce effects. Initial/terminal saves, stale/invalid
commands, every malformed-document case's unchanged pair/source/token authority,
real exclusive file collisions/links, and deterministic write/flush/close/cleanup
failure behavior were exercised. No implementation fix, Engine API change, broad
suite or global sanitizer setting was needed. Source/test registration, full logs,
immediate statuses and native coverage limits are in [next.md](next.md).
Leak checking and other platform/C-library behavior remain unverified. This accepts
only the assigned U3 console milestone; further work needs coordinator selection.
