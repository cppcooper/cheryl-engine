# Settings milestone: design and evidence

Prepared on `feat/settings` against source commit
`3542f883391d4225329dab8d5d61a5987e69c8f0`. The initial index and working tree
were clean, including untracked files. This record, [decisions](decisions.md),
and [next unit](next.md) are the entire change in this phase. All behavior below
is proposed unless identified as source inspection. No feature, build, native
test, download, installation, or runtime acceptance was executed.

The requested consumer is the existing `tutorial-game` with its existing TGUI
session. It exposes exactly two game settings and one rebindable gameplay action:

| Value | Default | Valid values and visible effect |
| --- | --- | --- |
| `camera_pan_speed` | `100.0f` | Finite number in `[25, 400]`, world units/second; A/D pans at this speed. |
| `show_health` | `true` | Boolean; shows/hides the existing health bar without changing health. |
| `add_score` binding | physical `Space` | One permitted unmodified keyboard key; a new press adds one point to the existing score. |

Settings open through a native TGUI button. The panel has speed, health visibility,
Rebind, Defaults, Apply, Cancel, and a status message. Defaults changes only the
draft; Escape cancels the panel, or just the current rebind operation when
listening. Closing the panel/window discards uncommitted edits. Engine options
requiring restart, asynchronous option setters, additional settings, gamepad/mouse
rebinding, and a general settings registry remain later work.

## Proposed caller sequence

This is an API sketch for app-private code, not an available Engine API or a
compiled example. `SettingsFile`, `SettingsEditor`, and `ScoreInput` are proposed
application helpers. The proposed Engine additions are immutable snapshot queries
for context membership and the published device gate.

