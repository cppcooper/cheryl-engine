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
[decisions](decisions.md) use the same `ST-001` through `ST-032` IDs;
[next](next.md) records the next coordinator review action.

U10 now proves private offline creative candidate validation/projection with two
manual ferry sets and a real headless consumer: 176/176 focused checks pass,
including all 154 U9 regressions. ST-021 through ST-024 below record its creative
scope, authority, ownership and bounded reference state; next.md owns actual
commands/results and the idle coordinator-review handoff.

U11 adds a playable, consequence-sensitive watch continuation composed from
shared manual ingredients. All 176 regressions passed; the 18 new checks have
passing execution after correcting one new authority-test expectation. See
ST-025 through ST-028 and next.md for actual attempts, limits and idle handoff.

U12 now composes a playable eight-node relief bargain from shared manual sets.
All 211 focused checks pass once, retaining all 194 prior names and 75 old child
transcripts. ST-029 through ST-032 record agency, pending obligations, retained
meaning and resource boundaries; next.md owns actual commands and queue handoff.

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

STORYTELLING-U4 authorizes loading one explicit bounded authored definition in the
private consumer before startup, preserving Engine and U3 save/session contracts.
Its implementation and exact format are below; 23 new plus all 60 accepted cases
passed (83/83), with full source/log evidence in [next.md](next.md).
No new personal preference evidence is
inferred from this format or the existing agent-authored APIs.

STORYTELLING-U5 authorizes private headless authored-content inspection only, from
accepted U4 `509bfcb12157815f621dae8d1cd07c3622b43694`. ST-013/014 below describe
its implemented report and cost boundary; all 15 new plus 83 retained cases passed
(98/98), with 54 expected child statuses. [next.md](next.md) records full evidence.

STORYTELLING-U6 authorizes reusable bounded Engine inspection from accepted U5
`24f9e04b2e4f912be6e0c56ec2cca009b971c16e`. The same advisory semantics now have
an Engine-owned CPU operation, explicit typed resource budgets, a thin private
adapter and independent actual-Engine consumer. ST-013/014 are updated and ST-015
records the export/consumer boundary; next.md distinguishes executed checks.

**ST-001 — Small reusable boundary.** U1/U2 implement CPU dialogue definition
validation and an explicitly advanced session; U3 adds the private console adapter
under `projects/apps/dialogue-console/`, with a default-OFF
`CHERYL_BUILD_DIALOGUE_CONSOLE` option. It links only Engine; its state library and
test cases explicitly include pinned JSON 3.11.3 privately. Root options/registration,
the tutorial-game's `cheryl_add_test_suite` pattern and Engine's helper were inspected
before wiring. No optional modules, Workers, public header changes or SDK export
are needed. The app owns its own gate fixture, not an import from a sibling. U4
adds explicit startup-only authored input and a second private crossroads fixture;
the no-argument gate and paired commands remain compatible. There is no implicit
asset discovery, live replacement, callback-time I/O or new Engine content service.
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
also borrow frozen storage. Exact `find_node` and node-scoped `find_choice` now use
immutable sorted ordinal indices (ST-016), return null on misses and never allocate
or normalize names. U7 replaces the original tiny-graph linear scans, preserving
all borrowed pointer identities and authored order. Retain a handle through
borrowed access; the private entry ordinal remains independent of lookup sorting.
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
Handlers/captures must survive the call; reentering view/either choice API/snapshot returns
`Busy`. Idle move construction/assignment preserve incarnation and are noexcept;
moved-from operations return `MovedFrom`. Active moves, replacement or destruction
terminate before invalidating storage beneath a callback. This hard lifecycle
precondition makes paired idle adoption nonthrowing, at the cost of unrecoverable
programming misuse; confidence in that policy is medium and reversal affects only
these initial callers. No operation supplies cross-thread synchronization.

`view` returns copied node text and authored-order choices with availability and
selection values. These values, snapshots and U9 Transition observations (ST-019)
retain their own strings and IDs;
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
scope a command; they are not a security token. `choose` and the optional
`choose_with_transition` (ST-019/020) check those fields,
terminal/fault/busy/moved-from state, membership and step
capacity, then reevaluates the condition against current application state.
A cached enabled row is insufficient. Rejections are explicit results such as
`WrongSession`, `WrongIncarnation`, `StaleSelection`, `UnknownChoice`, `Unavailable`,
`Finished` or `StepExhausted`, leaving progress and effects untouched. Terminal
views/snapshots and maximum-step snapshots remain valid; choosing cannot advance
a terminal or overflowed cursor.

Complete all lookup, validation, allocation and success-result preparation before
calling an effect. Legacy success is `expected<void, SessionError>`, prepared
before entry with statically checked nonthrowing copying and no owned payload.
The optional U9 result is `expected<Transition, SessionError>`: all owned fields
and that entire success value are prepared before entry, then moved without
throwing (ST-019/020). Legacy choose constructs no unused observation.
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

## U4 authored definitions before encounter construction

**ST-011 — One chosen file, existing compilation semantics.** The private
[loader](../projects/apps/dialogue-console/src/definition-codec.cpp) reads an
explicit path, structurally decodes owned Draft values, then calls the unchanged
CPU `compile` with `operation_keys()`. That private helper is shared with the
built-in gate and supplies exactly condition `introduced` and effect `introduce`.
JSON cannot declare handlers or expand the allowlist. The accepted compiler still
owns ID/reference/scope, node-shape and operation validation. Empty IDs remain
compiler errors; empty text and explicit cycles remain valid. Structural errors
stop decoding early. Semantic failures retain every original Diagnostic, including
definition/revision, authored indices, code, field and offending value. A private
DefinitionError owns origin, structural field/message/node/choice context and the
complete semantic vector. Formatted diagnostics keep the origin and row location;
none borrow bytes, a draft or parser token storage.

The small caller in main is equivalent to the header-first consumer case:

```cpp
auto loaded = GateConsole::load_definition(path);       // owns frozen definition or errors
if (!loaded) {
    std::cerr << GateConsole::format_definition_error(loaded.error()) << '\n';
    return 3;                                         // no Encounter, input or effects
}
GateConsole::Encounter encounter(std::move(*loaded), trace ? &effects : nullptr);
return GateConsole::run_console(encounter, std::cin, std::cout);
```

In interactive mode the CLI compiles either the explicit path or built-in gate
before creating Encounter. Options --definition PATH and --trace-effects may each
appear once in either order. Missing/duplicate/unknown options exit 2; read/decode/
compile rejection exits 3 before command processing; later unexpected exceptions retain the existing
exit 1 behavior. A supplied bad path never falls back. No argument preserves the
built-in encounter. This is a medium-confidence private CLI choice, not user API
taste; its reversal cost is confined to this consumer/tests.

Authored JSON is a complete, separate definition_schema 1 document. Root fields
are exactly definition_schema (integer 1), id (string), revision (positive uint64),
entry (string), nodes (ordered array). Each node requires id, text, kind (prompt or
terminal), choices (ordered array, explicitly empty for terminals). Each choice
requires id, text, target; optional condition/effect values are strings when present,
never null, bool or an expression. Required fields cannot be inferred. Properties
may appear in any order, but duplicate decoded names, unknown/missing fields and
wrong shapes/types reject at every object level. This does not change public
Draft/Session semantics or provide a general authored-content service.

[gate.dialogue.json](../projects/apps/dialogue-console/fixtures/gate.dialogue.json)
contains the existing four-node encounter. The second complete small example,
[crossroads.dialogue.json](../projects/apps/dialogue-console/fixtures/crossroads.dialogue.json),
is deliberately different data rather than a second hard-coded path:

```json
{
  "definition_schema": 1,
  "id": "crossroads",
  "revision": 7,
  "entry": "arrival",
  "nodes": [
    {"id": "arrival", "text": "Sunset at the crossroads.", "kind": "prompt", "choices": [
      {"id": "rest", "text": "Rest here", "target": "camp"}
    ]},
    {"id": "camp", "text": "You make camp.", "kind": "terminal", "choices": []}
  ]
}
```

The compiled handle retains a private frozen copy after input bytes and decoded
draft destruction/mutation. Encounter retains the chosen definition for start and
every resume. Existing exact save definition/revision/node checks therefore apply
to loaded content too; a valid revision 2 file does not authorize a revision 1 save.
Neither author loading nor restore infers flags, authenticates history, replays
effects or creates a save. A same-ID/revision edited definition remains an
application revision-discipline risk, not something this codec detects.

**ST-012 — Bound the owned draft and validation work.** All bounds are decoded
UTF-8 byte counts where strings are concerned, without normalization or coercion:

| Input bound | Reason and enforcement |
| --- | --- |
| 65536 document bytes | Bounded read allocates exactly cap plus one detection byte before opening; checks read/close, rejects oversize before parsing. No writes or sidecars. This also bounds nlohmann's lexer token buffer before callbacks. |
| 256 nodes / 2048 total choices | Check before emplacing each row; total choice budget is across nodes, not per node. Bounds draft vectors and existing compiler work before freeze. |
| 128 bytes per ID, operation key or property name | Reject before moving values into the draft; supports opaque UTF-8 IDs without unlimited identity/key copies. Empty identities still reach semantic validation. |
| 4096 bytes per node/choice text | Allows console prose while bounding each owned string and its freeze copy. Text may be empty. |
| 32768 aggregate node/choice text bytes | Check remaining budget before accumulation; limits repeated text copies in the draft and frozen definition. IDs are independently bounded by per-field and document caps. |
| Five container levels | Exactly document → nodes array → node → choices array → choice. Fixed SAX frames reject deeper objects/arrays before allocating recursive content. No arbitrary DOM is constructed. |

