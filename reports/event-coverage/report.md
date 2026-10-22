# Engine and module event coverage audit

**Later audit:** [Event publication and integration audit](re-audit.md) reassesses
publication selection, delivery and integration replacement. This initial report
retains its original findings and source baseline.

**Reviewed:** 2026-10-18

**Source baseline:** `aa4d670ee44bffb835f6a49eb653fe8a1175717b`

**Scope:** engine and module production C++; static investigation and recommendations.

The engine already has an event bus and delivery onto the simulation owner before
game updates. The main gap is publication and integration: useful state changes
are detected internally, but consumers cannot subscribe to them through a suitable
public path. The first changes should cover a complete runtime view, native window
focus, and gamepad availability.

For the UI issue that prompted this audit, the runtime should commit a copied view
snapshot and notify interested simulation services when that snapshot changes.
Context-created TGUI and RmlUi sessions could then receive their initial view and
subsequent changes automatically. This needs an explicit snapshot, ordering and
lifetime contract; subscribing directly to today's framebuffer resize channel
would leave important gaps.

This report recommends engineering work. It does not implement events or change
tutorials, API comments, the roadmap or testing requests.

## Scope and evidence

[The inventory](inventory.csv) records **65 occurrence families**, their current
delivery paths, possible consumers, proposed publication owners, priorities,
constraints and source references. These are families of observable behavior, not
65 proven bugs or 65 required EventBus channels. Logical-only resizing, for
example, is a case covered by the proposed complete-view change.

| Classification | Families | Meaning |
| --- | ---: | --- |
| Published | 1 | Concrete production EventBus publication exists. |
| Notification gap | 11 | State is observed or committed internally, but an independent consumer lacks the proposed subscription path. Some are optional extensions. |
| Existing path | 39 | A callback, input record, hook, future, result, snapshot or explicit control path already exposes the behavior. |
| Capability gap | 13 | The public/backend observation or state model needs work before an event can be promised. |
| Application owned | 1 | The engine supplies data, while the application owns the relevant transition. |

Priority **P1** means the first coherent change; **P2** means a useful next candidate;
**P3** means optional or deferred work requiring a consumer. **Retain** identifies
an existing mechanism to keep. Classification and priority answer different
questions: an existing snapshot can be adequate for its requester while optional
fanout would help a separate diagnostics service.

The production screen covered **397 tracked C++ files** under
`projects/engine` and `projects/modules`, excluding `tests` and `examples`
directories. Headers, implementations and production private/support sources were
included. The whole corpus was searched for event, callback, lifecycle and
state-transition patterns; responsible interfaces and implementations were traced
by subsystem. This was not a claim to have semantically reviewed every line of
every screened file.

| Owner | Screened files |
| --- | ---: |
| Engine | 276 |
| Audio / miniaudio | 5 |
| Graphics / OpenGL | 38 |
| Platform / debug-terminal-linux | 11 |
| Platform / native-glfw | 18 |
| Platform / native-sdl3 | 1 |
| Runtime / startup | 10 |
| Runtime / workers | 8 |
| UI / RmlUi | 14 |
| UI / TGUI | 16 |

Tests and relevant architecture/planning documents supplied supporting evidence.
Third-party internals, application-domain behavior, build/CI/tool lifecycle and
unimplemented engine facilities were outside the primary corpus. World/entity,
physics, networking and similar future systems cannot supply missing current
producers.

Concurrent application, documentation and UI API-comment changes appeared during
the audit. They were left untouched and are excluded from its conclusions. Findings
refer to the recorded committed engine/module baseline; source links use paths
and line numbers at that baseline.

## Existing transport and delivery