```cpp
// main, before runtime.run(): choose the data path once, independently of assets.
SettingsFile file(std::filesystem::absolute("tutorial-settings.json"));
auto loaded = file.load_or_defaults(); // owned value plus missing/loaded/recovered status
Game game(engine, assets, loaded.value, std::move(file), loaded.issue);

// Game::init_platform(), on the platform owner:
auto& input = engine.input();
keyboard = input.keyboard_id();
contexts = input.action_contexts();   // retain endpoint; never borrow Bindings in update
score_input = ScoreInput(contexts.attach(score_definition(keyboard, active.add_score)));
// Keep the existing non-score mappings and controller contexts.

// Game::init_simulation(), on the simulation/UI owner:
ui.init_simulation();                // reuse its single TGUI Session
editor.connect(ui, active, file, contexts, keyboard);

// Game::update(tick), on the simulation/UI owner:
ui.route_input(tick.input.records()); // callbacks enqueue editor intents
editor.finish_batch(tick.input);      // open/edit/cancel/stage/observe/save
if (!editor.suppress_gameplay_this_tick()) {
    pan_camera(active.camera_pan_speed, tick.delta_seconds, tick.input);
    if (score_input.read(tick.input).pressed()) add_one_point();
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

`GameSettings` is an ordinary owned value containing the two settings and the
portable key enum. `Game` owns the active value, file adapter, score context, and
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

Current snapshots do not expose device gates. Propose
`bool Snapshot::device_enabled(DeviceId) const noexcept`, preserving an immutable
copy of `Bindings::disabled_devices_` at semantic publication. A device absent
from that set is enabled, matching the mapper's existing default policy; this is
admission state, not evidence that hardware exists. Use the cached live keyboard
ID and a real newly consumed poll. Copy gates consistently into root and contextual
snapshots in `make_snapshot`; retained samples never change when live policy
changes. Checking current focus or a larger tick-local poll ID alone is
insufficient: concurrent runtime may already have queued newer polls before
release. The watermark also covers open/close before Exclusive focus ever
latches. Test both backlog cases before integration.

Missing required capture/session support fails initialization visibly. A failed
focus request while open retains the draft and modal gate, reports the failure,
and leaves pointer Cancel available; it must not silently enable gameplay.

Alternatives: focus-only isolation leaks already collected input and permits a
held key to press again when the gate reopens [E02]. Disabling the input system
would also starve the editor. Confidence is high in the need for the explicit
gate; combined-batch and owner scheduling checks are still required.

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

Replace the tutorial's direct score mapping with a dedicated score context.
Keep existing player contexts and other direct mappings. A private
`score_definition(keyboard, key)` maps one chord to a context-local action ID;
the persisted identity is `add_score`, never an `Actions::Id`, `ContextId`,
`BindingId`, device ID, or native key integer.

Apply first validates the whole draft. If its key changed, attach a candidate
context while retaining the old lease, active model, and modal gate. Acceptance
can throw synchronously, and a valid lease is not proof of installation. Continue
updates and require a consumed complete sample containing the candidate namespace.
Then attempt the file commit [SET-007]. On success, move the new lease into the
score owner and reset the old lease; query only the new namespace thereafter.
No global `Bindings::clear`, simulation-side mapper mutation, or unbind-first gap
is allowed. An unchanged key keeps its current context. Scope each pending Apply
to its draft generation so cancelled work cannot commit a later edit.

Current `Snapshot` has contextual queries but no membership query: absent and
inactive buttons both return defaults. Propose the minimal additive
`bool Snapshot::has_context(ContextId) const noexcept` over its immutable
`contexts_` map. It means membership in **this published sample**, not a live
lease/installation acknowledgment beyond that sample. Zero is absent; a departing
context remains present for its final release/zero sample, then disappears. This
must be tested before relying on it. Do not use a fake axis, held-key probe,
sleep, future `.get()`, or mere poll-number advance as acknowledgment.

The valid candidate lease plus `tick.latest_poll()->has_context(candidate.id())`
is the adoption precondition. Initial attachment uses the same publication gate
before gameplay starts. No-progress stays pending, with Cancel available; it does
not invent a timeout success. Input clear/closure invalidates the lease: show a
failure and preserve active/disk values. Poll/publication errors remain runtime
failures under the existing runtime contract; no settings-specific retry loop
or second polling owner is introduced. This context-observation path uses no
future; the separate release watermark in SET-003 retrieves a future only after
readiness, without waiting.

Alternatives: dispatching a mutation to platform would still need evidence of a
completed poll and would tie the editor to mutable mapper lifetime. Waiting one
frame is incorrect when polling and updates run independently. Confidence is
high that publication must be observable, medium in adding these snapshot queries;
confirm it is sufficient with the installation/removal/retry cases [E02].

## SET-006 — Use one explicit, versioned settings file and strict validation

The application selects an absolute `tutorial-settings.json` path once from its
starting working directory; relative injected paths resolve at construction.
Restart acceptance uses that same resolved path. Tests inject paths under their
own disposable directory. Do not put preferences in the asset registry, engine
startup configuration, or a sibling feature's data. OS user-directory discovery,
profiles, cloud storage, migration, and hot reload are deferred.

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
duplicate/unknown keys at every object level, wrong types (including numeric
booleans), unsupported/noninteger version, missing fields, malformed/trailing
JSON, files larger than 64 KiB, nonfinite/out-of-range speed, and unsupported or
reserved key tokens. Share value validation between typed construction, parsing,
and Apply. Accept both JSON integer and floating speed numbers, with checked
conversion to the finite float range. Numeric editing text must parse completely
without locale-specific separators; do not silently clamp or accept a prefix.

Load into a temporary value and return an owned result: `missing` selects defaults
without writing; `loaded` selects the entire valid snapshot; `recovered` selects
all defaults plus a diagnostic containing path, field/operation, and reason.
Malformed/invalid, future-version, and operational read failures are explicit
recovery outcomes. Allocation/programming failures propagate. Never partially
adopt a document, silently repair it, or save during load/shutdown. Preserve a
rejected file until a deliberate successful Apply; show that Apply will replace
the rejected file, including an unsupported-version file. Cancel preserves it.

Use the repository's pinned nlohmann JSON headers, once available, privately in
the app's `.cpp`. Engine's private include does not propagate to this consumer
[E06]; a later unit
must add a private app/test include from the existing checkout, without changing
dependency pins, downloading, exporting JSON types, or linking Workers for its
parser. Reuse the validation pattern, not the worker configuration schema/API.

Alternatives: a tiny handwritten JSON parser would create unnecessary escaping,
number, and duplicate-key risks. INI could suffice, but the repository already
uses versioned JSON configuration. Field-by-field recovery hides stale mixtures;
strict rejection costs forward compatibility and intentionally requires a new
version for extension. Confidence is medium-high as a design; neither JSON nor
this recovery policy is an established personal preference [H2–H3].

## SET-007 — Make successful Apply a saved, nonthrowing adoption

The externally visible states are Closed, Editing, Listening, AwaitingContext,
Saving, and Rearming. Only one Apply may be pending. Validation failure leaves
the draft editable and active/file/context unchanged. Successful validation
prepares the complete serialized bytes before any publication; a changed key
then follows SET-005. Cancel before Saving resets the candidate and discards the
draft without writing. No further edit/Apply is accepted while awaiting context;
Cancel remains available. Saving is a single synchronous operation, so Cancel
cannot interrupt or undo its commit point.

For this tiny bounded file, load occurs before runtime and Save runs on the
simulation owner at the explicit Apply boundary, while gameplay is gated. Do not
hold a runtime/platform lock, call platform dispatch and wait, or save in render
preparation. Sequential execution briefly stops polling during that call;
concurrent execution can queue polls. This latency is accepted for the first
consumer; asynchronous persistence requires a later contract.

Write only a newly and exclusively created same-directory temporary file owned
by this attempt. Write all bytes, check write/flush/close, then replace the target
with a single rename operation. Never truncate/remove the old target first and
never reuse/delete a temporary file belonging to another attempt. On any failure
before rename, leave the old target bytes and active model/old context intact,
drop the candidate lease, retain the draft, and show the operation/path error.
Clean only this attempt's temporary file; if cleanup fails, retain its path in
the diagnostic. A failed replacement has no remove-and-retry fallback.

Successful rename is the commit point. All fallible allocation/serialization and
context installation precedes it. Afterwards only nonthrowing value assignment,
context move/reset, and editor-state change adopt the candidate; store the key
as an enum so publication needs no string allocation; format status text later.
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
transaction ordering, medium in the file replacement implementation until its
failure paths are exercised. No existing general settings saver was found [E05].

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

Current tests corroborate intended contracts but were only inspected:
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

1. **Private value and file boundary** (the sole [next unit](next.md)): defaults,
   shared validation, strict parse/load, complete serialization, replacement save,
   and CPU/file failure tests. No GUI or input mutation yet. This settles data
   recovery and save preservation without accumulating callers.
2. **Poll-visible score context and device gate:** add/test
   `Snapshot::has_context` and `Snapshot::device_enabled`, introduce the private
   score context definition/owner, and stage replacement behind a modal gate.
   Exercise inactive publication, retained gates, release watermarks, queued
   focus-suppressed polls and quick open/close without a focused poll,
   held seeding, old/new coexistence, release retirement, validation failure, and
   shutdown invalidation. Keep other mappings and PlayerController behavior intact.
3. **One TGUI editor:** draft controls, explicit input ordering, dedicated rebind
   target/epoch, save-before-adoption, Cancel, focus loss, rearming, and lifecycle.
   Extend only directly affected application/test registration. Run the proposed
   focused checks and then the selected native consumer acceptance when execution
   is authorized. Update both decision views if evidence changes a choice.

No implementation phase begins as part of this design task. Each later unit needs
its selected task and applicable source/API/testing instructions. Future builds
must use the workspace's allowance/build-lock rules; this record grants none.

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

These cases are designs, not written or executed new tests. Existing tests were
read only. This task's verification is source/history tracing, record consistency,
local-link/provenance checks, and Git diff review; no runtime claim follows.

## Material questions retained for the next phase

No routine user decision is needed to start the next unit. The implementation must
establish replacement behavior on the selected local filesystem, including
failure injection; that is evidence still missing, not a claimed guarantee.
The snapshot queries must preserve final-release membership and historical
device-gate semantics, and be shown sufficient under delayed/failed/queued polls
before the editor depends on them. The existing JSON dependency is pinned but
unpopulated in this clone: `git submodule status -- extern/json` reports
`-b36f4c477c40356a0ae1204b567cca3c2a57d201`. Native parser compilation needs that
dependency supplied under a later task's authority; no retrieval occurred here.
The archived squash cannot recover finer authorship or design intent; stronger
personal-preference claims remain unsupported. Cross-platform replacement,
power-loss durability, OS preference directories, and reusable settings APIs
remain later design scope rather than implicit promises.
