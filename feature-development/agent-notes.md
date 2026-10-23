> **Parked by user direction, 2026-10-23.** See [parked.md](parked.md).
> U4 is an unverified interrupted draft; earlier U3 checks do not accept it.

# Settings milestone: design and evidence

The original design was prepared against source commit
`3542f883391d4225329dab8d5d61a5987e69c8f0` and saved in checkpoint
`157c75736bf5d325c8ea5174430b5dbbd75a2ae4` on `feat/settings`. Accepted U1
checkpoint `e4529aa9229334433a135b51598ebb50c11b8f4f` implements the app-private
value, validator, and file boundary in SET-002/006/007. Accepted U2 checkpoint
`2d9bdd873ab6ab23ab032465f76d1b2ed9cd8f0d` adds snapshot queries/metadata in
SET-003/005. Accepted U3 checkpoint `afc3d17b164650e754379f72e59bac9abbf0ec72`
implements the app-private `ScoreInput` owner/candidate protocol. U4 composes
CPU Apply in SET-005/006/007. Game integration, editor, modal gate and runtime
scheduling remain proposed. [Decisions](decisions.md) distinguish those boundaries;
[next.md](next.md) records actual commands, results, limits, and the next unit.

## U1 caller and implemented boundary

Include the app-private `src/settings.h`; no JSON type enters that header:

```cpp
Tutorial::SettingsFile file("tutorial-settings.json"); // resolves once against cwd
auto loaded = file.load_or_defaults();                // value, status, issue
auto candidate = loaded.value;
candidate.camera_pan_speed = 200;
Tutorial::validate_settings(candidate);               // invalid_argument; no mutation
file.save(candidate);                                // synchronous file commit only
```

`Missing` and `Recovered` both supply the entire default value; `Recovered` also
supplies a path and field/operation diagnostic. `Loaded` supplies the complete
valid snapshot. Saving never adopts application state or changes input mappings.
U1 registers settings source in both app and test cases, with private JSON includes.
The accepted U1 checkpoint passed 21 settings cases and five gameplay cases in the
Linux Release runner; current regression evidence is in [next.md](next.md).
Graphical and full-feature behavior remain outside that evidence.

## U2 queries and future caller obligation

The public additions are read-only `Snapshot::has_context(ContextId) const noexcept`
and `device_enabled(DeviceId) const noexcept`. They do not schedule, attach, retain,
or validate a lease. A future editor may use them as follows; this is an application
sketch, not an implemented Apply/rearming workflow:

```cpp
struct PendingCandidate {
    CE::Input::Actions::Context lease;
    std::uint64_t generation;
};
PendingCandidate candidate{endpoint.attach(definition), editor_generation};
// Retain candidate across updates on its owner; do not retain just its identity.
const auto newest = tick.latest_poll();
const bool candidate_observed =
    candidate.lease && candidate.generation == editor_generation &&
    !tick.polls().empty() && newest->has_context(candidate.lease.id());
const bool new_admitted_poll =
    !tick.polls().empty() && newest->poll() > release_watermark &&
    newest->device_enabled(keyboard);
```

`candidate_observed` is only publication evidence for a still-valid retained
candidate of the current editor generation. Reset, clear/shutdown invalidation,
or a stale generation blocks adoption even if a historical sample contains the
identity. Future Apply still validates/persists before adopting. `new_admitted_poll`
is a necessary rearming predicate, not sufficient neutrality: SET-003 still
requires the active context and neutral gameplay controls/edges for the consumed
batch, tagged to the same editor cycle. `latest_poll()` on an empty batch returns
its baseline; it never proves a newly consumed poll beyond the release watermark.

The planned consumer is the existing `tutorial-game` with its existing TGUI
session. It will expose exactly two game settings and one rebindable gameplay action:

| Value | Default | Valid values and visible effect |
| --- | --- | --- |
| `camera_pan_speed` | `100.0f` | Finite number in `[25, 400]`, world units/second; A/D pans at this speed. |
| `show_health` | `true` | Boolean; shows/hides the existing health bar without changing health. |
| `add_score` binding | physical `Space` | One permitted unmodified keyboard key; a new press adds one point to the existing score. |

The planned editor opens through a native TGUI button. The panel has speed,
health visibility, Rebind, Defaults, Apply, Cancel, and a status message. Defaults
changes only the draft; Escape cancels the panel, or just the current rebind operation when
listening. Closing the panel/window discards uncommitted edits. Engine options
requiring restart, asynchronous option setters, additional settings, gamepad/mouse
rebinding, and a general settings registry remain later work.

## U3 private score-input caller and protocol

`src/score-input.h/.cpp` implements the value/lease protocol; it does not replace
Game's direct Space mapping. Construct from the retained endpoint and cached
keyboard ID. All helper access, token moves/destruction, and generation changes
belong to one serialized caller. No helper borrows mutable `Bindings`, polls
devices, blocks on a future, or saves files. A small caller is:

```cpp
using Key = CE::Input::KeyboardKey;
Tutorial::ScoreInput score(contexts, keyboard, Key::Space);
score.observe(tick);                         // initial installation from tick.polls()
const auto score_button = score.read(tick);  // only the active namespace
score.set_generation(editor_generation);    // call on every generation change
auto candidate = score.stage(Key::B);        // nullopt explicitly means unchanged
if (cancel_requested) {
    score.cancel();                         // releases only the pending lease
    candidate.reset();
} else if (candidate &&
           score.observe(*candidate, tick) == Tutorial::ScoreInput::Status::Ready) {
    auto prepared = score.prepare(std::move(*candidate)); // fallible checks; active unchanged
    const bool adopted = score.adopt(std::move(prepared)); // noexcept, no allocation
}
```

The client consumes `score_button`/`adopted`; this is a private-protocol caller,
not a Game/UI integration example. Readiness is not gameplay admission. The held
seeding case deliberately shows a candidate press readable immediately after
adoption; SET-003's future gate/rearm policy must suppress that editing-era input.
Its neutral boundary still needs genuinely new consumed polls beyond the release
watermark, even when candidate readiness has latched. Detailed choices and costs
are in SET-005; none is new evidence of personal v3 API taste.

## U4 private CPU Apply caller

`SettingsApply` borrows an active `GameSettings`, `ScoreInput` and `SettingsFile`
that outlive it. It owns its editable draft and optional candidate. Access is
serialized with the score owner; while the helper lives, it exclusively controls
score generation/pending requests. Ordinary score observation/reading still belongs
to that same caller. Keep input clear/shutdown, owner moves, edits and foreign or
reentrant callbacks outside the entire synchronous `commit` call. No helper polls,
waits, registers handlers, renders, or changes gameplay admission.