Raw NUL is rejected across the entire byte span before parsing, including valid
JSON followed by NUL, NUL+junk or NUL+another document. Strict full-document parsing
rejects truncation, trailing junk/second documents, comments, wrong types/numeric
booleans, noninteger/unsupported schemas, zero/negative or overflowing revision.
Decoded key bitsets detect escaped-equivalent duplicates. String and aggregate
limits run before draft adoption; valid structure is compiled eagerly, including
unreachable rows, without callbacks. Rejected inputs expose no usable definition.

2048 shortest complete choice objects alone need 65535 bytes before their node/
document overhead, so the fixed document cap is stronger. Keep the assigned total
choice cap as defense rather than claim a production-boundary input can reach it.
A private Testing decoder can only lower that one budget; the same SAX guard is
tested at an exact smaller global boundary and +1, with all production byte/string/
node/depth checks retained. The real shortest 2048-choice document rejects as
oversized. This seam is local test coverage, not a way to relax caller limits.

Confidence is high in bounded ownership and staged startup, medium in the chosen
text caps/format longevity. Limits and private spelling are locally reversible;
changed schema meaning needs an explicit version. A DOM/pruning callback was
rejected because it can keep parsing arbitrarily deep ignored content. Another
compiler/loader in Engine would duplicate semantic authority and expand scope.
U4 instead changes only private app startup, data and tests, preserving the save
codec, file-save guarantees, public headers, Session/incarnation/fault contracts
and Runtime. Native results and portability/leak limits are in next.md.

## U5 advisory authored-content inspection

**ST-013 — Own structural facts without starting play.** U6 moves the topology
operation/report into Engine; the following private command and semantic contract
are retained.
`dialogue-console --inspect-definition PATH` uses the unchanged bounded loader,
operation-key allowlist, validation order and owned diagnostic formatter. It
compiles before analysis and returns before Encounter/Session construction, handler
creation or stdin processing. This mode accepts no --definition, --trace-effects
or extra arguments. Existing interactive options remain compatible. Usage exits
2, rejected files exit 3, valid advisory reports exit 0; unexpected exceptions
retain exit 1. No predicate/effect, flag interpretation, save/adoption, source
write, generated sidecar, implicit search or content repair occurs.

The header-first report consumer implements this small caller; errors return
before accessing a rejected value:

```cpp
auto definition = GateConsole::load_definition(path);
if (!definition) {
    std::cerr << GateConsole::format_definition_error(definition.error()) << '\n';
    return 3;
}
auto report = GateConsole::inspect_definition(**definition);
if (!report) {
    std::cerr << path << ": " << report.error() << '\n';
    return 3;
}
std::cout << GateConsole::format_definition_report(*report);
```

Literal gate output is stable and independent of command input:

```text
Definition: "gate-encounter"
Revision: 1
Entry: "approach"
Nodes: 4
Choices: 4
Terminals: 2
Entry-reachable (4): ["approach", "gate", "admitted", "departed"]
Unreachable (0): []
No terminal route (0): []
Advisory topology: conditions are ignored; no proof of gameplay reachability, effect history, termination or guaranteed completion.
```

The Engine InspectionReport (aliased privately as DefinitionReport) owns definition
ID, revision, entry, counts and all three ID lists, retaining neither the compiled
definition nor input/draft buffers.
Copies remain independent. Lists follow authored node order, never queue order.
Entry is the actual `Definition::entry()` node even when nonfirst. Terminals are
exactly `NodeKind::Terminal`; the empty path gives every terminal a route to
itself. Each authored choice is one directed edge, including parallel targets,
conditional rows and self-loops. Reverse analysis seeds **all** terminals, even
unreachable ones. Thus unreachable terminals are not mislabeled as lacking a
terminal route, and reachability and terminal-route lists answer different questions.
IDs are quoted with quote/backslash/C0/DEL escaping, and counts use the classic
locale, preserving one deterministic line per field.

Conditions are ignored rather than evaluated with invented flags. The gate's
pass edge counts even before introduction; a conditional-only exit also counts.
A cycle with an exit has a structural terminal route but can loop forever by
choice; a closed cycle can be intentional and remains valid. Neither positive
nor negative topology results prove gameplay feasibility, effect history,
termination or guaranteed completion. Report advisories never become compiler
warnings/errors. U6 adds an Engine observation operation without changing
Definition/Session or compilation semantics.

**ST-014 — Bound reusable Engine inspection.** The self-contained CPU header
[dialogue-inspection.h](../projects/engine/include/cheryl/core/game-framework/dialogue-inspection.h)
adds one exported operation in CE::Dialogue. A small public-only caller is:

```cpp
#include <cheryl/core/game-framework/dialogue-inspection.h>
// definition was successfully compiled and remains alive during this call.
auto result = CE::Dialogue::inspect(*definition, CE::Dialogue::InspectionLimits{});
if (!result) {
    // Inspect result.error(): a typed CE::Dialogue::InspectionError, no partial report.
    return;
}
const auto& report = *result; // owns every ID/list; definition can now be released
```

InspectionLimits is independent of private JSON constraints. Each call explicitly
budgets these resources; omitted limits use the defaults. Zero is a restrictive
budget, never an unlimited sentinel. Requests above **any** hard ceiling return
InvalidLimits before input traversal or allocation, including size_t-max requests.
Accepted larger budgets allocate for actual input, never the configured maxima.

| Budget | Default | Absolute supported maximum | Meaning |
| --- | --- | --- | --- |
| nodes | 256 | 4096 | All authored nodes, including unreachable nodes. |
| choices | 2048 | 65536 | All authored directed edges, including parallel/self/conditional edges. |
| copied_id_bytes | 131072 | 4194304 | Exact repeated definition/entry/list ID characters in the resulting report. No per-ID 128-byte Engine restriction. |
| storage_bytes | 1048576 | 16777216 | Accounted peak requested scratch/report payload, described below. |
| work | 134217728 | 1073741824 | Deterministic charged units, not CPU instructions or a time limit. |

InspectionError distinguishes InvalidLimits, NodeLimit, ChoiceLimit, CopiedIdLimit,
StorageLimit and WorkLimit. No refusal yields a usable partial report or mutates
the frozen definition/earlier reports. The operation borrows Definition/limits
only during the synchronous call and retains neither owner. Successful reports
add InspectionUsage with exact charged ID bytes, payload bytes and work. Default
budgets cover the old console maximum, including its <=128-byte identities; larger
valid Engine definitions can raise budgets within the hard ceilings. No handler,
flag/session, path/JSON, formatting or generic graph abstraction enters Engine.

The single production implementation is
[Engine dialogue-inspection.cpp](../projects/engine/src/core/game-framework/dialogue-inspection.cpp).
It first bounds nodes, subtraction-checks edge accumulation and sums the minimum
output ID bytes (definition, entry, all nodes once) using remaining budget. Hard
ceiling/product checks bound all size arithmetic, including 32-bit size_t. It
checks minimum output plus scratch storage/work before allocation. One contiguous
vector holds forward/reverse CSR offsets and adjacency, write cursors, a queue and
two visited sets. For N nodes/E choices its requested scratch is exactly
`(2*E + 6*N + 3)*sizeof(size_t)`; no recursion or input-controlled nesting exists.
Compiled identity/reference/kind guarantees supply valid entry/target membership;
no compiler validation is duplicated. Linear target lookup charges each candidate
and each actually compared byte, including early mismatch/length outcomes.

Forward traversal starts at the compiled entry; reverse traversal seeds every
explicit terminal. Visited is set before enqueue, bounding each queue to N and
traversal to O(N+E). Lookup is O(N E L), bounded additionally by the work budget,
where L is the compared ID prefix length. Final partition counts determine exactly
which IDs repeat in no_terminal_route; those bytes and exact output slots are
checked **before** any report allocation/copy. A budget may therefore refuse after
bounded scratch/traversal, still returning no partial output.

For S = N + no_terminal_route.size() and B = repeated copied ID bytes, charged
peak payload is `scratch + sizeof(InspectionReport) + S*sizeof(NodeId) + B + S+2`.
It includes report/vector objects, requested vector slots, string bytes and one
terminator per ID. SSO may make this conservative; allocator metadata/capacity
rounding, fixed stack, borrowed input, compiler/loader and caller formatting are
excluded. This is an explicit payload budget, not a promise of exact resident
memory or allocator behavior. Hard caps keep aggregate arithmetic small; even a
maximum requested budget does not reserve/allocate that many bytes in advance.