The production search found **one concrete bus occurrence family**:
`WindowResized`, published twice for compatibility, through the legacy named
`"window-resized"` route and the typed `window_resized_event`. It contains a
framebuffer size and a borrowed window pointer. No concrete engine/module
subscription was found outside the generic bus/facade infrastructure. This does
not mean input, task completion or toolkit interactions have no notification
mechanism. [GLFW publication](../../projects/modules/platform/native-glfw/src/core/display/window.cpp#L153),
[resize payload](../../projects/engine/include/cheryl/core/display/window-interface.h#L30).

The existing pieces already cover most transport needs:

| Mechanism | Current contract | Consequence |
| --- | --- | --- |
| EventBus | Immediate by default; typed channels identify exact name and payload type; queued delivery owns payload copies and requires an error sink. | Choose the execution owner explicitly. Typed payloads do not marshal callbacks by themselves. |
| Delivery adapters | Platform, simulation and worker delivery exist. | Reuse the existing transport for suitable cross-owner occurrences. |
| Simulation dispatcher | Each drain detaches one pending batch; reentrant/new posts wait for another boundary. | “Before update” does not mean recursively emptying the queue. |
| Registration | A persistent, copyable ID rather than an owning RAII subscription. | Sessions/services need explicit invalidation and safe capture lifetime. |
| Bus scope | The default EventSystem and independently constructed facades own different buses. | A matching channel name on an unrelated bus cannot hear the GLFW publisher. |
| State seeding | Registration stores a callback; it does not replay the latest value. | A new subscriber needs a separate initial snapshot or an integrated snapshot-and-subscribe operation. |

Sources: [bus delivery](../../projects/engine/include/cheryl/core/subsystems/event-bus.h#L34),
[registration and dispatch](../../projects/engine/include/cheryl/core/subsystems/event-bus.h#L124),
[owner adapters](../../projects/engine/include/cheryl/core/engine/event-delivery.h#L15),
[detached drain](../../projects/engine/src/core/engine/simulation-dispatcher.cpp#L77),
[facade and bus scope](../../projects/engine/include/cheryl/core/subsystems/event-system.h#L16),
[registration implementation](../../projects/engine/src/core/subsystems/event-bus.cpp#L30).

## Findings that should drive the first change

### 1. Publish the complete view consumed by an update

**Inventory:** W02–W03. **Priority:** P1.

The GLFW logical-size callback only updates its cache. The framebuffer callback
publishes only pixel size. A logical size can therefore change without a matching
bus occurrence. Both UI sessions need the logical and framebuffer dimensions:
their `set_view` implementations update logical layout, drawability and coordinate
conversion. Their context constructors currently import input/fonts, with no view
subscription. [logical-size callback](../../projects/modules/platform/native-glfw/src/core/display/window.cpp#L101),
[framebuffer publication](../../projects/modules/platform/native-glfw/src/core/display/window.cpp#L153),
[TGUI construction](../../projects/modules/ui/tgui/src/session.cpp#L211),
[TGUI view](../../projects/modules/ui/tgui/src/session.cpp#L266),
[RmlUi construction](../../projects/modules/ui/rmlui/src/session.cpp#L284),
[RmlUi view](../../projects/modules/ui/rmlui/src/session.cpp#L339).

The runtime already samples both dimensions and transfers copies to
`TickContext`. It is the natural owner of the simulation-visible pair. Publish
changes to that committed pair, including logical-only changes, rather than
requiring every game to compare sizes and call each session on every update.
The native callbacks may remain low-level observations.
[sequential sampling](../../projects/engine/src/core/game-framework/game-runtime.cpp#L296),
[concurrent handoff](../../projects/engine/src/core/game-framework/game-runtime.cpp#L622).

### 2. The current resize stream is insufficient as authoritative view state

**Inventory:** W01–W03. **Priority:** P1 integration constraint.

Legacy dispatch runs before typed dispatch. The existing native acceptance test
explicitly expects a nested legacy listener to produce typed sizes in the order
**inner, outer**, even though the final cached size is the inner value. Another
test expects a throwing legacy listener to prevent typed delivery entirely.
Callback failures are deferred through the native error boundary.
[failure boundary](../../projects/modules/platform/native-glfw/src/core/display/window.cpp#L114),
[dispatch order](../../projects/modules/platform/native-glfw/src/core/display/window.cpp#L153),
[nested resize expectations](../../projects/modules/graphics/opengl/tests/acceptance/src/native-opengl.cpp#L2274),
[legacy failure expectations](../../projects/modules/graphics/opengl/tests/acceptance/src/native-opengl.cpp#L2313).

Consequently, blindly applying each typed resize occurrence can restore an older
observation. This is source-backed existing behavior, not a newly observed test
failure. Preserve its compatibility contract while introducing a committed view
revision and a clear freshness rule for automatic consumers.

The borrowed `Window*` is another limit: copying the event does not retain the
window or make platform-owned getters legal on the simulation thread. The new
view payload should contain copied values and a scoped identity usable without
dereferencing a native/window object.
[window ownership and payload](../../projects/engine/include/cheryl/core/display/window-interface.h#L11).

### 3. Delivering before update needs a coherent snapshot boundary

**Inventory:** W02, R06. **Priority:** P1 integration constraint.

The proposed “dispatch before the game's update” boundary already exists.
Sequential execution samples sizes before draining the simulation dispatcher.
Concurrent execution drains first and then locks the scheduler to take the input
backlog and size snapshot. These are separate operations.
[sequential update boundary](../../projects/engine/src/core/game-framework/game-runtime.cpp#L296),
[concurrent update boundary](../../projects/engine/src/core/game-framework/game-runtime.cpp#L492).

**Inference for the proposed feature:** a notification queued from an earlier
platform observation could run before an update whose size snapshot is newer.
FIFO delivery alone would not guarantee that an automatic session's view equals
the `TickContext` view for that update. This is a contract to resolve when adding
automatic subscription, not a defect established in the current explicit-size API.

Commit the chosen update snapshot on the simulation owner, notify view consumers
from that commit, and pass the same pair to game update. Do not hold the scheduler
lock while invoking listeners. Decide whether intermediate platform sizes are
coalesced; a latest-view service normally needs the final committed pair rather
than every native callback.

Initial state also matters. In concurrent mode `game.init()` runs on the caller
before the simulation worker starts. Toolkit sessions retain their creating owner;
the runtime UI test creates a session in the first update. A session created after
a boundary notification must still acquire the current view immediately on its
owner. Merely emitting one startup event will miss such a subscriber.
[concurrent initialization](../../projects/engine/src/core/game-framework/game-runtime.cpp#L437),
[creating owner](../../projects/modules/ui/tgui/src/session.cpp#L169),
[first-update session construction](../../projects/modules/ui/tgui/tests/unit/src/runtime.cpp#L316).

### 4. Native focus changes are detected but exposed only through logs

**Inventory:** W04; related I05–I06. **Priority:** P1.

The GLFW input adapter samples `GLFW_FOCUSED`, compares it with retained state and
logs changes. The portable input record variant contains button, axis, pointer,
scroll and text records, with no native window-focus payload. Independent pause,
audio or UI services have no public subscription for this observation.
[focus detection](../../projects/modules/platform/native-glfw/src/core/controls/input-system.cpp#L374),
[portable record types](../../projects/engine/include/cheryl/core/controls/input-record.h#L28).

Publish observed native focus with copied window/source identity and an initial
snapshot. Keep it distinct from **requested keyboard routing focus** and the
**effective focus latched for an input poll**. They answer different questions.
Current sessions finish processing the old routed record batch before releasing
preempted toolkit focus; a new listener must preserve that ordering.
[poll focus latch](../../projects/engine/src/core/controls/input-interface.cpp#L6),
[routing requests](../../projects/engine/src/core/controls/input-routing.cpp#L27),
[TGUI preemption](../../projects/modules/ui/tgui/src/session.cpp#L368),
[RmlUi preemption](../../projects/modules/ui/rmlui/src/session.cpp#L385).

A poll-based source observes sampled state changes; guaranteeing every transient
native focus transition would require a different capture contract.

### 5. Gamepad availability is detected but unavailable through generic delivery

**Inventory:** I03; separate future scope I09. **Priority:** P1.

The adapter detects and logs a change in its convenience gamepad's availability.
It reconciles controls so unplugging cannot leave buttons or axes stuck, and
offers a backend-specific device snapshot. Those paths do not provide a generic
availability subscription for reconnect UI or controller policy.
[backend device query](../../projects/modules/platform/native-glfw/src/core/controls/input-system.cpp#L257),
[availability observation](../../projects/modules/platform/native-glfw/src/core/controls/input-system.cpp#L398),
[control reconciliation](../../projects/modules/platform/native-glfw/src/core/controls/input-system.cpp#L409),
[generic input surface](../../projects/engine/include/cheryl/core/controls/input-interface.h#L16).

Attach the observed availability change to a defined completed-poll/update
boundary and preserve the neutralizing control transitions. Specify identity,
initial state and reattachment semantics. This bounded change does not require
promising a full multi-controller inventory, which the current generic API does
not supply.

## Other useful candidates and boundaries

| Area and inventory IDs | Finding | Recommended treatment |
| --- | --- | --- |
| Audio AU01 | Natural voice end is detected when state is sampled and exposed as `Finished`; no completion listener exists. | Useful P2 candidate for music queues/dialogue. Include voice/playback generation and terminal reason. |
| Audio AU05–AU07 | Device recovery and later stream failure have no complete public status/observation model. Initial stream setup already waits and returns/throws. | Investigate backend observations before promising events. This audit does not establish which later native failures occur or are lost. |
| Assets A03–A06 | Preparation returns a batch; upload can publish partially before throwing; reload preserves the old shader on failed replacement. | Optional batch/key/generation notifications for independent loading/tool services, after publication commits. |
| Runtime R01–R05, R08 | Lifecycle hooks, stop tokens and tick timing exist; pressure/policy degradation is mainly logs/counters. RuntimeStats is unavailable while running. | Keep control paths; consider coalesced live health observation for supervisors. |
| Display W08–W12 | Monitor refresh, per-window scale and several native interaction capabilities are absent. | Treat as capability work with consumers, rather than missing dispatch calls. Monitor hotplug/scale already has deferred planning. |
| Input I01–I02, I05–I08 | Ordered records, immutable action snapshots and routing/capture leases already handle input. | Preserve sequence, poll and epoch semantics. Avoid duplicating input on a generic bus. |
| UI U02–U04, text A07–A08 | Toolkit interactions, scene futures/adoption and retained font/text snapshots already expose relevant changes. | Keep local mechanisms; publish application commands at the controller boundary when needed. |
| Graphics G01–G05 | Resource retirement, domain state, camera revisions and deferred native diagnostics already have owners. | Prefer existing queues, revisions and sampled diagnostics. Avoid a default event for every GPU object or frame. |
| Startup/workers/debug terminal M01–M03, R07–R10 | Factories, futures and process supervision exist; topology is construction-time and SDL is a scaffold. | Keep request/control paths; require capability work before topology or SDL conformance claims. |
| Utilities/memory/logging S01–S04, L01–L02 | Local observers, state hooks, lifetime mechanisms, file callbacks and failure counters already exist. | Preserve dedicated contracts; optional health fanout must stay outside allocator/backend failure paths. |

Evidence: [audio completion](../../projects/modules/audio/miniaudio/src/system.cpp#L52),
[audio state model](../../projects/engine/include/cheryl/audio/system.h#L12),
[initial stream handling](../../projects/modules/audio/miniaudio/src/system.cpp#L126),
[partial uploads](../../projects/engine/src/core/resources/asset-management/asset-loader.cpp#L230),
[reload commit](../../projects/engine/src/core/resources/asset-management/shader-mgr.cpp#L18),
[cache lock/callback](../../projects/engine/include/cheryl/templates/asset-mgr.h#L166),
[runtime diagnostics access](../../projects/engine/src/core/game-framework/game-runtime.cpp#L86),
[worker policy handling](../../projects/engine/src/core/engine/worker-pool.cpp#L112),
[deferred monitor work](../../docs/planning/long-term/README.md#L275),
[font catalogue replacement](../../projects/engine/src/text/font-service.cpp#L220),
[GPU retirement](../../projects/modules/graphics/opengl/src/backends/opengl/resource-lifetime.cpp#L121),
[camera revisions](../../projects/engine/include/cheryl/core/rendering/camera.h#L10),
[logging callbacks](../../projects/engine/include/cheryl/core/logging/types.h#L18).
The inventory supplies source references and owner constraints for every row.

Audio requires particular care: the public system is independently
application-owned, with no automatic runtime maintenance phase. General EventBus
dispatch can allocate, lock and run arbitrary listeners. A native mixing callback
should instead write a bounded completion record for a non-real-time owner, or
completion should be observed during suitable maintenance. Define overflow and
duplicate-completion behavior before selecting either approach.
[audio ownership](../../projects/engine/include/cheryl/audio/system.h#L55),
[voice maintenance](../../projects/modules/audio/miniaudio/src/system.cpp#L80),
[dispatch implementation](../../projects/engine/src/core/subsystems/event-bus.cpp#L66).

Asset notifications must also stay outside internal locks. The cache's existing
publication callback runs under a write lock for internal metadata updates; it is
not an appropriate insertion point for arbitrary listeners. Notify after the
commit/unlock, while retaining enough copied identity to describe the result.
[publication callback](../../projects/engine/include/cheryl/templates/asset-mgr.h#L166).

## Recommended contracts before implementation

These are proposed requirements, not claims about APIs already available.

1. **Scope the source.** Prefer a context/runtime view source with explicit
   lifetime. Context currently exposes the platform dispatcher; the simulation
   dispatcher belongs to GameRuntime. A session's context constructor cannot gain
   correct delivery merely by calling an existing context accessor. Provide the
   needed source/owner binding without coupling toolkit modules to a global
   runtime singleton. A shared bus also needs context/window and run-generation
   identity. [context accessors](../../projects/engine/include/cheryl/core/engine/engine-context.h#L98),
   [runtime dispatcher ownership](../../projects/engine/include/cheryl/core/game-framework/game-runtime.h#L100).
2. **Publish owned state.** A view contains logical size, framebuffer size and
   revision, with source identity valid across queued delivery. Preserve zero
   extents. Establish a new generation when a runtime/source restarts.
3. **Commit once per chosen update view.** Freeze the pair used for that update,
   apply changed state to subscribers on the simulation owner, then call update
   with the same values. State whether intermediate sizes are coalesced and how
   generic queued work relates to this commit.
4. **Seed late subscribers.** Read-current plus subscribe must not lose or regress
   a change. Serializing both operations on the view owner is one suitable
   design. A session created during its first update gets the current snapshot
   before layout/input/recording. Standalone sessions retain explicit view input.
5. **Own subscriptions.** Unregister before destroying callback captures. Account
   for queued work and already running callbacks; choose an RAII wrapper or
   another explicit lifetime owner. Waiting on one's own invocation is rejected
   by the bus. [removal barriers](../../projects/engine/include/cheryl/core/subsystems/event-bus.h#L190).
6. **Define failures and closure.** Choose whether a failed internal view listener
   stops the runtime or reports through a protected error owner; do not silently
   assume fire-and-forget success. Simulation delivery closes before quiesce, so
   required shutdown work cannot depend on a late queued simulation event.
   [sequential closure](../../projects/engine/src/core/game-framework/game-runtime.cpp#L371),
   [concurrent closure](../../projects/engine/src/core/game-framework/game-runtime.cpp#L704).
7. **Keep observations separate from decisions.** Stop tokens, required lifecycle
   hooks and requester futures remain their control/completion mechanisms.
   A close-request notice does not by itself provide a veto; a focus-request
   notice does not prove that a poll used that focus.

The first engineering unit should establish the complete-view source and automatic
context-session binding in both runtime modes. Native focus and the existing pad's
availability are the next bounded producers to expose. Audio completion and
asset/runtime observation need separate consumer-driven decisions; monitor refresh,
device recovery and broad device inventory remain capability projects.

## Acceptance evidence and remaining verification

Existing tests provide useful coverage to extend, but **none were executed for this
audit**.

| Existing source evidence | What it establishes in the test code | Additional acceptance needed |
| --- | --- | --- |
| [typed event unit tests](../../projects/engine/tests/unit/src/typed-events.cpp#L39) and [queued acceptance tests](../../projects/engine/tests/acceptance/src/typed-events.cpp#L62) | Channel identity, payload ownership, cancellation, failure handling and delivery ordering have targeted tests. | Scoped source identity, initial snapshot and automatic subscription lifetime. |
| [runtime owner-delivery test](../../projects/engine/tests/all-tests/src/core/runtime-adapter.cpp#L1923) | A manually dispatched resize payload is delivered through owner adapters in both run modes. | Authoritative view/tick coherence rather than merely callback thread identity. |
| [native resize tests](../../projects/modules/graphics/opengl/tests/acceptance/src/native-opengl.cpp#L2231) | Test expectations include unchanged sizes, nested legacy/typed ordering and deferred failures. | Preserve compatibility while rejecting stale committed-view updates. |
| [TGUI runtime test](../../projects/modules/ui/tgui/tests/unit/src/runtime.cpp#L316) and [RmlUi runtime test](../../projects/modules/ui/rmlui/tests/unit/src/runtime.cpp#L296) | Sessions are created on the update owner and use explicit initial view setup. | Initial automatic seed, resize delivery and teardown for both adapters. |
| [audio state tests](../../projects/modules/audio/miniaudio/tests/unit/src/audio.cpp#L121) | Voice controls, completion and restart are checked through snapshots. | If AU01 is selected: once-per-playback completion, owner delivery, stop/close distinction and restart races. |

The first implementation's acceptance should include:

- No resize at startup; subscription during the first update and later updates.
- Logical-only, framebuffer-only and combined changes; unchanged pairs do not
  trigger repeated layout work; zero-size/minimized and restoration behavior.
- Several native changes between updates; reentrant changes; view revision
  monotonicity; the subscriber and tick observe the same committed pair.
- Both runtime modes, a slow simulation, recovery updates, and no-update cycles.
- Multiple listeners and scoped sources; invalidation before queued invocation,
  session destruction, runtime shutdown and source restart.
- Explicit listener failure policy, including the existing legacy-resize failure
  behavior.
- For native focus and device availability: initial state, repeated same state,
  disconnect/reconnect, neutral controls and preservation of input batch ordering.

These cases are proposed acceptance, not added tests. No builds, native tests,
full documentation generation, dependency operations, device access or external
services were used. Validation of the report artifacts is limited to CSV structure,
source-link/line existence, baseline consistency and whitespace checks.

## Reproducing the screen

The concrete publication/subscription search was:

```sh
rg -n 'EventChannel[[:space:]]*<|EventSystem::get\(|register_listener\(|unregister_listener\(|unregister_and_wait\(|\.dispatch\(|->dispatch\(' \
  projects/engine projects/modules \
  -g '*.{h,hpp,inl,ipp,tpp,cpp,cc,cxx}' \
  -g '!**/tests/**' -g '!**/examples/**'
```

The 397-file denominator is the tracked files below those two roots whose suffix
is `.h`, `.hpp`, `.inl`, `.ipp`, `.tpp`, `.cpp`, `.cc` or `.cxx`,
excluding files with a `tests` or `examples` path component. This makes the count
independent of local build outputs. The bus search was supplemented with callback
registrations, state comparisons, status enums, lifecycle methods, futures,
diagnostic counters and their owners; absence of a bus match alone was not used
to classify an occurrence as missing.