```cpp
using Key = CE::Input::KeyboardKey;
Tutorial::SettingsFile file("tutorial-settings.json");
auto active = file.load_or_defaults().value;
Tutorial::ScoreInput score(contexts, keyboard, active.add_score);
Tutorial::SettingsApply apply(active, score, file); // no load/save/attachment here
score.observe(tick);                            // initial active installation
const auto score_button = score.read(tick);      // active namespace only
apply.edit({200, false, Key::B});                 // owned draft; advances generation
auto request = apply.request();                 // validate, stage changed key, retain lease
// On subsequent owner updates; no separate polling loop:
const auto observed = apply.observe(request, tick); // only tick.polls() adds evidence
const auto outcome = cancel_requested
    ? apply.cancel(request)                     // no write; draft retained
    : apply.commit(request);                    // Pending or prepare -> save -> adopt
// Only after outcome == Applied; presentation/diagnostics are separate:
const auto saved = file.load_or_defaults();       // Loaded, value == active
```

Consume the statuses and score button in the eventual application. A failed save
throws `SettingsFileError` with the existing operation/path diagnostic; the caller
may show it later, then explicitly call `request()` to retry the retained draft.
Allocation/programming exceptions retain their original types. An unchanged key
creates no candidate; it still waits for witnessed active installation and saves
the other values without changing context identity. Tests are CPU consumers of
this exact composition. Game remains unwired. SET-003 still gates the eventual
gameplay path; this caller does not establish input isolation.

## Proposed caller sequence

This is an API sketch for app-private code, not an available Engine API or a
compiled example. `SettingsFile`, `ScoreInput` and `SettingsApply` are implemented helpers;
`SettingsEditor` and Game integration remain proposed. U2 supplies the snapshot queries
for context membership and the published device gate used by the future caller.

```cpp
// main, before runtime.run(): choose the data path once, independently of assets.
SettingsFile file(std::filesystem::absolute("tutorial-settings.json"));
auto loaded = file.load_or_defaults(); // owned value plus missing/loaded/recovered status
Game game(engine, assets, loaded.value, std::move(file), loaded.issue);

// Game::init_platform(), on the platform owner:
auto& input = engine.input();
keyboard = input.keyboard_id();
contexts = input.action_contexts();   // retain endpoint; never borrow Bindings in update
score_input.emplace(contexts, keyboard, active.add_score); // future optional<ScoreInput> Game member
// Keep the existing non-score mappings and controller contexts.

// Game::init_simulation(), on the simulation/UI owner:
ui.init_simulation();                // reuse its single TGUI Session
editor.connect(ui, active, file, contexts, keyboard);

// Game::update(tick), on the simulation/UI owner:
ui.route_input(tick.input.records()); // callbacks enqueue editor intents
editor.finish_batch(tick.input);      // open/edit/cancel/stage/observe/save
score_input->observe(tick.input);     // active installation from consumed polls
if (!editor.suppress_gameplay_this_tick()) {
    pan_camera(active.camera_pan_speed, tick.delta_seconds, tick.input);
    if (score_input->read(tick.input).pressed()) add_one_point();
    update_other_gameplay(tick);
}
ui.present(active.show_health);       // time, HUD, scene adoption/publication continue
```

The example omits existing rendering/setup boilerplate. `finish_batch` advances
pending application across updates; it must never wait for a poll on either
owner. The active model remains readable throughout. There are no immediate
gameplay side effects in toolkit callbacks. Reorder today's UI-last game update
so the modal decision precedes quit, scoring, camera, join/leave, and movement.

## SET-001 — Keep the consumer small and use TGUI

Use `projects/apps/tutorial-game`, rather than introducing a new application or
adding settings to the multi-toolkit demo. It already has all three observable
effects: camera panning, a health bar, and a Space-to-score command [E07]. Add
the editor to the existing `UIController` session and keep native widget APIs.
TGUI permits one process-global session; a second session is inappropriate [E04].

Alternatives: RmlUi has an existing integration too, but would add a document and
another toolkit lifetime to this consumer. The demo has interactive widgets but
more unrelated controls and coexistence behavior. Neither choice is evidence of
the user's preferred toolkit. Confidence is high in this bounded consumer choice;
native usability remains unverified.

## SET-002 — Own typed active and draft values in the application

U1's `GameSettings` is an ordinary owned value containing the two settings and the
portable key enum. `validate_settings` rejects invalid speed/key values without
changing its argument. Callers can construct invalid typed candidates, but save
validates again before any file effects. This API is private to the application.
The remaining ownership below belongs to the later consumer:
`Game` owns the active value, file adapter, score context, and
editor. The editor owns an optional draft seeded from active values, with separate
speed-editing text so invalid intermediate text never enters `GameSettings`.
Validation converts the complete draft into a valid candidate without modifying
active values. A second editor
cannot open while one is active or applying. There is one settings writer.

Consumers read the active value on the simulation owner. Rendering continues
through the existing prepared frame/scene path, without retaining a draft or
reading mutable UI state from the platform owner. The platform caches the
keyboard identity and context endpoint at initialization; their values can be
used from simulation. Keep live adapter/window lifetime outside those handles.

Alternatives: a global service or string-to-variant registry would give early
generic access but introduce global identity, conversion, subscription, and
rollback policy before a second consumer exists. Binding widget setters directly
to live values would make Cancel unreliable. Confidence is high for the scope;
typed caller-supplied setup has medium historical preference support [H1], while
application ownership itself is an engineering choice, not a confirmed taste.
Reversal cost is currently small because no runtime or UI caller depends on the
new helpers; no general Engine registry has been introduced.

## SET-003 — Runtime polls; a modal gate owns gameplay admission

Runtime remains the sole poll owner. `System::poll`, mutable `Bindings`, and device
queries stay on the platform owner. Simulation receives immutable `Tick` values,
ordered records, and context-local action views [E01–E03]. No UI backend pumps
events, and no settings helper polls native input or destructively drains a queue.
Toolkit construction, input, time, recording, and ordinary destruction stay on
its creating simulation/UI owner [E04].

Opening requests Exclusive keyboard focus through the existing session. Its
Events lease already supplies physical records; session focus adds Text capture.
Pointer delivery is explicitly selected for the Settings button and modal panel,
using observation-time positions, and is withheld from gameplay while modal.
Capture alone is not consumption, and Exclusive focus only gates keyboard State.
Therefore the application suppresses **all gameplay command dispatch** during the
opening batch, editing, binding capture, apply, closing batch, and rearming.
Freeze gameplay updates while modal; continue runtime input, UI, and rendering.