Work charges one per row/edge visit in the preflight, adjacency, reverse-fill,
seed, partition, flood and report passes; one per scratch-initialized/prefix/
cursor-copy index; one per lookup candidate/actually compared byte; one per report
list append; and each copied ID byte/terminator. Report-copy work is charged from
exact lengths/counts before copying. Constant bookkeeping and allocator internals
are excluded. This model measures actual selected operations, not a worst-case
N*E reservation or wall time. Engine cases distinguish equal-length graphs with
early versus late byte mismatches, and exact usage versus one-unit-short budgets.
std::bad_alloc/other allocator exceptions propagate; there is no catch-all,
rollback claim or reusable failure-injection service. All prior values remain
untouched while local scratch/report construction unwinds.

U5's accepted prototype used private fixed arrays with 41496 bytes of index/visited
element storage on this 64-bit build, at 256/2048 and 128-byte per-ID caps. U6
replaces that fixed allocation and inferred cost bound with explicit budgets and
one actual-size scratch allocation; at 256/2048 the new all-size_t scratch alone
is 45080 bytes. The legacy caps still apply at the console adapter. As before,
2048 complete JSON choices cannot fit in its 64 KiB document; direct compiled
Engine fixtures exercise graph limits independently. U6 tests also accept 257
nodes, 2049 edges and 256-byte IDs under raised/appropriate Engine budgets.

A generic graph index/package and an Engine JSON loader were rejected as broader
responsibilities. Retaining only fixed 256/2048 public caps would unnecessarily
couple reusable CPU data to a toy JSON consumer. Unbounded size_t budgets would
make overflow/work refusal misleading. Confidence is high in ownership/bounded
mechanics, medium in defaults/hard ceilings/work-unit longevity; caps or charging
changes require coordinated contract/test review. No personal API-taste inference
follows from these engineering choices. Allocation exceptions, broader platforms,
SDK packaging, leak checks and runtime integration remain explicit limits.

**ST-015 — Real Engine ownership, thin compatible consumer.** The new source is
owned by the actual Engine `src/*.cpp` CONFIGURE_DEPENDS glob and normal CE_API
boundary. It is not compiled into a private alternate implementation/archive.
The existing public Definition observation APIs and compiler/session source remain
unchanged. The console aliases InspectionReport and delegates to Engine inspect;
only its ordered legacy node/ID/choice-cap preflight, string error translation and
unchanged formatter remain private. There is one production graph analysis.
The small legacy preflight preserves exact error precedence/bytes for direct
private callers and is bounded to 256 rows without walking graph edges. All
private JSON schemas/keys, built-in/loaded interactive mode and paired saves stay
with their original owners.

The separate `tests-dialogue-inspection` suite is registered with the existing
Engine helper, includes the public header **first**, and links actual Cheryl::Engine.
It has no private Engine/app/JSON include or export-macro override. It executes
returned report values, retained ownership and typed failures. In this Linux shared
configuration its public_header case also checks dladdr's actual function owner
against the real Engine target file; this probe is test-only. Generated compile
rules establish Engine-only source ownership/export flags and consumer import
flags. Native nm/readelf records establish defined Engine inspect, undefined
consumer import and NEEDED libcheryl-engine.so.4; next.md preserves commands,
complete outputs and the resolved audit-matcher failure. These are actual source/
link/execution observations, not SDK/static/Windows acceptance. Keeping normal
Engine source ownership costs its existing dependency/link closure; this operation
is CPU/JSON-free, not a promise of dependency-free Engine packaging. Confidence
is high for executed Linux shared ownership; other platform/ABI/SDK work remains
unverified and requires its own unit.

## U7 immutable lookup indices

**ST-016 — Sort ordinals after validation and owned copying.** The unchanged
caller remains:

```cpp
#include <cheryl/core/game-framework/dialogue.h>
const auto* choice = definition->find_choice({"gate"}, {"pass"});
if (choice) {
    // Borrowed authored Choice; retain definition while using it.
}
```

The definition's private constructor deep-copies Draft first, then builds a sorted
node-ordinal vector and one sorted choice-ordinal vector per authored node. Every
comparison reads the private copied strings. No caller string_view, pointer into a
draft or mutable lookup cache survives construction. The authored vectors/strings
are not sorted, moved or reallocated by index construction or lookup; indices hold
only size_t ordinals. Published Definition remains actually const and immovable.
The public find_node/find_choice signatures, noexcept, null-on-miss, exact identity
and pointer lifetime contracts are unchanged. Choice indices are indexed by the
**authored node ordinal**, so repeated choice names in different nodes remain
independent. Missing nodes, missing choices and terminal empty-choice indices all
return null. Entry still uses its validated authored entry_index_, not the first
sorted name. All index storage is ready before first lookup; there is no lazy path.

compile's validation/reporting body stays byte-identical to accepted U6: definition
id/revision, supported conditions/effects, entry, then authored node/choice rows.
Only after complete successful validation does new const Definition deep-copy and
build indices. Allocation/copy/index-construction/control-block exceptions propagate
normally; C++ constructor/RAII cleanup prevents a partial usable Definition from
being published. No new authoring errors or changed diagnostic order are introduced.
Lookup performs lower_bound over ordinal vectors using std::string lexical order,
then checks exact equality. It creates no key/string/vector copies and calls no
allocator, handler or Session operation. Core IDs remain opaque nonempty strings:
long common prefixes, embedded NUL and high bytes are neither truncated nor
normalized. Query-key construction belongs to the caller, before entering lookup.

For N nodes and Ci choices in node i, retained index payload is
`(N + sum(Ci))*sizeof(size_t)` plus N vector control objects and allocator/capacity
overhead. Per-node vectors avoid new aggregate edge-count arithmetic and keep
scope/order mapping direct; terminals need no choice-element allocation. Sorting
costs O(N log N + sum(Ci log Ci)) string comparisons after linear ordinal filling.
Node lookup uses O(log(N+1)) comparisons plus final equality; choice lookup adds
O(log(Ci+1)). Each comparison may inspect a common prefix, so byte work is
O(Ln log(N+1) + Lc log(Ci+1)) for the two relevant ID-prefix lengths. These are comparison
bounds, not constant-time or measured speed claims. No timing campaign was run.
Construction/index storage lies outside inspect's existing per-call payload/work
budgets. Inspection's explicitly charged manual target comparisons remain unchanged;
no attempt is made to substitute uncharged core lookup into that contract in U7.

Session::choose still prepares choice/target lookups before the effect and commits
only its nonthrowing cursor/step afterward. No live-incarnation/authority, effect,
flags, JSON/file, snapshot or application policy changes. Private Definition layout
grows, with matching-header Engine/consumer rebuilds; exported public declarations
and new public facilities are not redesigned. Mixed old binaries/installed SDK
compatibility is not claimed by this local build.

Alternatives: sorting authored vectors would break pointer/order semantics;
retained string-key maps would add key/hash/collision policy and potentially copied
identity storage; a lazy cache would introduce mutation/synchronization and a first
lookup allocation path. One flat choice array could reduce allocator/control
objects, but needs offset bookkeeping and gives no new observable guarantee here.
The selected per-node vector arrangement follows existing authored ownership with
no generic indexing service. Confidence is high in semantics/ownership, medium in
that internal memory tradeoff. A later private storage change is local, but requires
Engine/consumer rebuild and lookup/ownership/diagnostic/allocation checks. This is
an engineering choice, not additional evidence of user API taste.

Focused source cases cover 17 shuffled nodes and 31 permuted choices per prompt,
all hits with exact authored addresses, misses/boundary names, independent scoped
choice IDs and illegal duplicates within a node, 4096-byte common prefixes with
NUL/high bytes, original draft destruction, retained-owner/pointer stability and
an eleven-diagnostic literal sequence after source destruction. A separate public
header-first actual-Engine consumer exercises the unchanged caller and retained
long/binary IDs through exported lookups. The isolated Linux allocation probe
prepares keys/expected storage before denial, verifies scalar/array/aligned controls,
then makes **first-use** and repeated hit/miss/terminal lookups with C++ allocation
rejected. A separate failed Engine compile confirms that shared-library allocations reach
the hook too, before any lookup warms up. It is never linked into GoogleTest,
does not exhaust memory and does not claim arbitrary C/custom allocator instrumentation. Actual results belong to next.md.

This is only STORYTELLING-U7 in finite batch three-commits-20261024. The pinned
plan/queue policy were read without changing coordinator records. A committed
ready receipt is written only after focused acceptance; the controller, not this
worker, selects U8 after the current run drains. The receipt reports execution,
not automatic coordinator source/platform acceptance.

## U8 bounded advisory structural routes

**ST-017 — Return an owned shortest structural route, never playback authority.**
The new public-header caller (implemented and executed) is:

```cpp
#include <cheryl/core/game-framework/dialogue-route.h>
const auto route = CE::Dialogue::find_route(*definition, {"start"}, {"end"}, {});
if (route) {
    // For the literal graph: nodes = start,z,end; choices = c0,c0.
    // choices[i] is scoped to nodes[i], targeting nodes[i+1].
}
```