Callbacks enqueue intents, not file writes or focus release in the middle of a
record batch. Drain the entire batch under the session's entry epoch, then enact
open/close/focus transitions. If the panel opens anywhere in a batch, suppress
that whole tick, including earlier collected gameplay edges. If another owner
preempts focus, drain already routed records, stop editing/listening, retain the
draft and gate, and show a Resume/Cancel state; reacquisition needs a deliberate
Resume click. Do not continually steal focus back.

On close, retain the gate after releasing focus and queue a read-only platform
callback through the existing retained submission endpoint. The callback captures
no simulation objects and returns `engine.input().action_snapshot()->poll()`.
It runs after any current native pump completes; its poll ID is the watermark
excluding every poll begun before release. Check the future's readiness in later
updates before retrieving its value; never block either owner. Rejected submission,
callback failure, or shutdown `broken_promise` keeps gameplay gated and reports
failure rather than guessing completion [E08].
Tag the pending result with the editor cycle; a late result from an earlier cycle
cannot rearm a reopened editor. Disable opening during Rearming and submit once
per release transition, without a retry loop.

Require a consumed poll strictly newer than that watermark with the **published
keyboard device gate enabled**, the active score context present, and every
fixed/contextual gameplay button inactive with no press in the consumed batch.
Suppress that neutral tick too; resume on a later update. Check quit, pan,
join/leave, both players' movement controls, and score. This prevents held editing
keys or focus re-enabling from becoming a game press. An empty tick, a neutral
sample queued under Exclusive focus, or a pre-open sample from a quick open/close
cannot satisfy this boundary. Starting gameplay after initial context installation
requires a real gate-enabled neutral poll too; there is no prior modal release
to fence at startup.

U2 implements `bool Snapshot::device_enabled(DeviceId) const noexcept`, preserving
an immutable copy of `Bindings::disabled_devices_` at semantic publication. A device absent
from that set is enabled, matching the mapper's existing default policy; this is
admission state, not evidence that hardware exists. Use the cached live keyboard
ID and a real newly consumed poll. `make_snapshot` copies the publishing mapper's
gate set into each root/context candidate before any publication commit. A copy
can allocate; failure leaves the preceding complete publication untouched. Retained
samples never change when live policy changes. Metadata-only polls can have
`has_changes() == false`; the existing backlog/accumulator still transfers their
handles and policy in order. Do not filter progress by `has_changes()`. Checking
current focus or a larger tick-local poll ID alone is
insufficient: concurrent runtime may already have queued newer polls before
release. The watermark also covers open/close before Exclusive focus ever
latches. Test both backlog cases before integration.

Missing required capture/session support fails initialization visibly. A failed
focus request while open retains the draft and modal gate, reports the failure,
and leaves pointer Cancel available; it must not silently enable gameplay.

Alternatives: focus-only isolation leaks already collected input and permits a
held key to press again when the gate reopens [E02]. Disabling the input system
would also starve the editor. Confidence is high in the need for the explicit
gate; combined-batch and editor scheduling checks are still required. The copied
policy is an engineering choice, not inferred user taste. It costs one set copy
per mapper sample; shared mutable policy would invalidate historical observations.
No poll owner, mapper mutation rule, focus behavior, or scheduling policy changes.

## SET-004 — Capture a portable physical key in a separate focus epoch

For this consumer, permit `Space`, letters A–Z, and digits 0–9, excluding A, D,
J, and L because those already control camera/players. Store canonical tokens
`space`, `a`–`z`, `0`–`9` with the same exclusions; this is a deliberately narrow
whitelist, not the full `KeyboardKey` enum. Escape, Enter, Tab, arrows, F2/F3,
modifier keys, Unknown, and other devices are unsupported. Reject a candidate
with Shift/Control/Alt/Super held; CapsLock/NumLock do not change physical identity.

After draining the Rebind-click batch, release the session's keyboard focus and
acquire an app-owned Exclusive routing lease with a distinct nonzero target
(tutorial UI target 2, rebind target 3). Keep the session and its Events lease
alive. Examine only Keyboard `ButtonPhase::Press` records for the cached keyboard
device, rebind target, and exact lease epoch. Never translate Text, native codes,
scancodes, repeat, or release into a binding. Older target/epoch records cannot
select a key. Session handling still sees the full stream for modifiers/pointer
selection, but will not deliver the rebind target's keyboard events to widgets.

Session release leaves its remembered focus epoch available for draining already
collected records; it does not invalidate all queued old widget events [E04].
While Listening, disable non-Cancel widgets and reject their draft-edit, Defaults,
and Apply intents. Keep field presentation frozen and refresh it from the draft
when leaving Listening. This preserves the full modifier stream without allowing
queued old-session keyboard/Text to alter settings or apply them during capture.

Accept at most the first valid press, update only the draft, and drain the rest
of that batch before returning focus to the session. Unsupported keys show a
reason and leave listening active. Escape exits listening with the prior draft
binding; panel Cancel discards the whole draft. Pointer Apply is disabled while
listening; the Cancel button remains usable. The selected key is always behind
the modal/rearm gate. If focus is lost, do not accept later keys until resumed.

Alternatives: listening to all records risks capturing the opening gesture or
another recipient's input; listening to text would bind layout/IME results rather
than controls. A separate target adds a short lease transition but avoids filtering
away toolkit input or guessing the session's private focus epoch. Confidence is
high in identity/routing rules, medium in this first keyboard subset as product
scope. No v3 preference supports this particular scheme [H3].

## SET-005 — Stage a replacement and observe publication before adopting it

U3 implements `ScoreInput` only as an app-private protocol. Game still uses its
current direct Space binding. Future integration will replace that mapping with
the helper's dedicated context while preserving direct/non-score/player mappings.
A single `.cpp`-private semantic action maps one physical key in that namespace;
no runtime context/action/device ID is persisted. Constructor and stage reuse the
U1 validator with an otherwise default `GameSettings`, so there is no second key
whitelist, JSON API exposure, or settings behavior change.

The helper retains `ContextSubmission` and the cached keyboard identity, owns its
active context/key and installation evidence, and keeps one weak pending slot.
A move-only `Candidate` owns a separate record containing the replacement lease,
key, generation and evidence. `prepare` transfers that record to a move-only
`Prepared` token. The slot itself does not keep the lease alive: dropping either
token releases it. Owner destruction or move assignment cancels its old pending
record even when a token outlives the owner. There is no dangling owner pointer.

A second request, including an unchanged-key request, rejects while a token is
pending (`logic_error`). Otherwise an unchanged key explicitly returns `nullopt`
without a new lease or replacement. Invalid keys throw the shared validator's
`invalid_argument`; moved-from/invalid active owners reject locally. Attachment
and allocation failures occur before changing active ownership or assigning the
pending slot. A closed endpoint invalidates existing leases; constructor attachment
then reports the Engine failure, while a surviving helper rejects stage as invalid.
These outcomes preserve its stored active identity/key, without claiming liveness.

Active observation latches installation only from a namespace in `tick.polls()`.
Candidate observation additionally requires this exact pending record, a live
active lease, its live candidate lease and the current generation. Evidence belongs
only to that record/generation and may survive an empty later tick; an empty tick
cannot manufacture evidence from `latest_poll()`'s baseline. Do not use action
activity, `has_changes()`, frame counts, sleeps, futures or current focus as proof.
Empty/inactive installed namespaces count. Zero, pending/not-yet-published,
cancelled-before-installation and subsequently retired identities are absent.
Departing membership includes the final release/zero sample and remains historical:
clear/shutdown invalidation wins over retained membership or prior readiness.

The owner generation starts at zero. `set_generation` accepts any uint64 value;
a change permanently marks the pending record Stale and releases its lease. Calling
it with the same value preserves evidence. Reusing an earlier number cannot revive
an old token. The application must call it whenever its editor generation changes.
`cancel()` marks only the current candidate/prepared record Cancelled and releases
that lease; active ownership/key/evidence remain unchanged. Nonterminal tokens associated
with another owner's weak slot report Foreign and cannot be prepared/adopted there.
Empty tokens and owned engine-invalidated tokens report Invalidated; Waiting and Ready distinguish
accepted requests from witnessed publication. Terminal Stale/Cancelled outcomes
remain terminal even though their old slot is no longer pending.

`read(tick)` summarizes only the active namespace after active installation has
been observed and while its lease is valid. Owned Tick summaries may allocate.
Staging, observing and preparing a candidate never change this namespace, key or
active lease. They never combine old/candidate presses. Owner moves transfer active
lease/evidence, generation and the exact pending association; sources read inactive
and cannot cancel/adopt the transferred candidate. Candidate/Prepared moves transfer
one record; overwriting a token releases its preceding lease and slot naturally.

`prepare(Candidate&&)` revalidates live/current/owned/witnessed readiness and throws
`logic_error` without consuming invalid or non-ready tokens. Success consumes only
the candidate token; the prepared token still reserves the pending slot. No active
state changes. `adopt(Prepared&&) noexcept` rechecks those conditions: rejection
returns false without consuming the token or changing active ownership, including
wrong-owner attempts. Success moves the lease, assigns key/evidence, and clears
the weak slot. It allocates nothing and formats no status text. Context move
assignment retires the old lease; Engine publishes its final release/zero namespace
before retirement, while historical ticks remain usable.

U4's private `SettingsApply` composes this protocol with [SET-007]. Its generation
starts at the score owner's current generation. Every edit and new request advances
it (checked uint64 exhaustion throws before mutation); retry cannot reuse an old
request identity. Requests belong only to this helper's lifetime. One candidate is
retained until preparation, cancellation, edit, failure or destruction. A second
pending request rejects without replacing it; changing the draft cancels that
request before starting another. A stale observation/cancel/commit cannot affect
the current request. Successful completion is terminal/idempotent for its identity;
another request requires an explicit edit, even to the same value. No hidden retry.

U4 adds only friendship, a private candidate-status query, and private
`adopt_ready(Prepared&&) noexcept` to `ScoreInput`. The existing U3 `adopt` retains
its guarded false-on-invalid contract and delegates successful moves to that body.
Apply validates ownership/generation/readiness and calls fallible `prepare` before
save. After save it calls only the private unchecked transfer, never a guard that
could discover invalidity after disk commit. The borrowed active value cannot be
modified externally; generation/pending management is exclusively this helper's.
All three borrowed owners remain stable/alive, with no intervening cancel/edit,
clear/shutdown, owner move or foreign/reentrant callback throughout synchronous
save/adopt. Concurrent polling retains its existing Engine contracts, but teardown
cannot be concurrent. This is a precondition, not a lock or shutdown transaction.

Unchanged-key Apply retains its active lease; both changed and unchanged requests
need witnessed active installation. Candidate evidence latches only for that exact
live record/generation. Clear/shutdown invalidates ownership even when old samples
still contain it. These checks are distinct from SET-003's new-poll/device-admission/
neutral-rearming obligations; U4 supplies none of that modal workflow.

Alternatives: raw owner pointers complicate moves and lifetime; bare context IDs
cannot prove live ownership; automatic supersession hides pending-work cancellation.
The weak record costs one small bookkeeping allocation per replacement in addition
to Engine attachment allocations, but makes drop/move/foreign-owner semantics
explicit without a global registry or task framework. Confidence is high after the
focused CPU checks in [next.md](next.md). API reversal is cheap before Game callers
exist. These are engineering decisions; [H1–H3] add no specific preference support.
U2 changes `Snapshot` layout, so Engine and consumers still rebuild together.

## SET-006 — Use one explicit, versioned settings file and strict validation

U1's `SettingsFile` resolves its path once with `absolute` and lexical
normalization; empty/NUL-containing paths reject as invalid arguments. The
future application selects `tutorial-settings.json` from its starting directory;
the file boundary itself does not select a global path or write during setup.
Restart acceptance uses the same resolved path. Tests inject clone-local disposable
paths. Asset registries, Engine startup configuration, sibling data, OS preference
directory discovery, profiles, migration, hot reload, and cloud storage stay outside
this unit.

The complete version-1 document is:

```json
{
  "version": 1,
  "camera_pan_speed": 100.0,
  "show_health": true,
  "bindings": { "add_score": "space" }
}
```

Require the root object and all fields shown; order is immaterial. Reject
duplicate/unknown keys at every level, wrong types (including numeric booleans),
unsupported/noninteger version, missing fields, malformed/trailing JSON, raw NUL,
files larger than 64 KiB, nonfinite/out-of-range speed, and unsupported/reserved
key tokens. Explicit typed validation, parsing, and save share one validator.
Both JSON integer and floating speed numbers are accepted, with range checking before
conversion to float. U4 validates the complete typed draft before staging or any
file I/O; save revalidates and serializes before opening its temporary. Numeric
editing text remains later work and must parse completely without locale-specific
separators or silent clamping. U4 may retain an invalid intermediate typed draft;
request validation rejects it without changing active ownership or file bytes.