Route owns definition ID/revision, ordered NodeIds and ChoiceIds, plus resource
usage. Its two sequences have sizes edges+1 and edges, with each choice explicitly
scoped to the corresponding source node. No DefinitionHandle, borrowed parser or
input string storage, Selection/incarnation, session ID or gameplay flag survives
in the result. Callers supply a live const Definition reference and stable endpoint
IDs for the call; there is no nullable handle overload. Independent frozen reads
can run concurrently. Earlier routes and the definition stay unchanged on every
refusal or exception; result adoption belongs to the caller.

Iterative BFS uses one bounded FIFO and first-discovery parent-node/choice ordinals.
Edges are visited in authored choice order; FIFO discovery determines equal-length
ties, including parallel choices. Sorting IDs/authored node positions never chooses
a route. Arbitrary endpoints need not be entry or terminal. Equal known endpoints
return the one node and zero choices, without traversing outgoing edges. Self edges,
cycles, repeated choice names across nodes, conditional/effect-bearing edges and
terminal-free definitions remain valid. UnknownFrom, UnknownTo and Unreachable
are distinct ordinary errors, with no partial usable output. Structural paths
ignore every condition: they prove no actual availability, effects, resume history,
termination or completion. The operation accepts no handlers and constructs no
Session; an unavailable live choice remains rejected by Session.

Own implementation in the real Engine source glob, CE_API on find_route, and a
self-contained CPU/JSON-free header. A public-header-first consumer links the actual
Cheryl::Engine and tests returned values, owned lifetime, typed refusals and shared
symbol ownership; it does not patch exports, copy source or include private app/JSON
headers. Existing inspection/console/session implementations and all file schemas
remain unchanged. Two obsolete detached lookup comments from U7 are corrected to
match its already-implemented comparison contract; no lookup behavior changes.

Alternatives: DFS can return a longer route; all-pairs/prebuilt adjacency adds
retained policy/storage unrelated to one query; a generic graph/A* service imports
unneeded costs and heuristics. Returning live choices would confuse advisory
structure with session authority. Explicit parallel arrays avoid copying source
IDs twice and preserve existing scoped ChoiceId meaning. Confidence is high in
BFS/ownership and medium in type spelling; this is engineering judgment, not user
API taste. Changing result spelling affects this small consumer, with no save/file
migration. U9 transition observations remain a separate queued assignment.

**ST-018 — Budget complete input counts, indexed search and owned output.**
RouteLimits defaults are 256 authored nodes, 2048 total choices, 255 output edges,
128 KiB repeated output-ID bytes, 1 MiB requested payload and 128 Mi work units.
Hard request ceilings are 4096 nodes, 65536 choices, 4095 output edges, 4 MiB ID
bytes, 16 MiB payload and 1 Gi work units. Larger compiled CPU definitions than the
private JSON caps are supported up to these ceilings when callers raise allowances.
Every ceiling is independent; zero permits none. No private loader/schema policy
is imported. Above-ceiling and size_t-maximum requests return InvalidLimits before
traversal/allocation. There is no allocate-to-budget behavior.

Preflight counts all authored rows and choices, even unreachable ones, using checked
subtraction/addition. Source/destination lookups charge work before exact U7 immutable
indexed lookup. Each lookup reserves `(bit_width(N)+1)*(1+query_bytes)` units: at most
bit_width(N) lower_bound string comparisons plus final equality, each bounded by
query length plus fixed length comparison. This is a conservative byte/comparison
reservation, not actual prefix measurements or constant-time lookup. Multiplication
uses division against remaining work before evaluating the product. Long unknown
query keys therefore cannot escape work accounting. A future lookup algorithm
change must revisit this bound; U7 is a verified prerequisite, not a cache assumption.

After endpoints are found, check copied definition/source minima and scratch/minimum
output payload before allocating. A single 3*N size_t scratch vector owns parents,
parent choice ordinals and FIFO; unique discovery bounds queue occupancy by N.
Checked count/product arithmetic precedes allocation; no recursion or extra edge
storage is used. Each examined edge charges a visit and indexed target lookup.
Search ends at first destination discovery or queue exhaustion. Output-edge budget
is checked after shortest-path discovery; it never truncates search or converts a
resource refusal into Unreachable. Parent walks determine exact length and repeated
ID bytes before result vector/string allocation. Assembly reserves all remaining
initialization/copy visits, ID bytes and terminators first.

For N nodes and R returned edges, requested peak payload is
`3*N*sizeof(size_t) + sizeof(Route) + (R+1)*sizeof(NodeId) + R*sizeof(ChoiceId)
 + copied_id_bytes + 2*R+2`. This counts one terminator for definition and each
returned ID. Borrowed input/Definition indices, fixed stack, allocator bookkeeping,
rounding and implementation-specific string capacity are excluded; it is an honest
payload budget, not RSS/exhaustion prevention or a timing guarantee. Work also
charges preflight rows, initialized scratch/result slots, BFS nodes/edges, path
walks and output copy visits/bytes/terminators. Search is O(N+E) traversal plus
O((E+2)*log(N+1)*L) reserved query byte work and output copying. Usage is independent
of unused budget headroom. There is no timing campaign.

Ordinary resource errors distinguish NodeLimit, ChoiceLimit, OutputLimit,
CopiedIdLimit, StorageLimit and WorkLimit. Failure precedence is hard ceilings,
authored counts/work, source then destination lookup, minimum payload, search,
then output count/ID/storage/copy work; resource refusal can precede an endpoint
or unreachable diagnosis. std::bad_alloc/other allocation exceptions propagate;
violating immutable compiled target membership is a programming-invariant
std::logic_error, not an ordinary content error. Nothing partial is published.

Confidence is high in checked bounds and finite BFS, medium in budget defaults,
ceilings and conservative lookup charging. Reversing those engineering choices
needs public limit/usage contract and boundary-test review, but no file-format or
application-policy migration. These choices extend ST-014's bounded CPU discipline
without changing inspect's work accounting or making a general budget framework.
Actual checks, pass counts, limitations and receipt status belong to next.md.

## U9 owned successful transition observations

**ST-019 — Observe a successful checked choice, without delivery or replay authority.**
The implemented and executed public caller is:

```cpp
#include <cheryl/core/game-framework/dialogue.h>
const auto offered = session.view(test).value();
const auto transition = session.choose_with_transition(offered.choices[0].selection, test, apply);
if (transition) {
    // Independently owned definition/revision, session, source/target,
    // source-scoped choice and old/new completed-boundary steps.
}
```

Transition owns DefinitionId, definition_revision, SessionId, source/target NodeIds,
source-scoped ChoiceId and old_step/new_step. It retains neither DefinitionHandle
nor Session, handlers, Selection/incarnation marker, application flags, operation
meaning or a save format. Source equals target for a self-cycle; new_step is exactly
old_step+1 after capacity validation, including max-1 to max. No observation exists
on rejection or exception. Later copying/logging a returned observation may fail
separately; that cannot reverse the already committed choice. The result can outlive
source bytes/draft, definition,
session and presentation; copied observations are independent values. Logical IDs
and steps are not authenticated history or proof of one active process. Copying or
editing this value cannot manufacture a live Selection, replay a path or validate
an application snapshot. Structural routes likewise confer no selection authority.

The explicit named opt-in returns std::expected<Transition, SessionError>, rather
than changing choose's expected<void,SessionError> or allocating an unused receipt.
Normal gameplay still requires a Selection actually issued by view and a current
condition recheck. Both variants have identical error ordering, missing-handler,
step/terminal, moved-from, busy/fault and stale/foreign/incarnation checks. Stable
keys are borrowed synchronously. Start/view/snapshot/resume and result observation
never invoke effects. There is no automatic publication/event bus, callback registry,
durable delivery, external rollback or exactly-once crash protocol. Application
persistence/flags/outcome routing remain outside Engine.

The small real shared Engine consumer executes the new exported member, inspects
fields, releases the final session/definition owner, and tests rejection of tokens
from a prior resume incarnation. The header remains CPU/JSON-free, with no runtime
module or private app include. Session object data layout and legacy source call
syntax are unchanged; only the new member/type are exposed. Installed/mixed-version
ABI and other platforms are not inferred from this local rebuild.

Confidence is high in ownership/authority separation, medium in field/method
spelling. These are engineering choices driven by the task and current compatibility,
not additional evidence of personal taste. An optional callback/event would add
failure and delivery policy; an always-owned choose result would charge every
legacy caller. Changing this new shape is local to the new callers and needs no
save migration, while adding durable effects would require a separate assignment.

**ST-020 — Prepare the complete selected success before effects, share checks.**
A private bool-template choose_impl has exactly two compiled variants, void legacy
success or owned Transition success. Existing validation through pure condition
recheck and target/next-step preparation stays in one path and in the previous
order. Only the observation variant constructs owned fields and its complete
expected success, before effect invocation. The result is non-const so return uses
NRVO or implicit move; Transition and expected<Transition,SessionError> have static
nothrow-move checks. Public wrappers return the matching result directly, without
post-effect conversions or payload copying. No generic result/persistence framework
or public template extension is introduced. A runtime nullable receipt pointer could
share checks, but would split success construction/validation across wrapper and
implementation; duplicated choice bodies would risk divergent rejection/fault rules.