U1 acquires at most 64 KiB plus one detection byte into a fixed buffer. Its SAX
reader permits only root/bindings objects and schema scalars; arrays/deeper objects
reject before constructing any DOM. Duplicate names compare after JSON escape
decoding. Reject every raw NUL before parsing because pinned JSON 3.11.3 treats it
as EOF even in strict mode. Ordinary trailing bytes/documents also reject. A DOM
is used only to serialize the fixed, validated schema.

Load produces an owned result: `Missing` selects all defaults without writing;
`Loaded` selects the entire valid snapshot; `Recovered` selects all defaults plus
a diagnostic containing path, field/operation, and reason. Inspection/open/read
failures are distinct from genuine absence. Loading rejects directory and symlink
leaves, including dangling symlinks, rather than following a target. Saving has
different leaf semantics: rename replaces the leaf link itself without writing
its referent. Ancestor traversal/race hardening remains outside this boundary.
Only settings data/I/O errors recover; allocation and programming errors propagate.
There is no partial adoption, automatic repair, or save during load/destruction.
The later editor must retain rejected bytes until deliberate successful Apply,
including unsupported-version files, and explain that replacement to the player.

The populated pinned nlohmann headers are private to `.cpp` files. App and
case targets each receive the private include, because Engine's include is not
transitive [E06]. `settings.cpp` is registered in both, with existing gameplay
cases preserved. U1 introduced no pin, Workers, or public Engine API changes;
U2's two Engine queries are the separate boundary described above.

Alternatives: handwritten JSON would add escaping/number/duplicate-key risks;
INI could suffice, but the repository already uses versioned JSON configuration.
Field-by-field recovery hides stale mixtures. Strict rejection costs tolerant
forward compatibility; changing persisted meaning requires a schema version.
Confidence is high for this bounded policy after focused native checks, not a
personal preference claim [H2–H3]. SAX is an engineering choice for bounded input;
a private parser/API swap is inexpensive before runtime callers exist.

## SET-007 — Make successful Apply a saved, nonthrowing adoption

The eventual editor states are Closed, Editing, Listening, AwaitingContext,
Saving and Rearming; they are not implemented by U4. The CPU helper instead returns
Idle, Pending, Ready, Applied, Cancelled, Stale, Invalidated or Failed for one exact
request generation. It owns a draft, validates it before staging a changed key,
and returns Pending without any file I/O until SET-005 evidence is ready. Editing
explicitly drops pending work, advances generation and replaces only the draft.
Repeat Apply while pending rejects; Cancel drops only that candidate and retains
the draft, active state and file. Future panel Cancel may discard the draft by
closing the editor. Cancel cannot interrupt or undo synchronous saving.

Construction checks active value/key agreement and rejects an existing score
candidate, without load, attachment or save. Destruction releases only its own
candidate; neither setup nor teardown autosaves. The helper is deliberately
noncopyable/nonmovable so its borrowed-owner and request lifetime stay explicit.

For this tiny bounded file, load occurs before runtime and Save runs on the
simulation owner at the explicit Apply boundary, while gameplay is gated. Do not
hold a runtime/platform lock, call platform dispatch and wait, or save in render
preparation. Sequential execution briefly stops polling during that call;
concurrent execution can queue polls. This latency is accepted for the first
consumer; asynchronous persistence requires a later contract.

U1's `SettingsFile::save` implements the file portion now composed by U4.
Validation and complete serialization precede opening
`<resolved-path>.pending` in exclusive binary mode. Its deterministic name makes
stale/conflicting attempts visible: fail once without touching that file instead
of retrying names or deleting another attempt. This private engineering choice
is cheap to reverse if later consumers need unique attempt names.

Write only a newly and exclusively created same-directory temporary file owned
by this attempt. Write all bytes, check write/flush/close, then replace the target
with a single rename operation. This replaces a symlink leaf itself and preserves
its referent bytes; it does not follow the leaf for writes. Never truncate/remove the old target first and
never reuse/delete a temporary file belonging to another attempt. On any failure
before rename, leave the old target bytes and active model/old context intact,
drop the candidate lease, retain the draft, and show the operation/path error.
Clean only this attempt's temporary file; if cleanup fails, retain its path in
the diagnostic. A failed replacement has no remove-and-retry fallback. The borrowed
app-private `Detail::SettingsFileOperations` seam exposes operations for failure
injection and must outlive `SettingsFile`. Close consumes its stream even on error;
replacement must not throw after committing. Ordinary save I/O failures become
`SettingsFileError`; invalid typed saves throw `invalid_argument` without creating
a temporary. Unexpected exceptions propagate after owned cleanup.

Successful rename is the commit point. U4 rechecks active/candidate ownership,
generation and consumed evidence before save, copies the accepted complete value,
and prepares an optional score token. It never does throwing value/result/handler
preparation after save: there are no callbacks, strings, UI or result containers in
completion. Invalidated, stale or cancelled requests reject before writing. U1's
validation/serialization also finish before exclusive temporary creation. Ordinary
file failure propagates its primary diagnostic, marks Failed, releases the prepared
candidate during unwinding, and keeps exact predecessor bytes, active value/key/
identity and draft. Only an explicit new request retries with a new generation.
Cleanup failure preserves the primary error and names the owned leftover temporary.

After save returns, `complete(...) noexcept` calls the private unchecked lease
transfer for a changed key, copies the trivial enum/bool/float value, and assigns
Applied. Static assertions require trivial/nothrow value copies, nonthrowing token
move/destruction and completion/adoption. Source review follows only shared/weak
pointer moves/reset, Context's atomic release/reset and primitive assignments;
none allocates. There is no conditional readiness rejection after disk commit.
Terminal Applied calls return the same status without another save or adoption.
Format status text and invoke presentation later.
Toolkit presentation is separate: its later failure follows normal UI/runtime
failure handling and must
not report that the saved model rolled back. The new model affects the next
permitted gameplay update; after focus release SET-003 rearming still applies.

This promises a complete old or new file under normal local replacement semantics,
not a disk+memory transaction across process death. A crash after rename can
leave the new file for next startup before in-memory adoption. Power-loss
durability/fsync, network filesystems, multiple processes editing the same path,
and filesystem race/security hardening are outside the first consumer. The first
native acceptance target is the existing Linux/GLFW/OpenGL/TGUI composition;
other hosts need replacement behavior checked before claiming support.