The existing BusyGuard covers validation callbacks and owned-result preparation.
A guard throw or any copy/allocation failure before effect entry propagates, leaving
a healthy unchanged cursor and no effect. An effect exception retains the source
cursor, marks faulted and rethrows the original exception; app state may already
be partial, and no receipt escapes. No rollback or retry is claimed. After effect
success only numeric cursor/step installation, nonthrowing result movement and
busy cleanup remain. Legacy choose has no receipt copies/allocations before or
after effects. Callers must still serialize the session and app state and keep
borrowed handlers/arguments valid during the call. Moving/replacing/destroying an
active session, including in its callbacks, remains prohibited; reentrant normal
operations predictably return Busy.

The isolated Linux C++ allocation probe uses only bounded local failures: verify
scalar/array/aligned rejection and shared Engine interposition, then sweep at most
32 preparatory allocation positions on long-field observations. Each injected
failure must precede effects, preserve the snapshot and leave the original token
usable. A successful effect enables allocation denial itself; actual receipt return
and a further result move must allocate nothing, with stable borrowed effect keys
and committed cursor. Early unavailable/wrong-session rejection and legacy choose
run under denial from entry, proving no unused observation cost. No exhaustion,
OOM, generic failure framework, custom allocator or arbitrary C-allocation proof
is claimed. Hooks never link into GoogleTest.

Confidence is high in the shared sequencing and tested language move guarantees,
medium in the private template spelling. Keeping the two variants together costs
a small implementation instantiation and adds opt-in string-copy exceptions before
effects; reversing that private arrangement requires validation-order, ownership,
exception and allocation probes. Actual executed checks and portability/leak limits
belong to next.md; no source/commit alone establishes acceptance.

## U10 private offline creative candidate projection

The explicit U10 assignment follows completed U9
`4f5e7eab4af961d95f5a04d6f4f079517e39a758`, outside the finished finite batch.
Its clean source, ready receipt, 154-case evidence and current records were read
before dependent work. The workspace's `coordination/story-generation.md` supplies
the proposal/authority and facts/knowledge/attitude decomposition; this assignment
selects the private proof, not a provider format or public story framework. These
are engineering interpretations with stated uncertainty, not new personal API
taste evidence. No Engine/public dialogue or existing console code/schema changes.

**ST-021 — Retain bounded manual proposals under host authority.** The caller
introduced before accumulating helpers is now exercised in `story_offline.caller`:

```cpp
namespace S = OfflineStory; // Private projects/apps/story-offline/src/story.h
const auto sets = S::manual_sets();             // Explicitly manual, no model call.
const auto accepted = S::accept(sets[0], S::ferry_catalogue(), S::Context{}).value();
const auto graph = S::project(accepted, 0).value();
auto owner = S::Encounter::start(graph, {"ferry-01"}).value();
const auto offer = owner.view().value();        // Actual Session-issued selections.
const auto observed = owner.choose(offer.choices[0].selection);
const auto paired = owner.capture().value();   // Idle snapshot + owned State.
owner.restore(paired).value();                 // Fresh incarnation, no reward replay.
```

`CandidateSet` owns origin, ID/revision, proposition and ordered actions. Actions
own actor/operation/condition/outcome references, structured claimed consequences,
reward references and scoped wording/NPC-response flavours. `Catalogue` is the
tiny host owner: actors Mara/Ivo/Lena, canonical pay_fare/repair_hull/decline and
can_pay/can_repair/can_decline keys, permitted passage reward and five typed
outcomes. Outcome's method defaults to Decline; invalid enum values still reject
explicitly. A candidate cannot register code or add vocabulary; its actor, method,
costs, flags, reward and attitude claims must match an approved outcome. All
catalogue rows are checked, including unused ones. Actions cannot duplicate a
mechanical Method; two flavours are paraphrases, not extra actions. At least two
actions must be initially eligible together. Initial context must be coherent,
unsettled and observer-unaware. Text may still make false promises: validation
cannot prove prose truth, and a literal promise of a crown/1000 coins grants neither.

`accept` returns an owned first typed Error with field/row/offending value capped
at 64 bytes, or an immutable AcceptedHandle. Private constructors, deleted copies,
deep copies of every source/catalogue/context value and allocation of an actual
`const Accepted` prevent a surviving mutable alias. Projection likewise has a
private factory and actual const object; it retains Accepted and the compiled
Definition. Lifetime checks mutate/destroy every source, release producer handles,
play successfully, then retain the frozen definition independently. Normal
allocation exceptions propagate without a partially published accepted artifact.
A borrowed/provider-backed content object was rejected because it would make
later play depend on producer lifetime. Confidence is high in tested ownership,
medium in the private shape; replacing it is local, with no provider/save migration.

**ST-022 — Fixed mechanics, independently chosen flavour.** `validation.cpp`
prepares an ordinary Draft and OperationKeys, then uses existing `Dialogue::compile`;
compile failures retain its complete owning diagnostic vector. Authored action
order is preserved. `eligible-ACTION`/`apply-ACTION` keys bind only to approved
typed plans through the private owner, never executable text. The initial offer
chooses a fixed reply node with its effect, followed by an explicit no-effect
Continue to fixed destination/shore terminals. With three actions the graph has
six nodes/six choices/two terminals; nothing advances automatically or routes
inside effects. Conditions are pure and structural inspection ignores them.

| Manual set / action | Cost from 8 coins, 3 planks | Final resources | Mara / Ivo / Lena attitude | Terminal |
| --- | --- | --- | --- | --- |
| tide / pay | 3 coins | 5 coins, 3 planks | +1 / -1 / 0 | destination |
| tide / repair | 2 planks | 8 coins, 1 plank | +3 / -2 / 0 | destination |
| storm / pay | 5 coins | 3 coins, 3 planks | +1 / -1 / 0 | destination |
| storm / repair | 1 plank | 8 coins, 2 planks | +4 / -3 / 0 | destination |
| either / decline | none | 8 coins, 3 planks | 0 / 0 / 0 | shore |

This composition selects different approved material/costs/relationships, not
merely different adjectives. Each action has two literal wording/response flavours
with identical mechanical keys/targets/consequences. Definition identity is
`story-SET-flavour-N`, with the set revision; changing flavour rejects old paired
state. Revision discipline is the host's responsibility, not a content hash or
proof of history. Random callback outcomes and a generic branch language would
violate the existing fixed-target boundary. Confidence is high in mechanics,
medium in this reply/Continue pacing; changing topology is cheap privately but
requires exact identity/revision and resume checks. Fun/pacing need human playtest.

**ST-023 — Facts, information and evaluation remain separate.** `encounter.cpp`
owns bounded resources and factual paid/repaired/declined/licensed passage,
three knowledge values, three beliefs and three attitudes. Mara witnesses and
approves passage; Ivo hears of it and disapproves under a deliberately mistaken
unlicensed belief; Lena remains unaware. Ivo's belief does not revoke factual
licensed passage. Decline is witnessed neutrally only by Mara. Observer roles are
fixed reference application policy, not propagation, renown or global reputation.

Encounter retains Projection and delegates to actual Session APIs, including U9
choose_with_transition. View and choice recheck eligibility without mutation.
After live resources change, an old enabled row rejects before effects; another
still-eligible issued choice can succeed. A bounded scalar `stage` prepares and
validates complete State before its statically nothrow assignment inside the
effect. Engine already prepares the owned Transition/result before effect entry;
after effect success only its nonthrowing cursor/result commit remains. The
saturating effect counter is unsaved instrumentation, not a gameplay flag/ledger.

One serialized owner captures idle healthy Snapshot and State together. Restore
validates state, constructs a fresh Session::resume, then nonthrowingly adopts
session/state. Failure preserves the old pair and live token; success invalidates
old incarnation tokens and invokes no handlers. Same-owner and fresh-owner restore
at the reply followed by Continue do not reapply cost/reward: counters stay one
and zero respectively. Edited step or observer belief does not authenticate
history or reconstruct facts. No disk format, crash durability, external rollback
or exactly-once reward guarantee is claimed. A durable ledger/global reputation
service would need a separate assignment. Confidence is high in exercised owner
sequencing, medium in fixed social policy; changes affect private interpretation.
Magic may read these authority/ownership lessons alongside U9 without branch
import; no spell system or shared backend is introduced.

**ST-024 — Explicit private allowances and a real finite consumer.** Limits are
lower-only against these starting ceilings: 8 actors, 8 outcomes, 3 actions,
4 flavours per action, 8 names per vocabulary list, 4 reward references per list,
1024 bytes per string, 32768 aggregate counted string bytes, 256 KiB requested
payload storage and 1 Mi work units. IDs additionally allow only lowercase ASCII,
digits, hyphen/underscore, at most 64 bytes. Text rejects empty/C0 bytes except
newline/tab; Unicode correctness/markup/prose truth are not validated. Zero
allowances allow none; above-ceiling/size_t-maximum requests reject as InvalidLimits
before traversal. Input is already-owned C++ data, not a new parser/format.

Preflight checks row counts before walking/copying them and checks string length
before scanning. Storage charges object/vector slots plus string bytes/terminators;
work charges rows/slots, scan/copy bytes, query-length-plus-one comparisons and a
fixed allowance for small scalar validation. Checked remaining-budget arithmetic
precedes additions/products; all counts have small hard ceilings. Projection
rechecks limits before creating Draft, reserves bounded vectors and charges
accepted retention plus private projected payload, generated names and copy work.
Some generated key/target lengths are conservatively reserved. Its usage is a
separate allowance, not incremental process RSS. Engine compiler/Definition/index
internals, allocator rounding/control blocks and stack/diagnostic overhead are
excluded; the compiler input itself is at most six nodes/six edges. Work units
are a conservative prototype model, not CPU instructions or a wall-time promise.
This avoids a generic allocation framework; ordinary allocation failures propagate.
Exact/one-short/zero/count/text/work/storage/overflow request checks establish
the declared model, not a system allocator cap. Confidence is high in tested
mechanics, medium in reversible ceilings/model; expansion needs boundary review.

`CHERYL_BUILD_STORY_OFFLINE` defaults OFF. Its private static model and real
`story-offline` executable link actual Cheryl::Engine without Engine source copies,
JSON/provider dependencies or platform modules. No arguments run eight finite
scenarios; explicit `tide|storm pay|repair|decline [0|1 [none|restore|drain]]`
selects a demonstration. Output separates Proposed/manual, Accepted, actual
issued choices, NPC prose and actual facts/observer consequences. Usage errors
exit 2 before play; a checked refusal exits 3; completed demonstrations exit 0.
No stdin action parser, scheduler or networking is involved. Native tests include
literal expected facts/attitudes/text rather than deriving expectations from plans.
Commands, complete logs, actual counts and remaining gaps belong to next.md.

## U11 consequence-sensitive playable continuation

U11 starts at the accepted clean U10 `9771d7aec720c02bbbd1ed4df9ca2566460f4386`,
outside the completed finite queue. Current workspace/clone instructions, autonomy,
memory policy, feature brief, story-generation.md and U10 source/records/evidence
were inspected. The assignment selects creative continuity, shared composition
and the real private consumer. Existing agent-written Engine/U10 contracts are
compatibility constraints, not evidence of personal taste. No new v3 preference
claim is made. Public Engine APIs, fixed Session semantics and console formats
remain unchanged. Magic may read these ownership/meaning lessons without imports.

**ST-025 — Compose and retain shared manual ingredients.** The proposed caller,
shown before dependent APIs, is implemented in `src/continuation.h/.cpp` and
`story_continuation.caller` under projects/apps/story-offline:

```cpp
const auto completed = ferry.capture().value(); // Terminal paired ferry boundary.
const auto content = compose_watch(manual_library(), ferry_graph, completed, 0).value();
auto watch = WatchEncounter::start(content, {"watch-01"}).value();
const auto offered = watch.view().value();      // Real issued live authority.
watch.choose(offered.choices[0].selection).value();
const auto paired = watch.capture().value();   // Reply boundary, world + evidence availability.
watch.restore(paired).value();                  // Fresh incarnation, no reward replay.
watch.choose(watch.view().value().choices[0].selection).value(); // Explicit later question.
```

The literal manual library contains 18 greeting/wording/reply fragments with two
flavours each, three typed consequence fragments and three recipes. Recipes bind
proposition/action/outcome identities, actor Ivo and fragment references to prior
paid/repaired/declined facts. Arrival, suspicion, withholding, later question and
closure ingredients are shared; there are no three unrelated monolithic handlers.
Each ingredient is checked even when not selected. Duplicate identities/prior
recipes, unknown or missing references, wrong fragment roles, unsupported actor/
operation/condition, conflicting learned-knowledge/attitude claims, invalid enum/
origin/revision and incompatible context refuse without a usable partial artifact.
At most three candidate recipes are examined, without regeneration or random search.

A tiny host table permits only verify_record/has_record/fare-confirmed,
hear_testimony/mara_available/repair-confirmed and
explain_shore/still_ashore/shore-explained, with their exact typed knowledge and
absolute attitude targets. Fragment content cannot register callbacks, charge a
fare or grant a reward. Validation checks structured meaning/roles, not arbitrary
prose truth. Continuation's private factory deep-copies Library and Checkpoint into
an actual const object, retains the immutable source Projection and frozen watch
Definition, and exposes const observations. Constructor/copy bypasses are closed;
source library/context mutation/destruction cannot alter accepted content. Normal
allocation exceptions propagate without publishing a partial continuation.
An extensible provider schema or generic composition search was rejected for this
proof. Confidence is high in ownership/authority, medium in this private recipe
shape; replacement is local but retained meaning requires revision discipline.

**ST-026 — Evidence changes information, never crossing facts.** Paid passage
offers fare-record evidence; repair offers Mara's testimony; decline offers an
explanation on the original bank. Each also offers withholding, which has no
effect and preserves the current account. All definitions have five fixed nodes,
four choices and three terminals. Clarification enters the understood reply;
Ask Again, guarded by is-informed, or Finish then explicitly ends the conversation.
Withholding immediately reaches its own terminal. All destinations are chosen
before compilation; callbacks never route, generate, recompose or perform I/O.

| Source | Evidence / Ivo knowledge after clarification | Ivo attitude / belief | Authoritative facts |
| --- | --- | --- | --- |
| paid | fare-record / VerifiedFare | +1 / no unlicensed belief | 5 coins, 3 planks; paid, licensed passage |
| repaired | mara-testimony / HeardTestimony | +1 / no unlicensed belief | 8 coins, 1 plank; repaired, licensed passage |
| declined | explain-stay / SawDecline | 0 / no unlicensed belief | 8 coins, 3 planks; declined, no passage |

The table uses the tide fixture; storm repair retains 8 coins/2 planks. Mara's
prior attitude/knowledge and Lena's unawareness remain unchanged. U11 adds only
VerifiedFare/HeardTestimony to the existing private Knowledge enum; U10's initial
unaware-context contract and all old printed bytes stay intact (ST-023).
Flavour changes text but not choice IDs, keys, targets or state consequences.

WatchEncounter borrows pure handlers synchronously through real Session and U9
choose_with_transition. Host evidence availability may change after presentation;
selection rechecks it before effects. Clarification prepares a complete scalar
WatchState, verifies exact unchanged Facts/Mara/Lena and coherent Ivo information,
then adopts nonthrowingly. The absolute attitude target is not a repeatable
increment/reward. Engine still prepares its owned success before effects and
commits only nonthrowing cursor/result movement afterward. Counters are unsaved
instrumentation. The later issued Ask Again checks learned knowledge; an edited
reply cursor alone cannot enable it. A separately prepared remembered greeting
uses explicit latest world context, without starting another session or changing
the already frozen greeting. Known ashore text explicitly remembers no crossing.
Confidence is high in tested separation, medium in these social rules/pacing;
participant fun/comprehension need actual evidence, not headless pass counts.

**ST-027 — Completed origin, serialized handoff and value resume.** Preparation
checks bounded source identity values, coherent ferry/world/observer context and
an exact source Session::resume, then requires a terminal destination/shore node
consistent with the supplied facts. It never infers resources or knowledge from
node/step, authenticates history or invokes handlers. The retained terminal ferry
cursor is origin context, not live selection authority. A host may supply updated
knowledge alongside that historical completion when explicitly preparing new
content; the demo labels this remembered greeting as prepared, not started.

Watch start copies the accepted world into one serialized reference owner. Its
paired checkpoint owns Snapshot, world and evidence_available. Resume validates
exact definition/revision and coherent world with unchanged source Facts/Mara/Lena,
constructs a fresh idle Session, then adopts session/state nonthrowingly. Failures
preserve the current pair and live token; successful resume revokes old incarnation
tokens. Same-owner and fresh-owner resume at the clarified reply followed by Ask
Again keep original fare/resources unchanged, with watch effect counts one and
zero respectively. Failed context/ingredient preparation preserves both previous
completed ferry pair and existing accepted/live watch values. Idle moves preserve
authority; moved-from operations reject safely. No disk format, durable transaction,
external rollback, authentication or exactly-once crash promise is added. Confidence
is high in exercised value ownership, medium in deliberately strict context policy;
expanding historical/world compatibility requires another bounded contract review.

**ST-028 — Bounded composition and execution.** Lower-only hard ceilings are 24
fragments (two flavour strings each), 3 consequence fragments, 3 recipes/candidates,
5 nodes/4 choices, 1024 bytes per source/generated string, 32768 counted string
bytes, 256 KiB requested payload and 1 Mi work units. Library IDs/refs cap at 64
lowercase ASCII/digit/hyphen/underscore bytes. Existing core snapshot identity bytes
are opaque, nonempty and cap at 128 each. Counts precede loops/copies, lengths
precede scans/concatenation, checked remaining-budget arithmetic precedes growth.
Both flavours and unused ingredients are checked. A combined greeting exceeding
the string cap refuses before concatenation. Above-hard-cap/size_t-maximum requests
are typed InvalidLimits; zero or one-short allowances refuse as appropriate.