Alternatives: save-after-live-apply is simpler but creates accepted values that
may not survive restart; independent asynchronous setters require rollback and
completion semantics expressly deferred by the brief. Confidence is high in the
bounded CPU composition and file implementation based on the focused checks in
[next.md](next.md). U1's file cases cover real
replacement, all injected I/O failures, collision, cleanup failure, and unexpected
exceptions; U4 adds generation/poll/adoption and failure/retry composition cases.
Borrowing owners instead of relocating them keeps future Game ownership intact
but makes their stable lifetime/exclusive protocol access an explicit precondition.
Returning enums and propagating existing exceptions avoids a second error framework
and post-commit diagnostic allocations. Using guarded adoption after save was
rejected because its failure could leave disk/live state divergent. Synchronous
CPU composition is implemented; Game/runtime adoption remains unimplemented. No
existing general settings saver was found in the baseline [E05].

## Source evidence at the reviewed baseline

These are source observations, not executed behavior. Linked files are current
baseline files; line numbers describe `3542f883391d4225329dab8d5d61a5987e69c8f0`.

| ID | Declaration → implementation → concrete consumer |
| --- | --- |
| E01 | [Input System](../projects/engine/include/cheryl/core/controls/input-interface.h), `System`, lines 26–60: platform polling/mapping, cached endpoint, immutable samples. [Implementation](../projects/engine/src/core/controls/input-interface.cpp), lines 12–38: one latched focus gates keyboard and capture, then complete publication. [GLFW consumer](../projects/modules/platform/native-glfw/src/core/controls/input-system.cpp), lines 471–487: `begin_input_poll` precedes the native pump/update. |
| E02 | [Action contexts](../projects/engine/include/cheryl/core/controls/action-context.h), `Context`/`ContextSubmission`: owned definitions, move-only leases, next-poll requests. [Attach](../projects/engine/src/core/controls/action-context.cpp), lines 38–51 validates/builds before acceptance. [Bindings](../projects/engine/src/core/controls/input-bindings.cpp), lines 46–92 and 320–336 seeds pending contexts from held controls/device gates and builds all namespaces before semantic publication; lines 182–197 implement the device gate. [Snapshot](../projects/engine/include/cheryl/core/controls/action-snapshot.h), lines 63–108 has no membership or device-gate query. [PlayerController](../projects/apps/tutorial-game/src/player-controller.h), `keyboard`/`read`, is a real context consumer. |
| E03 | [Tick](../projects/engine/include/cheryl/core/controls/tick-input.h), `records`, `records_for`, `actions`: immutable shared batch, borrowed views. [Accumulator](../projects/engine/src/core/controls/input-accumulator.cpp), lines 6–24 concatenates complete-poll records. [Runtime](../projects/engine/src/core/game-framework/game-runtime.cpp), lines 515–555 and 990–1016 owns sequential/concurrent polling and transfer; no new settings poller is needed. |
| E04 | [TGUI Session](../projects/modules/ui/tgui/include/cheryl/ui/tgui/session.h), `Session`, focus and `handle_input` declarations: one global session, owner thread, borrowed input, explicit pointer selection. [Implementation](../projects/modules/ui/tgui/src/session.cpp), lines 175–200 and 400–453 owns Events/Text/focus leases and drains the entry epoch before releasing preempted focus. [Tutorial UIController](../projects/apps/tutorial-game/src/ui-controller.h), lines 36–61 uses one simulation session, manual time/input, scene upload/adoption, then quiescent teardown. [Interactive TGUI demo](../projects/apps/demo/src/tgui-demo.cpp), `create_widgets`/`handle_input` shows native buttons/editing and per-record pointer/focus policy. |
| E05 | [FileIndex](../projects/engine/include/cheryl/core/resources/fileio/file-mgr.h) and [implementation](../projects/engine/src/core/resources/fileio/file-mgr.cpp) index extension buckets. [FileRegistry](../projects/engine/include/cheryl/core/resources/asset-management/file-registry.h) and [implementation](../projects/engine/src/core/resources/asset-management/file-registry.cpp) own normalized path lists, not contents or transactional saving. [Loader](../projects/engine/src/core/resources/asset-management/asset-loader.cpp), lines 27–42, 409–417 feeds that registry. [DemoAudio::load](../projects/apps/demo/src/audio-demo.cpp), lines 32–48 resolves names then explicitly opens/decodes streams: the registry does not persist application state. Searches of `projects`/`tools` found test writers and configuration loaders, no reusable settings replacement writer. |
| E06 | [Worker JSON declarations](../projects/modules/runtime/workers/include/cheryl/core/engine/worker-topology/configuration.h) return inert typed values. [Parser](../projects/modules/runtime/workers/src/configuration.cpp), lines 139–198 rejects duplicates/unknown fields/version, then validates the whole value. [compose_workers](../projects/modules/runtime/workers/examples/shared-pool.cpp) concretely loads, validates, and only then constructs pool/backend/groups; [consumer](../projects/modules/runtime/workers/tests/consumer.cpp), lines 19–29 demonstrates typed/JSON equivalence. This loader has no settings-save API. [Engine CMake](../projects/engine/CMakeLists.txt), lines 27–29 keeps JSON headers private; [tutorial CMake](../projects/apps/tutorial-game/CMakeLists.txt) currently links Startup/GLFW/OpenGL/TGUI and registers `tests-tutorial-game` with Engine only. |
| E07 | [Tutorial Game](../projects/apps/tutorial-game/src/game.cpp), lines 22–34 binds Space directly and creates scoped player inputs; lines 89–95 hardcodes pan speed 100 and increments score before UI. [Hud](../projects/apps/tutorial-game/src/hud.h), lines 23–27 and 51–59 owns and updates the visible health bar. Settings hooks/setters described above do not yet exist. |
| E08 | [PlatformDispatcher::Submission](../projects/engine/include/cheryl/core/engine/platform-dispatcher.h), `submit`, always defers, transports values/exceptions, and exposes closure cancellation through the future. [Runtime](../projects/engine/src/core/game-framework/game-runtime.cpp), lines 520–530 and 976–995 drains platform work before polling; a callback cannot overlap a platform pump. [TGUI scene adoption](../projects/modules/ui/tgui/src/scene.cpp), lines 169–195 is a concrete submit/readiness-check consumer. Reuse that nonblocking progress pattern for the post-release watermark. |

U1 evidence, added after the reviewed baseline: [settings declarations](../projects/apps/tutorial-game/src/settings.h),
[value/parser/file implementation](../projects/apps/tutorial-game/src/settings.cpp),
[focused value/file tests](../projects/apps/tutorial-game/tests/settings.cpp), and
[tutorial registration](../projects/apps/tutorial-game/CMakeLists.txt). These are
app-private additions; they do not change the historical preference evidence.