Usage charges new library/context objects and vector slots, string bytes/terminators,
projected Draft/OperationKeys payload and generated copy work. It charges query
length-plus-one comparisons, rows/slots, small fixed semantic allowances and a
conservative bounded source-factory lookup reservation. Generated ID size reserves
one extra byte. Pre-existing shared ferry storage remains under U10's separate
caps; Engine factory/compiler/frozen-definition internals, allocator capacity/
rounding/control blocks, stack/diagnostic overhead and RSS are excluded. Work is
declared units, not instruction/time bounds. Compiler inputs are exactly bounded
by the five-node/four-edge graph; ordinary allocation exceptions remain possible.
This keeps the resource contract honest without a generic graph/allocator framework.
Confidence is high in tested boundaries, medium in reversible caps/model.

The existing default-OFF headless app gains only explicit
`--watch tide|storm pay|repair|decline evidence|withhold [0|1 [none|restore|drain]]`.
Default and old explicit U10 modes stay byte-compatible. New mode plays one
completed ferry then one watch encounter, prints manual provenance,
issued choices/replies and actual consequences separately; it never starts an
automatic scene loop. Unknown/missing commands exit 2 before play, live denial
exits 3, completed scenarios 0. Tests use independent literal state/text expectations.
Actual attempts, original assertion failure/correction, retained transcript proof
and remaining visual/leak/platform/durability/fun gaps are recorded in next.md.

## U12 multi-beat relief bargain from shared sets

U12 begins at clean accepted U11 `6f3b9caaaa1a6b71f10fa97f8b7d25e57847b905`.
Its committed receipt and complete 193/194 accumulated plus corrected 1/1 evidence
were inspected; that original failure is preserved, not relabeled green. The
feature-round-20261024-0500 policy/plan selects this unit only. Workspace/clone
instructions, autonomy/memory policy, feature brief, story-generation.md and
private candidate/continuation/owner contracts were read. Current agent contracts
are compatibility evidence, not personal API taste. No new v3 inference is made.

**ST-029 — Shared composition with real agency.** The caller proposed before APIs
is now implemented privately in story-offline/src/bargain.h/.cpp and its caller case:

```cpp
const auto content = compose_bargain(sets, catalogue, context, {"ramp-quay", 0}, limits).value();
auto scene = BargainEncounter::start(content, {"relief-01"}).value();
const auto offered = scene.view().value();
scene.choose(offered.choices[0].selection).value(); // Ask Terms; real issued authority.
const auto paired = scene.capture().value();
scene.restore(paired).value();                    // New incarnation, no repeated information.
scene.choose(scene.view().value().choices[1].selection).value(); // Negotiate.
```

Manual sets own 18 two-flavour text ingredients, seven typed intents and four
location/request bindings. The host catalogue owns two actors, two cost rules
and supported operation/condition names. Ramp/shelter reuse wording, information,
responses and consequence semantics; they differ in actual costs and obligations.
Four bindings mean two rules on two banks, not four independent story handlers.
There is one owner/graph template, no provider schema/parser or universal planner.
BargainPick is an explicit owned request ID/flavour, never live Selection authority.

| Node | Actual issued alternatives / fixed targets |
| --- | --- |
| request | Ask Terms -> terms; Help Now -> helped; Pay -> paid; Promise -> promised; Negotiate -> proposal (disabled until terms); Refuse -> refused |
| terms | Help Now -> helped; Negotiate -> proposal; Refuse -> refused |
| proposal | Commit -> promised; Help Now -> helped; Refuse -> refused |
| helped / paid / promised / refused | One Close -> closing, after showing a distinct NPC response |
| closing | Terminal, retaining distinct resolution/resources/obligation |

The eight authored nodes and sixteen forward edges are independently checked as
a DAG, all entry-reachable with a terminal route. Request/terms/proposal contain
substantive alternatives, not padded Continue chains; each chosen outcome has one
closing step. Ask Terms changes information and enables negotiation. Negotiation
itself is a no-effect move to another decision, not an automatic commitment.
Every target is frozen before effects; callbacks neither route nor generate.
Confidence is high in exercised agency, medium in pacing and private shape. A
general planner would add unselected search/meaning policy; reversal is local
with identity/revision and participant review. Headless checks do not prove fun.

**ST-030 — Typed costs, pending obligations and observed reactions.** The only
application operations are learn_terms, give_material, pay_relief, record_promise,
record_negotiated and decline_relief. Supported conditions recheck Open state,
resources or learned terms synchronously. Intents' IDs/keys/claimed resolutions
must match these host meanings; data cannot introduce operations/rewards. The
catalogue supplies positive bounded costs (1..10), ordinary due 2..10 and a
positive strictly smaller negotiated due. Prose may lie; it cannot deliver its claim.

| Rule, after paid tide ferry/watch | Immediate | Payment | Ordinary promise | Negotiated promise after information |
| --- | --- | --- | --- | --- |
| ramp, initially 5 coins/3 planks | 1 plank, 5/2 left | 2 coins, 3/3 left | 2 planks due; 5/3 retained | 1 plank due; 5/3 retained |
| shelter, initially 5 coins/3 planks | 2 planks, 5/1 left | 3 coins, 2/3 left | 3 planks due; 5/3 retained | 2 planks due; 5/3 retained |

Refusal preserves supplies with zero due. The requester witnesses help/payment,
hears a promise/refusal, or discusses terms; this knowledge is distinct from
resolution/factual costs and from attitude (+3 help, +1 payment, 0 promise, -1
refusal). Promises record a positive pending quantity only, with no fulfillment
operation/field/reward. U14 owns any later payoff. The original crossing/licensing,
Mara/Ivo/Lena knowledge/beliefs/attitudes remain unchanged. The new requester
learns only this conversation, not private ferry/watch history. Nera is at the
quay; Oren is on the original bank. Declined passage requires the bank binding,
with no teleport or invented crossing. Flavours change text, not mechanical branches.

One serialized BargainEncounter retains the artifact and uses actual Session
view/choose_with_transition. Availability is pure and rechecked after presentation.
It stages/validates the complete scalar ReliefState, then assigns nonthrowingly;
Engine retains preparation-before-effect and allocation-free cursor/result commit.
The effect counter is unsaved instrumentation. Host resource changes are allowed
only while Open, within bounds; old enabled rows can reject without spending or
progress. Confidence is high in tested sequencing, medium in costs/reactions.
There is no global reputation, durable effect delivery, random callback outcome,
observer omniscience or new application flag hidden in Engine.

**ST-031 — Retained accepted meaning and paired resume.** BargainArtifact has a
private constructor, deleted copies and is allocated as an actual const object.
It deep-copies sets/catalogue/pick/context and retains the immutable watch source
and compiled Definition; no mutable producer alias survives. Definition identity
includes sets/catalogue/request IDs, catalogue revision, source definition and
flavour; definition revision is the set revision. Revision discipline is the
host's responsibility, not a content hash/authenticated history. Both flavours,
unused requests and all structured ingredients are eagerly validated. Unknown
actors/rules/keys, duplicates, conflicting roles/consequences/costs, missing intents
and impossible location/source context return owned failures; compile diagnostics
remain owned if compilation rejects. No partial artifact is published.

Preparation reuses WatchEncounter start/restore to validate the supplied source
pair without effects, then requires a terminal source node. The input world and
live owners remain unchanged on preparation/refusal. A bargain checkpoint owns
Snapshot and ReliefState (resources, terms, resolution, due and requester state).
Restore checks coherent application values and exact Engine identity/revision/node,
constructs fresh state, then adopts idle session/state nonthrowingly. Failure
preserves previous pair/token; success renews incarnation. Same/fresh-owner resume
after information or cost/obligation boundaries neither recharges resources nor
replays ferry/watch effects. An edited cursor alone conveys no terms knowledge;
a promised state with zero due is rejected. Bounds/coherence do not authenticate
edited history, rollback or arbitrary external transactions. Existing Snapshot,
watch/ferry types and all console formats stay unchanged; this is a new private
in-memory pair, not a new save schema. Confidence is high in tested ownership,
medium in strict origin policy; expanding compatibility requires bounded review.

**ST-032 — Explicit finite selection and honest allowances.** Hard lower-only
ceilings: 40 two-flavour texts, 7 intents, 8 requests/candidates, 2 actors, 4 rules,
8 names per supported domain, 8 nodes/16 choices, 1024 bytes per source/generated
string, 32768 counted string bytes, 256 KiB requested payload and 1 Mi work units.
IDs cap at 64 lowercase ASCII/digit/hyphen/underscore bytes; source snapshot IDs
are opaque, nonempty and cap at 512 bytes each. Counts precede traversal/copy;
lengths precede scans/concatenation; remaining-budget arithmetic precedes growth.
At most eight request candidates are examined; there is no random regeneration.

Usage charges new owned sets/catalogue/context objects/slots, string bytes with
terminators, Draft/OperationKeys payload, generated fields, rows, comparison byte
reservations, scalar checks and copy work. Subsequent graph-reference lookup work
is charged before each lookup/copy. Conservative reserves include unused reply
copy allowance and bounded source-factory lookup work. It is requested payload,
not peak memory: previous shared watch/ferry storage, Engine factory/compiler/
frozen-definition internals, allocator capacity/rounding/control blocks, temporary
copy/concatenation overhead, stack/diagnostics and RSS are excluded. All underlying
inputs/graph sizes remain independently hard-capped. Normal allocation/programming
exceptions propagate without mutating caller-owned live values or publishing a
partial artifact. Work units are a model, not instructions/time. Confidence is
high in tested refusals, medium in caps/model; extension needs resource review.