U3 evidence is the private [owner/tokens](../projects/apps/tutorial-game/src/score-input.h),
[implementation](../projects/apps/tutorial-game/src/score-input.cpp), and
[controlled mapper/tick cases](../projects/apps/tutorial-game/tests/score-input.cpp),
registered in the existing tutorial suite. Game, UI, runtime and Engine source are
unchanged by U3. Actual checks and limits are in [next.md](next.md).

U2 evidence extends [Snapshot](../projects/engine/include/cheryl/core/controls/action-snapshot.h)
and [publication](../projects/engine/src/core/controls/input-bindings.cpp), using
the existing context map and an owned gate set prepared before commit.
[Context/query cases](../projects/engine/tests/unit/src/action-contexts.cpp) cover
history/lifetime, metadata-only backlog consumption, and controlled publication
failure/retry. The U1 [symlink case](../projects/apps/tutorial-game/tests/settings.cpp)
clarifies load-versus-save behavior without changing the writer. Actual execution
and its limits are recorded in [next.md](next.md), not inferred from source presence.

At the design checkpoint, these baseline tests were only inspected; current U2
execution is recorded separately in [next.md](next.md):
[action_contexts](../projects/engine/tests/unit/src/action-contexts.cpp) covers
`poll_boundary`, `latched_departure`, `focus_gate`, `attach_under_focus`,
`lifetime`, `validation`, and `retry`;
[input_routing](../projects/engine/tests/all-tests/src/core/input-routing.cpp)
covers stale leases and mid-poll focus;
[ui_tgui_session](../projects/modules/ui/tgui/tests/unit/src/session.cpp)
`routing`, `pointer_selection`, and `owner_teardown` covers toolkit boundaries.
`input-*` shared unit sources and `action-contexts.cpp` reach `tests-engine`
through [Engine test registration](../projects/engine/tests/CMakeLists.txt) and
its [unit suite](../projects/engine/tests/unit/CMakeLists.txt).
TGUI cases reach `tests-ui-tgui` through its
[unit suite](../projects/modules/ui/tgui/tests/unit/CMakeLists.txt).
[Tutorial gameplay tests](../projects/apps/tutorial-game/tests/gameplay.cpp)
cover independent contexts and existing behavior; they do not cover settings.

## Historical preference evidence and its limits