The existing default-OFF executable gains only explicit
`--bargain ramp|shelter pay|repair|decline immediate|pay|promise|negotiate|refuse
[0|1 [none|restore|drain]]`. It plays one ferry, one watch and one finite bargain
through real issued choices, prints actual terms/replies/state separately from
manual prose, and optionally restores a completed boundary. Default and older
modes remain byte-identical. Invalid usage exits 2 before play, live refusal 3,
normal completion 0. There are no callbacks doing I/O, automatic scene loops,
provider calls, seeded selection, extra parsers, public Engine changes or sibling
imports. Exact execution and the finite-controller handoff are in next.md.

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
| U3: console acceptance | Accepted private headless gate, numbered selections, complete paired JSON envelope and exclusive fresh-path save/resume, with 26 new plus 34 retained cases passing. Full evidence/limits are in next.md. |
| U4: authored startup | Private bounded definition_schema 1 input compiles before Encounter construction with fixed application keys. 17 definition cases and six new transcript cases passed with all 60 regressions (83/83); all 32 child statuses match expected normal/usage/rejection outcomes. |
| U5: advisory inspection | Accepted private prototype with owned report and bounded iterative analysis; 10 report and five new transcript cases passed with all 83 regressions (98/98). All 54 child outcomes match, including 22 inspection invocations. next.md records full evidence/limits. |
| U6: reusable Engine inspection | Public CPU header and Engine-owned exported operation, typed per-call budgets, thin legacy CLI adapter and independent actual-Engine consumer. next.md records executed cases/link proof and limits. |

U1V completed U1 verification; U2 completed CPU sessions/values; U3 completed the
console milestone. U4 completes authored startup; U5 adds private inspection; U6
exposes its bounded CPU analysis through Engine. Later scope requires coordinator selection.
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

U4's [definition cases](../projects/apps/dialogue-console/tests/definition-codec.cpp)
and added [executable transcripts](../projects/apps/dialogue-console/tests/transcripts.cpp)
now provide native evidence for ST-011/012: all 17 definition and six new transcript
cases passed with all 60 accepted regressions (83/83, no skips or sanitizer
diagnostics). The 32 actual children returned expected exits: 18 normal, six usage,
eight rejected-startup outcomes. Invalid startup has no encounter output, effects
or command processing. Loaded gate order/routes and zero-replay paired resume,
crossroads data, ownership after source destruction, exact retained diagnostic
vectors, strict structural rejection and actual file bounds/byte preservation
were exercised. The lower-only guard seam and stronger document limit remain
distinguished above. Public headers/Engine/Runtime and save-codec/file contracts
were preserved. Exact commands, full logs, counts and portability/leak/integration
limits are in [next.md](next.md). This accepts only assigned U4; stop for coordinator review.

U5's [owned report and analysis](../projects/apps/dialogue-console/src/definition-report.cpp),
[header-first graph cases](../projects/apps/dialogue-console/tests/definition-report.cpp)
and executable inspection transcripts now have assigned native evidence: **98/98**
passed, including ten report cases, five new transcript cases and all 83 retained
regressions. All 54 child outcomes match expected exits; 22 exercise inspection's
valid/advisory, usage and rejected-input paths. Literal output, ignored stdin
commands, no requested save/sidecars, unchanged rejected source bytes, complete
original diagnostics, retained report lifetime, nonfirst entry and exact graph/ID
bounds are exercised. No ASan/UBSan diagnostic, skipped/unrun case or resource
refusal occurred. Leak coverage and platform/SDK/runtime integration remain
unverified. These observations complete assigned native U5 checks, ready for
coordinator review; no later context/operation or unit is implemented automatically.

U6 now has executed native boundary evidence: 11 Engine inspection cases and three
independent public consumer cases passed alongside all 98 accepted regressions
(**112/112**, no skips), followed by **2/2** focused rechecks after adding late
storage-refusal and exact hard-budget assertions. All 54 complete retained child
blocks match U5 byte for byte, not only their expected statuses. The real shared
Engine exports inspect, the separate consumer imports it and needs
libcheryl-engine.so.4; its live dladdr owner check passed. Native logs show no
ASan/UBSan diagnostics. The supplementary metadata matcher initially failed on
CMake regex escaping after all three native tools exited 0; its corrected
read-only matcher passes, with both attempts preserved. No implementation defect
or sanitizer flag adjustment was needed. next.md records exact commands/statuses,
full logs and remaining allocator/platform/leak/SDK/runtime limits. This completes
assigned U6 checks and stops for coordinator review, without integration or
publication transport.


U7 has executed acceptance for this finite milestone: all **121/121** focused
checks passed (the 112 retained names, six new Engine lookup cases, two public
consumer cases and one isolated allocation probe), no skips/unrun cases or
ASan/UBSan diagnostics. All 54 complete console transcripts match U6 byte for byte.
The probe verifies ordinary/array/aligned and shared-Engine rejection controls,
then performs 2300 first-use/repeated lookups without an additional allocation
attempt. Exact scoped identity, binary/long names, authored pointers/order,
retained owners and the eleven-diagnostic literal are exercised. Normal source
comparison also confirms the validation/reporting body is unchanged. next.md owns
complete logs/commands/counts and portability/leak/SDK/runtime limits. The committed
U7 ready/true receipt is execution evidence; queue selection/source review remain
separate, and this worker stops without starting U8.


U8 has executed its assigned focused acceptance: **138/138** checks passed (all
121 retained names/order, 14 Engine route cases and three actual shared-Engine
consumer cases). Literal shortest/tie/conditional/cycle routes and 512 independent
BFS-reference endpoint queries agree. Owned output survives definition/draft release;
exact/one-short/zero/above-ceiling/size_t-maximum budgets and typed endpoint/no-route
refusals pass. The public header is first in the actual-Engine consumer, with
executed dladdr/equivalent export ownership. All 54 complete console transcripts
match U7 byte for byte; no skips/unrun cases or ASan/UBSan diagnostics occurred.
next.md preserves complete logs/statuses, the static premature-audit correction,
limits and gaps. A ready U8 receipt reports this execution; source review and U9
selection remain the queue controller/coordinator's next action after run drain.


U9 has executed focused acceptance: **154/154** cases passed, with all **138 U8
names/order retained**, 12 new Engine transition cases, three actual public Engine
consumers and one isolated allocation probe. The probe exercises five preparatory
allocation failures, preserving healthy snapshots and original tokens with no
effects, then denies allocation inside an effect through receipt return and result
movement. Legacy choose/early rejects run under denial from entry without unused
receipt cost. Shared Engine hook controls and stable borrowed effect arguments
are verified. Native exports include the new member and legacy choose; private
templates stay hidden. All 54 complete child transcripts are byte-identical to U8,
with no skips/unrun cases or ASan/UBSan diagnostics. Exact commands/statuses, full
logs, corrected static link-rule audit and remaining leak/platform/SDK/runtime
limits are in next.md. The ready U9 receipt reports execution only. This completes
the third assigned milestone of the finite batch and stops for coordinator source
review; no new unit, dispatch/persistence framework or publication is selected.

U10 now has executed acceptance: **176/176** focused checks passed, retaining all
**154 U9 names/order** and all **54 byte-identical prior child transcripts**.
Eighteen private candidate/projector/reference-owner cases and four actual
executable transcript cases add nine child outcomes (4 exit 0, 4 exit 2, 1 exit 3);
the default child completes eight finite scenarios. Literal state/output checks,
owned lifetimes, semantic/refusal/budget boundaries, no-effect preparation,
changed live eligibility and same/fresh-owner paired restore all pass. Final review
default-initialized the host method enum and added an invalid-method assertion;
a targeted rebuild and **22/22 private recheck** pass, with all nine child blocks
unchanged. The accumulated regression campaign ran once. The read-only link audit initially
missed CMake's second explicit Ninja output; its corrected matcher and diagnostic
are saved, without weakening or rerunning native checks. No skips/unrun cases or
ASan/UBSan diagnostics occurred. Leak checking, other platforms, durability,
Unicode/prose truth, actual model output and human fun remain unverified. next.md
owns exact commands, full logs/statuses, receipt and coordinator review action.
This completes only the private U10 proof; it selects no provider, public story
service, integration, spell system or automatic next unit.

U11 has passing execution for **194 unique focused cases across attempts**: the
single accumulated run passed 193/194, including all 176 regressions; one new
stale-before-terminal expectation was corrected to the existing Engine ordering,
then its exact 1/1 recheck passed. Original failure/exit 8 is preserved alongside
successful rebuild/recheck, with no Engine fix or broad rerun. All 63 prior child
blocks remain byte-identical and 12 new children match expected exits (6 normal,
5 usage, 1 live denial). There are no skipped/unrun cases or ASan/UBSan diagnostics.
Meaningful state/prose, context ownership, no-effect preparation, paired restore,
authority and resource boundaries have native evidence in next.md. The receipt
reports worker execution; source review and participant/platform/leak/durability
acceptance remain distinct. Stop after U11's coherent local checkpoint, idle for
coordinator review rather than selecting another scene or integration.