The archived v3 squash is **`eb3f81c219e779b0200c1e82e5bc7239386528e8`**,
subject **“V3.0a (#12)”**, author `cppcooper
<cppcooper@users.noreply.github.com>`, authored/committed
`2026-10-14T18:49:46-07:00` (committer GitHub). Its message preserves descriptions
of individual edits, but the original individual commits are not recoverable:
`git rev-list --count` of this revision returns 2 (initial README plus squash).
Consequently the following provenance uses the exact squash revision, file,
symbol/lines, and preserved edit description, not invented internal commit IDs.
Authorship/import membership alone is insufficient; the inferences below also
require matching rationale or concrete behavior. None is a direct user decision.

| ID | Intentional v3 evidence at that exact revision | Inference, confidence, and counterevidence |
| --- | --- | --- |
| H1 | `include/abstracts/singleton.h:7–11` explicitly assigns typed constructor arguments to the caller; `ConcreteSingleton::construct` at 98–107 forwards arguments and rejects duplicate construction. Squash body records “Adds new and revises existing Singleton template” and an earlier singleton edit explicitly rejecting repeated `construct_once`. | **Medium:** prefer a small caller-supplied typed value and visible invalid transitions over implicit mutation. Supports SET-002's typed setup, not singleton ownership. `get()` at 81–95 also lazily default-constructs; `SuperSingletonDerived` at 61–65 questions its construction model. No specific modern settings API is established. |
| H2 | `include/core/filemgr.h:9–24` declares a distinct `FileMgr`; `src/core/filemgr.cpp:49–82`, `FileMgr::load`, takes the caller's manifest path and exposes bad-registration/read/open/argument failures. Squash message records “Adds FileMgr implementation” then “Revises disk.h and separates FileMgr class.” | **Medium-low:** interpretation/discovery can have a distinct owner and explicit failure outcomes. Supports the file/UI/input separation in SET-006/007. There is no settings saver or external FileMgr consumer proving this proposed sequence. Registration is incremental and `unload` at 113–114 retains a TODO; transactional semantics cannot be inferred. |
| H3 | Negative controls: archived `include/core/engine.h:7–14` and `include/core/game.h:3–10` explicitly say v1.0.2/2018; the config-file comment in the latter at 38 is legacy. `include/core/modules/input-controls-2/input-context.h:5–6` labels itself incomplete, `include/core/modules/input-controls-2/input-system.h:11,23–24` questions replacement and has empty queue methods; `src/core/modules/input-controls-2/input-binding.cpp:14,30` proposes queuing while calling inline. | **No positive preference inference.** Imported globals, callbacks, queue/thread guesses, and old config comments cannot choose the new settings API. The squash ends in WIP pool/allocation refactors too. No supported v3 preference was found for JSON, toolkit, atomic saves, asynchronous setters, or modern poll ownership. |

Archive boundaries are explicit: `75bfca596c7246539c604d51d2a0d36a2577f40b`,
`cppcooper`, `2026-10-14T19:04:53-07:00`, merges `v3-incomplete` via PR #13.
`8854ef84f360fafa3572b58ee5440b5aabc0e67c`, same author,
`2026-10-14T19:24:32-07:00`, **“Replaces archived V3 files with the modern
development baseline”**, explicitly retires the archive while preserving ancestry
and preparing 58 modern commits for replay. Today's APIs are compatibility
evidence, including recent agent-authored work, not independent evidence of
personal taste even when a commit uses the same Git identity.

Reproduce the preference evidence entirely within this clone:

```sh
git show -s --format=fuller eb3f81c219e779b0200c1e82e5bc7239386528e8
git show eb3f81c219e779b0200c1e82e5bc7239386528e8:include/abstracts/singleton.h
git show eb3f81c219e779b0200c1e82e5bc7239386528e8:include/core/filemgr.h
git show eb3f81c219e779b0200c1e82e5bc7239386528e8:src/core/filemgr.cpp
git show -s --format=fuller 8854ef84f360fafa3572b58ee5440b5aabc0e67c
```

## Integration overlaps and lifetime completion

- **Input:** SET-003/005's device-gate and membership queries extend the shared
  input snapshot contract, including retained gate state. Context replacement is
  feature-local; tell the coordinator
  about this overlap before integrating other input consumers. Never merge a
  sibling branch as a shortcut.
- **Map storage:** both features need version rejection and failure-preserving file
  replacement. Keep settings JSON and its private writer independent; compare
  error/rename contracts at integration, not by adopting sibling code or format.
  FileIndex is not a common persistence service.
- **Tilemap/storytelling:** settings use game-owned semantic names, not tile IDs,
  encounter IDs, or session save data. Neither feature is a prerequisite. A future
  application save may reference settings separately; no combined save transaction
  is promised here.
- **UI/runtime:** share the existing TGUI session, target allocation, frame upload,
  view following, and owner rules. Do not add a native toolkit backend, an input
  pump, a global settings bus, or worker dependency. A failed scene upload remains
  a UI presentation failure, independent of whether settings were saved.
- **Shutdown:** add the app's `deinit_simulation` override, including partial-init
  cleanup: on that owner discard draft/reset pending context and rebind focus,
  retire pending editor generations/futures without waiting, stop callbacks,
  and release panel/widget references. [AbstractGame](../projects/engine/include/cheryl/core/game-framework/abstract-game.h)
  owns this hook's pairing and thread contract. After runtime joins simulation,
  follow the existing `close_after_quiescence` path;
  drop active context/endpoint before input teardown. Pending initial/application
  requests do not justify polling during teardown. Never autosave on destruction.
  Retained CPU/render scenes may outlive the toolkit under their existing rules.

## Smallest coherent implementation sequence

1. **U1 private value and file boundary** (implemented; [checks/handoff](next.md)): defaults,
   shared validation, strict parse/load, complete serialization, replacement save,
   and CPU/file failure tests. No GUI or input mutation yet. This settles data
   recovery and save preservation without accumulating callers.
2. **U2 snapshot presence and admission:** implement only `Snapshot::has_context`
   and `Snapshot::device_enabled`, publication metadata, and their focused cases.
   No score-context/editor integration or runtime gate belongs to U2.
3. **U3 app-private score-context owner** (implemented): retain active/candidate
   leases and generations, observe real consumed polls, cancel/drop/move safely,
   and validate a nonthrowing adoption. No Game wiring, file commit or modal policy.
   Preserve existing direct/player mappings and verify final release/retirement.
4. **CPU Apply composition** (proposed next boundary): combine validated settings,
   SettingsFile and the prepared score lease under explicit owner/teardown rules.
   Verify file failures preserve old active values/context and candidate lifecycle.
5. **One TGUI editor:** draft controls, explicit input ordering, dedicated rebind
   target/epoch, save-before-adoption, Cancel, focus loss, rearming, and lifecycle.
   Extend only directly affected application/test registration. Run the proposed
   focused checks and then the selected native consumer acceptance when execution
   is authorized. Update both decision views if evidence changes a choice.

The U3 assignment activates only step 3 and its focused checks. Steps 4–5 remain
later coordinator assignments, not work activated by this record. Current build
and test work uses the resource-gated shared-lock wrapper and one job. Exit 75
means checkpoint idle rather than an unchanged retry or an unrestricted fallback.

## Observable acceptance proposed for the completed consumer

| Case | Required observation |
| --- | --- |
| Absent file | Start with the selected path absent: speed 100, health visible, Space adds one point; no file is written until Apply. |
| Draft/defaults/cancel | Change all three values or select Defaults, then Cancel/reopen: active speed/health/key and file bytes are unchanged; a rejected file is unchanged too. |
| Apply and restart | Apply speed 200, health hidden, key B: pan advances 200 world units within float tolerance over one second of commanded game time, health bar hides, B scores and Space stops scoring. Restart with the same resolved file: these choices return. |
| Invalid persistence | Independently try truncated JSON, duplicate/unknown fields, missing fields, wrong types, out-of-range/nonfinite numbers, reserved/unknown tokens, unsupported version, and oversized input: complete defaults plus path/field diagnostic, no partial adoption or automatic rewrite. A deliberate Apply replaces the rejected document with valid version 1. |
| Save failure | Seed a valid old file; force temporary open/write/flush/close/rename failures. Compare exact old bytes, active values, old mapping, retained draft, and actionable error. No successful Apply is reported and no candidate controls gameplay. |
| Context timing | Delay polls, attach during a poll, publish an inactive candidate, and fail publication before retry. Apply stays pending until a complete consumed sample proves candidate membership; no unbind gap, blocking wait, or false installation acknowledgment. Cancel before Saving performs no write. |
| Input isolation | In both sequential and concurrent execution, open and type keys that also map gameplay, including a mixed opening/closing batch; edit/rebind/Apply/Cancel never scores, pans, moves, joins/leaves, or quits. Mouse gestures stay in UI. Hold the old/new action key across closing: no action until neutral rearm and a fresh press. Queued disabled-gate polls cannot rearm. A quick open/close before focus latches must still fence pre-release enabled polls through the platform watermark. |
| Rebind identity | Repeat, release, Text, modifiers, another device/target, stale epochs, and reserved keys cannot select a binding. Queued old-session Text/widget input cannot change draft values or Apply during Listening. A fresh permitted physical press changes only the draft; Cancel keeps the prior mapping. |
| Ownership/lifecycle | Focus preemption does not erase a newer owner's lease; cancel pending apply and close while editing/listening without autosave, deadlock, or calls after session/input teardown. Retained scene behavior remains intact. |

This table remains whole-consumer acceptance. U1 value/file, U2 snapshot and U3 owner cases
are reported in [next.md](next.md). Those results do not establish the future
editor, runtime Apply, input isolation, context replacement, or GUI presentation.

## Material questions retained for the next phase

There is no U1 prerequisite blocker after the selected native checks pass. The
coordinator populated JSON at unchanged pin
`b36f4c477c40356a0ae1204b567cca3c2a57d201` and resolved the initial missing
`assets/fonts/DejaVuSans.ttf` by initializing assets at unchanged pin
`98453b2a6e0edfcfba22eea3fde801526d72b861`. No font substitution or dependency
pin change was used. The configure warning about a disconnected Gainput HIDAPI
source directory did not block the selected Engine-based runner; it does not
establish building the graphical/native/TGUI targets.

File replacement evidence is bounded to the Linux host/filesystem in [next.md](next.md).
Power-loss durability/fsync, multi-process writers, network filesystems, OS
preference directories, other platforms, and reusable settings APIs remain later
scope. U3 retains candidate/generation and validates adoption, but future composition
still must coordinate file commit with serialized owner access and input teardown.
Its latched readiness cannot rearm gameplay. UI isolation, release watermarks and
sequential/concurrent application scheduling remain unimplemented.
The archived squash cannot recover finer authorship or design
intent; stronger personal-preference claims remain unsupported.
