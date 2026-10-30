# Event publication and integration audit

**Reviewed:** 2026-10-18

**Source baseline:** `ccf514018faa9a5c5f0750c21c0da321c0f82e96`

**Later design assessment:** [Engine and module event design](design.md) develops
the selection, lifecycle protocols and integration replacements further, with a
[23-row contract catalogue](event-catalogue.csv). Its recommendations supersede
this audit's proposed selection; the findings below retain their reviewed scope.

**Scope:** engine and module production C++, with application callers and tests
used to assess integration and migration consequences.

Publish meaningful transitions when event dispatch supplies useful communication
between engine components, modules or applications. Existing internal handling
does not settle that decision. An integration can also be worth replacing with
dispatch, provided its ordering, state and failure contracts survive the change.

The strongest replacement is the repeated application-to-UI view feed. The main
new observations are native focus, gamepad availability, audio completion,
committed resource changes, upload outcomes, runtime session lifecycle and
runtime pressure state. Platform viewport application is a viable internal
conversion with a smaller benefit. Its timing differs from the simulation view
and needs separate treatment.

The [recommendation inventory](recommendations.csv) contains **ten publication
decisions**: one recommended integration replacement, seven additions, one existing
publication to retain and one conditional internal replacement. It includes only
publication candidates. Existing mechanisms that should remain are assessed
below without turning every operation into an event recommendation.

This audit supersedes the selection and integration recommendations in the
[initial report](report.md). That report and its [65-row inventory](inventory.csv)
retain their original evidence and baseline. No engine, module, application,
published documentation or test implementation changes accompany this audit.

## Selection and categories

For each candidate, identify what changed, the producer and interested consumers,
whether dispatch should replace or supplement existing communication, and the
consequences of that choice. A useful internal consumer is sufficient. A credible
extension use is also relevant even when that consumer is not implemented today;
the scenarios below describe intended uses rather than existing subscriptions.

| Category | Responsibility | Documentation decision |
| --- | --- | --- |
| System | Runtime state and coordination between engine services and modules. | Document contracts required by interchangeable modules. Coordination confined to engine implementation can remain internal. |
| Device | Native window focus and input-device availability. | Document the portable observation, source identity and sampling limits for modules and applications. |
| Resource | Changes to shared resource contents and loading outcomes. | Document commit, retained-generation and partial-failure semantics. |
| Audio | Playback lifecycle observations. | Document playback identity, observation latency and terminal-state distinctions. |

These categories organize meaning. They do not require separate buses, prescribe
delivery or automatically determine API visibility. In particular, a System event
used by TGUI, RmlUi or another UI module is a public module contract. An internal
event can remain discoverable in code without acquiring that supported contract.

## Publication decisions

Event labels here describe proposed semantics; new channel names and payload APIs
remain design work.

| ID | Event | Category | Decision | Priority |
| --- | --- | --- | --- | --- |
| E01 | Runtime view committed | System | Replace repeated UI view integration | P1 |
| E02 | Native framebuffer resized | System | Retain existing publication and compatibility | Retain |
| E03 | Platform framebuffer snapshot changed | System | Conditional internal replacement for viewport integration | P3 |
| E04 | Native window focus changed | Device | Add observation | P1 |
| E05 | Convenience gamepad availability changed | Device | Add observation | P1 |
| E06 | Playback naturally finished | Audio | Add observation | P2 |
| E07 | Asset cache contents changed | Resource | Add observation after committed changes | P2 |
| E08 | Asset upload attempt settled | Resource | Add aggregate outcome observation | P2 |
| E09 | Runtime session lifecycle | System | Add observation alongside control flow | P2 |
| E10 | Runtime pressure state changed | System | Add coalesced internal observation | P2 |

P1 identifies the first focused work; P2 identifies useful subsequent units.
P3 is conditional on choosing the internal integration refactor after weighing
its smaller benefit. An addition does not imply that the current subsystem fails
to handle the transition internally.

## Delivery choices and conversion costs

The eligible modes are **immediate, platform and simulation delivery**. Worker
delivery is excluded from every recommendation in this audit. A producer running
on a background thread does not require worker delivery for its consumers.

| Mode | Appropriate use | Consequence when replacing a call |
| --- | --- | --- |
| Immediate | The publication owner is already the required execution owner; required view application before update is the main example. | The caller waits for listeners. Reentrancy, registration order and propagated exceptions become part of the integration. Concurrent dispatch callers can overlap. |
| Platform | A notification must reach platform-owned policy or native work from another owner. | Delivery is deferred even when dispatch starts on platform. Queue progress, cancellation and the deadline before native use must be established. |
| Simulation | Platform or service observations must reach simulation-owned state. | Delivery occurs at a later update boundary. FIFO within a listener does not establish agreement with the input or view selected for that update. |

Delivery is selected per subscription. Queued modes require an independent,
nonthrowing error sink. Both dispatcher drains detach one batch; reentrant posts
wait for another drain. Merely replacing a call with a queued listener can move
the effect beyond the operation that depended on it.
[delivery adapters](../../projects/engine/src/core/engine/event-delivery.cpp#L89),
[platform drain](../../projects/engine/src/core/engine/platform-dispatcher.cpp#L65),
[simulation drain](../../projects/engine/src/core/engine/simulation-dispatcher.cpp#L77).

The bus provides no required-listener guarantee, automatic replay or owning
subscription lifetime. Immediate listener failure stops that dispatch, while
queued listener failure reaches its error sink. Those behaviors need explicit
treatment when a successful direct call becomes a required event-driven effect.
[dispatch loop](../../projects/engine/src/core/subsystems/event-bus.cpp#L66),
[delivery and failures](../../projects/engine/src/core/subsystems/event-bus.cpp#L110),
[registration and removal](../../projects/engine/include/cheryl/core/subsystems/event-bus.h#L128).

### E01 Runtime view committed

**Recommend replacement. Public module contract. Immediate delivery on simulation.**

The tutorial compares dimensions and calls `set_view`; both demos feed the view
manually. Context-based session constructors currently import input and fonts,
without acquiring a runtime view. Replacing that repeated integration with a
documented notification would support additional UI modules on the same terms.
[tutorial caller](../../projects/apps/tutorial-game/src/ui-controller.h#L38),
[TGUI demo](../../projects/apps/demo/src/tgui-demo.cpp#L223),
[RmlUi demo](../../projects/apps/demo/src/rmlui-demo.cpp#L158),
[TGUI constructor](../../projects/modules/ui/tgui/src/session.cpp#L235),
[RmlUi constructor](../../projects/modules/ui/rmlui/src/session.cpp#L284).

The producer should be the runtime committing the copied logical/framebuffer
pair for an update. It should publish changed committed state on simulation,
apply required UI listeners immediately, then call game update with that same
pair. Logical-only and framebuffer-only changes belong to this one contract.
[sequential sampling and update](../../projects/engine/src/core/game-framework/game-runtime.cpp#L296),
[concurrent update snapshot](../../projects/engine/src/core/game-framework/game-runtime.cpp#L492).

The conversion requires:

- A source-scoped current view and subscription path reachable through the
  context. Matching names on a different bus do not connect producer and session.
  Such a view service is an API addition; it is not supplied by today's context.
- Copied dimensions, source/session identity and revision; no native/window
  dereference by simulation listeners. Coalesce intermediate observations when
  only the selected update view matters, preserving zero extents.
- Initial seeding serialized with subscription. Sessions created inside the
  first or a later update must receive the current view before layout/input.
  Registration alone cannot supply that state.
- Subscription ownership tied to session lifetime. Preserve a caller-managed
  mode for standalone or intentionally overridden views, and remove repeated
  application feeds only after the automatic mode is established.
- An owner rule: concurrent `game.init()` runs on the caller, and sessions keep
  their creating owner. Automatic simulation integration cannot silently move
  a session created there to simulation.
- A required-listener failure policy. The smallest consistent behavior is to
  abort the update if required view application fails. Continuing after a failed
  delivery could give the UI and game different views. Optional observers need
  an explicitly contained failure path.

Today's simulation mailbox drains before the concurrent snapshot is selected.
A queued native resize can therefore describe a different view from the upcoming
tick. The committed-view publication must occur at the selected-snapshot boundary;
adding `simulation_delivery` to the existing resize subscription is insufficient.
The sizes are already available to the runtime; this audit recommends a change
in communication and ownership, not a new native size query.
[concurrent initialization](../../projects/engine/src/core/game-framework/game-runtime.cpp#L454),
[session owner check](../../projects/modules/ui/tgui/src/session.cpp#L240),
[tick view contract](../../projects/engine/include/cheryl/core/game-framework/tick-context.h#L14).

### E02 Native framebuffer resized

**Retain existing publication. Existing public contract. Immediate on platform;
platform or simulation delivery remains suitable for asynchronous observers.**

GLFW publishes the legacy named notification followed by the typed notification.
The payload contains pixel size and a borrowed window pointer. Preserve that
compatibility while introducing E01. Native observation and the committed
simulation view have different meanings; E01 does not require another channel
for every dimension that changes.
[producer](../../projects/modules/platform/native-glfw/src/core/display/window.cpp#L153),
[payload](../../projects/engine/include/cheryl/core/display/window-interface.h#L30).

Nested legacy listeners can yield typed observations in inner-then-outer order,
and a throwing legacy listener can prevent typed dispatch. Existing tests encode
both cases. Consequently, this stream cannot become authoritative simulation
state merely by applying every callback. Queuing its payload also preserves the
borrowed pointer limitation. Current raw listeners retain their contract; new UI
integration uses E01's copied, committed state.
[native resize expectations](../../projects/modules/graphics/opengl/tests/acceptance/src/native-opengl.cpp#L2231),
[legacy failure expectations](../../projects/modules/graphics/opengl/tests/acceptance/src/native-opengl.cpp#L2313).

### E03 Platform framebuffer snapshot changed

**Conditional internal replacement. Immediate delivery on the platform owner.**

Both runtime modes sample the framebuffer, compare it with a retained viewport
and call `renderer.set_viewport`. Dispatching a copied platform snapshot to a
runtime-owned renderer listener could centralize this integration and allow other
platform policies to observe it. This is a legitimate engine-to-engine use even
without a public application use case. The listener can remain internal while
renderer modules continue implementing the existing `set_viewport` interface.
[sequential viewport](../../projects/engine/src/core/game-framework/game-runtime.cpp#L270),
[concurrent viewport](../../projects/engine/src/core/game-framework/game-runtime.cpp#L622),
[OpenGL viewport](../../projects/modules/graphics/opengl/src/backends/opengl/renderer.cpp#L216).

Its benefit is smaller than E01's because the runtime already owns the required
renderer call. Recommend retaining that call in the first unit and considering
this refactor only as a coherent platform-view integration. A replacement must
install its required listener, seed it after renderer initialization, invoke it
before the next render, propagate required application failures and detach before
renderer teardown. Keep resize accounting consistent. A logical-only change
should not cause an unnecessary viewport call.

This event would publish at a safe platform observation phase, outside a native
callback. Queued platform delivery would need an additional guaranteed drain
before render; immediate delivery avoids that change. E01 cannot drive the
viewport because rendering can proceed without a simulation update. If another
module must subscribe directly to E03, document that integration contract.

### E04 Native window focus changed

**Add observation. Public device contract. Produce after completed platform
observation; immediate platform, queued platform or simulation delivery by consumer.**

The input adapter detects `GLFW_FOCUSED` changes and logs them. Pause policy,
audio policy and UI modules can use a portable focus notification even though
native input continues to be processed internally. This event describes native
window focus; requested keyboard routing and poll-latched effective routing retain
their separate meanings.
[focus observation](../../projects/modules/platform/native-glfw/src/core/controls/input-system.cpp#L374),
[input focus latch](../../projects/engine/src/core/controls/input-interface.cpp#L6).

Stage a changed observation and publish after the relevant poll has completed,
with copied source identity, state and poll/revision metadata. Seed current state
for late subscribers. Sampling cannot promise every transient native transition.
A simulation-delivered observation is asynchronous relative to poll consumption;
an input-dependent policy needs an explicit consumption boundary. Automatic blur
must respect already captured records rather than retroactively changing their
routing or destroying the session's old focus epoch before those records drain.
[TGUI drain then preemption](../../projects/modules/ui/tgui/src/session.cpp#L368),
[RmlUi drain then preemption](../../projects/modules/ui/rmlui/src/session.cpp#L385).

### E05 Convenience gamepad availability changed

**Add observation. Public device contract. Produce after completed platform poll;
immediate platform, queued platform or simulation delivery by consumer.**

The adapter detects availability and reconciles the pad's controls. That keeps
actions correct after disconnect; it does not notify a reconnect screen,
controller-assignment policy or another module. These uses justify publication
without duplicating button/axis input records.
[availability and neutralization](../../projects/modules/platform/native-glfw/src/core/controls/input-system.cpp#L398),
[backend snapshot](../../projects/modules/platform/native-glfw/src/core/controls/input-system.cpp#L257),
[portable input surface](../../projects/engine/include/cheryl/core/controls/input-interface.h#L24).

Publish the staged transition after `publish_input()` succeeds, with portable
source/device identity, availability, observation poll and attachment generation.
Keep release/neutral-axis reconciliation in the input implementation. A neutral
action snapshot cannot distinguish an idle device from an absent one.

The generic service needs a current availability snapshot as well as the event.
The current device ID identifies one convenience pad; it does not identify every
physical controller or promise full hotplug enumeration. Define initial presence,
reattachment and source teardown. Preserve sampled disconnect/reconnect order
when consumers need both transitions.

For observation, queued simulation delivery is suitable. For policy that must
apply with a particular input batch, carry the transition with that completed poll
and notify at its simulation consumption boundary, or define an equivalent ordered
handoff. The ordinary event queue does not supply that guarantee. Do not publish
the same event twice on one bus at both boundaries without distinct semantics.
[completed poll](../../projects/engine/src/core/controls/input-interface.cpp#L27),
[poll identity](../../projects/engine/include/cheryl/core/controls/action-snapshot.h#L73),
[ordered consumption](../../projects/engine/src/core/controls/input-accumulator.cpp#L6).

### E06 Playback naturally finished

**Add observation. Public audio contract. Immediate on a designated maintenance
owner, with platform or simulation delivery for consumers on another owner.**

Natural completion is detected when voice state is sampled. Internal reclamation
does not advance a playlist, dialogue sequence or another module's state. A
completion event can remove per-voice application monitoring while retaining
voice controls and snapshots.
[completion state](../../projects/modules/audio/miniaudio/src/system.cpp#L52),
[voice interface](../../projects/engine/include/cheryl/audio/system.h#L35).

Existing maintenance checks unreferenced voices for reclamation; it is not a
completion sweep for all retained voices. Introduce completion detection and
deduplication for the relevant playbacks, then emit owned records after releasing
the audio mutex. A completion handler that starts the next sound must not reenter
that mutex from inside `maintain_locked`. The audio system is application-owned
and has no runtime maintenance pump; designate the observation owner and cadence.
[maintenance predicate](../../projects/modules/audio/miniaudio/src/system.cpp#L80),
[maintenance lock](../../projects/modules/audio/miniaudio/src/system.cpp#L376).

Include system/voice identity and playback generation. Restart replaces the
underlying voice data, so stale completion must not finish a new playback. Keep
natural finish distinct from explicit stop, pause and system close. The API must
state observation latency and whether close can discard an unobserved completion.
If a native handoff is later needed for tighter latency, it must be bounded;
general dispatch and arbitrary listeners do not belong in the mixing callback.
[restart](../../projects/modules/audio/miniaudio/src/system.cpp#L221).

### E07 Asset cache contents changed

**Add observation. Public resource contract. Publish on the mutation owner after
commit and unlock; immediate, platform or simulation delivery by consumer.**

Publication, replacement and explicit invalidation change the resource selected
by a cache key. Resource modules, an asset browser or a reload coordinator can
react even while existing handles deliberately retain their old generation.
Successful shader replacement belongs to this family; it does not need a second
success event merely because another API initiated it.
[cache publication and replacement](../../projects/engine/include/cheryl/templates/asset-mgr.h#L160),
[shader reload commit](../../projects/engine/src/core/resources/asset-management/shader-mgr.cpp#L18).

Keep construction, metadata commit, handle return and rollback behavior direct.
The internal publication callback is constrained to nonthrowing metadata work
under the cache write lock; replace neither that callback nor its atomicity with
arbitrary bus listeners. Collect copied cache/provider/key/generation information
at commit and publish after unlocking. Serialize notification on its owner when
concurrent mutations could otherwise dispatch revisions out of order.

Notification failure must not turn a successful replacement into an apparent
resource-load failure. In particular, putting a throwing dispatch inside the
shader reload catch region could report `preserved_previous` after replacement
already happened. Contain observation errors separately from resource admission.
Notifications enable explicit adoption/rebinding policy; they do not rewrite
retained frames or invalidate old handles.

Explicit cache reset needs a defined observation path, but `clear_assets()` is
`noexcept` and is also called during destruction. Do not insert ordinary throwing
dispatch into that destructor path. Specify explicit reset/source closure and
resynchronization after notification loss before promising complete cache tracking.
[clear and destructor](../../projects/engine/include/cheryl/templates/asset-mgr.h#L121).

### E08 Asset upload attempt settled

**Add aggregate observation. Public resource contract. Publish on the upload owner
after outcome recording; immediate, platform or simulation delivery by consumer.**

An upload can finish, partially publish and fail, or fail while publication counts
are unavailable. The caller's return/exception remains authoritative for its
request, while independent loading UI and coordination modules benefit from one
terminal batch notification. Existing last-attempt statistics can be overwritten
by the next upload; they are not a subscription.
[upload and outcome recording](../../projects/engine/src/core/resources/asset-management/asset-loader.cpp#L230),
[statistics contract](../../projects/engine/include/cheryl/core/resources/asset-management/asset-loader.h#L42).

Include loader/batch/provider identity, outcome, completed counts, count validity
and owned failure information. Counts describe selected typed-cache observations,
not an atomic snapshot of all resource state. Registry merging can also have
effects, so zero counted cache publications does not establish no side effects.
Keep cache-change E07 distinct from aggregate attempt completion E08.

The current `finish` function is `noexcept`; adding an ordinary dispatch there
could terminate during failure handling. Emit through a contained observation
path after releasing the statistics lock and preserve the original return or
exception. CPU preparation still returns its owned batch directly. Preparation
success, upload completion and resource adoption are different boundaries; no
additional event is recommended merely for every stage or individual provider call.
[finish boundary](../../projects/engine/src/core/resources/asset-management/asset-loader.cpp#L256),
[registry and metadata commits](../../projects/engine/src/core/resources/asset-management/asset-loader.cpp#L302).

### E09 Runtime session lifecycle

**Add observation alongside direct control. Public module contract. Immediate on
the runtime control owner for admission and terminal outcome.**

Module supervision, embedded hosts and tools can observe session start and final
outcome without owning the game's hooks. The initial family should describe run
admission and terminal outcome. Finer readiness/quiescence milestones need exact
commit points; diagnostic `phase_` strings are not already a public state model.
[run admission and outcome](../../projects/engine/src/core/game-framework/game-runtime.cpp#L54),
[pre-start stop path](../../projects/engine/src/core/game-framework/game-runtime.cpp#L764).

Keep `init`, `update`, `quiesce`, `deinit`, stop tokens and cleanup ordering as the
required control path. Replacing them with optional listeners would introduce
missing-listener behavior, registration-order dependencies and different failure
handling. Observation can supplement them without making every internal phase
a separate event. Distinguish admission, successful readiness, stop request and
completed teardown, including partial initialization and failure.

Terminal delivery cannot depend on the runtime's closed simulation or platform
queues. Use immediate observation on the control owner, or a separately live
host-owned endpoint. Platform/simulation delivery is appropriate only for earlier
milestones while the target remains serviced. Do not use these observations as
substitutes for required shutdown work or barriers. Preserve the primary runtime
failure even if notification fails, and specify what a throwing optional listener
does to the remainder of that notification.
[sequential teardown](../../projects/engine/src/core/game-framework/game-runtime.cpp#L364),
[concurrent teardown](../../projects/engine/src/core/game-framework/game-runtime.cpp#L705).

### E10 Runtime pressure state changed

**Add coalesced observation. Internal System event initially. Immediate on the
state's owner, with platform or simulation delivery for another owner.**

The runtime already retains lag-warning and input-backpressure-warning states,
and reports degradation and recovery. These supply a bounded transition with
useful meaning for runtime policy coordination and monitoring. Internal consumers
are sufficient; a documented public health API is not a prerequisite for this
publication. If separately developed modules depend on it, document its contract.
[timing state](../../projects/engine/src/core/game-framework/game-runtime.cpp#L149),
[platform pressure state](../../projects/engine/src/core/game-framework/game-runtime.cpp#L181).

Retain the current reporting cadence and thresholds initially, emitting on state
entry/recovery rather than each repeated warning or counter change. Include
runtime/session identity, pressure kind, observed state, sample time and relevant
counts. Timing state belongs to the simulation timing owner; input-backpressure
state belongs to platform. Cross-owner policy effects must be delivered to their
owner and apply at a defined future decision boundary.

These reporting functions are `noexcept`. Contain event preparation and observer
failures, retain counters/logging, and avoid recursive failure reporting. State
which transitions can be coalesced, provide initial state if subscribers can join
late, and use lifecycle closure rather than inventing recovery when a session
ends under pressure. This is a reporting-state notification; it does not promise
lossless incident history or replace the scheduler's tick timing data.

## Integrations to keep and candidates to defer

These decisions describe why a general dispatch replacement is not recommended
now. They do not establish that internal handling forbids future publication.

| Existing mechanism | Decision and conversion consequence |
| --- | --- |
| Ordered input, action snapshots, capture and routing leases | Retain. Their sequence, poll identity, target epoch and gameplay gating already form the communication contract. A bus replacement must preserve that entire protocol; broadcasting individual records adds no established integration benefit. |
| UI input processing and preemption checks | Retain. The demos interleave visibility/focus decisions with individual records, and sessions blur after draining the old routed batch. An applied-focus event becomes useful if a shared coordinator needs it, but replacing the current checks requires a defined after-drain boundary. I06 is not an established missing generic event. |
| UI time updates and game update/render hooks | Retain their chosen control order. A module phase event is possible, but a borrowed `TickContext::input` cannot safely be copied into arbitrary deferred work. Recovery updates must not replay input, and automatic module updates must not duplicate caller updates. No separate per-tick broadcast is recommended here. |
| Futures, upload/adoption results and text revision ledgers | Retain authoritative result transfer and adoption. Ready work and currently adopted state differ. E07/E08 add useful shared observations without announcing superseded work as current or replacing exception/result ownership. |
| Native window mode requests and close handling | Retain control. A mode-change observation could serve shared settings/UI consumers, and E01 covers extents rather than mode. First define committed-mode observation when a native transition partly fails; a successful request must not be confused with a complete native state transition. Close confirmation/veto also requires a decision protocol and a delivery window before stop. |
| Renderer maintenance, GPU retirement, per-frame outcomes and camera revisions | Retain ownership and local tracking. Maintenance must run even without a frame or update; dispatch cannot replace collection barriers. Keep ordinary observation sampled rather than adding a generic broadcast for each resource release or frame. E03 separately assesses viewport integration. |
| Startup factories, worker completion/shutdown, local observers/state-machine hooks and logging callbacks | Retain their dedicated contracts. Replacing return values, predicates or required ordered cleanup with observers changes control semantics. An owning application can publish a meaningful domain transition after those operations. |
| Font configuration and file-registry insertion | Retain current configuration/merge operations. Fonts are configured before consumers and existing handles/sessions retain snapshots. A live catalogue event needs a deliberate live-use/rebinding contract. File-registry merges may partially succeed; they do not establish filesystem watching or resource replacement. |
| Native diagnostics, worker-policy fallback and logging losses | Keep diagnostic collection/reporting. E10 uses the runtime's existing degradation/recovery states. Additional health transitions need their own state, sampling and recovery contract; a speculative diagnostics subscriber alone does not justify converting every counter/log into an event. |

Supporting paths: [input latching](../../projects/engine/src/core/controls/input-interface.cpp#L6),
[per-record application policy](../../projects/apps/demo/src/tgui-demo.cpp#L178),
[text adoption](../../projects/engine/src/text/text-updater.cpp#L107),
[font configuration contract](../../projects/engine/include/cheryl/text/font-service.h#L34),
[registry partial insertion contract](../../projects/engine/include/cheryl/core/resources/asset-management/file-registry.h#L33),
[worker outcome ownership](../../projects/engine/src/core/engine/worker-pool.cpp#L160),
[renderer maintenance](../../projects/modules/graphics/opengl/src/backends/opengl/renderer.cpp#L205),
[local observer contract](../../projects/engine/include/cheryl/templates/observed-variables.h#L12).

Monitor hotplug, per-window scale, full controller inventory, audio device
loss/recovery and later stream failure can deserve events when their observation
capabilities and state models exist. They remain capability work, rather than
missing dispatch calls. The current monitor list is constructed once; the generic
input API has one convenience pad; audio has no later-failure state. Native file
drops, file watching, CPU-topology refresh, audio markers and the SDL scaffold
likewise require capability decisions before a publication contract can be audited.
[monitor snapshot](../../projects/modules/platform/native-glfw/src/core/display/display-system.cpp#L69),
[generic input capabilities](../../projects/engine/include/cheryl/core/controls/input-interface.h#L24),
[audio state model](../../projects/engine/include/cheryl/audio/system.h#L12).

## Shared requirements before conversion

1. **Scope the source and bus.** Connect each integration to the intended producer
   domain. A process-default channel name alone does not identify a runtime,
   device attachment or resource provider. Decide which endpoints are public.
2. **Define observable commit and order.** Publish copied committed state, with
   source and revision/generation as appropriate. Define coalescing and whether
   every observed transition must survive. Serialize publication after unlocking
   when concurrent producers could otherwise reorder committed revisions.
3. **Provide initial state where needed.** View, focus, availability and resource
   tracking need a snapshot/subscribe contract; registration does not replay.
4. **Preserve ownership and shutdown.** Registrations persist after their IDs are
   dropped. Detach before destroying captures, settle entered callbacks and keep
   queued error-sink captures valid independently. Avoid self-waits and dispatch
   from allocator, backend-lock or destructor failure paths.
5. **Separate required effects from optional observation failures.** Required UI
   application needs a failure policy that prevents an incoherent update. Resource
   and lifecycle observations must not falsify a completed operation or mask its
   primary failure. Define notification loss and resynchronization.
6. **Measure the conversion cost.** Bus dispatch copies listener snapshots, typed
   publication owns a payload, and queued delivery adds per-listener ownership and
   queue work. These costs replace a direct call's simpler path. Use state-change
   publication and coalescing where appropriate; do not broadcast unchanged state
   merely because an update occurred.

## Acceptance required for implementation

No native tests were run for this audit. The cases below are proposed acceptance,
supported by existing test source rather than executed results.

| Change | Acceptance that distinguishes a correct conversion |
| --- | --- |
| E01 and E02 | Initial state without a resize; subscription during first/later update; logical-only and pixel-only changes; zero extents and restoration; reentrant/coalesced observations; view equals the matching tick in both modes; manual mode and late-session owner rules; listener failure and destruction; legacy resize compatibility. |
| E03 if selected | Listener installed before required application; initialized renderer seeded; viewport updated before render even without simulation updates; no logical-only viewport call; failure propagation, resize accounting and teardown. |
| E04 and E05 | Initial presence/focus; repeated state suppression; sampled disconnect/reconnect; neutralized controls; correct source/generation; poll ordering under backlog/recovery; late listeners; no retroactive routing or duplicate notifications from two publication boundaries. |
| E06 | Once per playback generation including retained voices; next-playback handler outside the mutex; restart races; loop/stop/pause/close distinctions; observation cadence and close policy. |
| E07 and E08 | Notifications only for committed changes; observer reentry after unlock; old handles survive; partial/unknown batch outcomes; counts do not imply atomic registry state; notification failure cannot make successful replacement look failed; explicit reset and loss recovery. |
| E09 | Admission versus readiness; stop-before-start, initialization/thread-start failure and normal exit; ordered terminal outcome; no reliance on closed queues; observer failures preserve the primary runtime error and cleanup. |
| E10 | Warning-state entry/recovery once per transition; repeated reports do not repeat events; current cadence/thresholds preserved; producer and delivery owners; state under shutdown; no exception escapes the reporting boundary. |

Relevant existing tests include
[typed event ownership and failure](../../projects/engine/tests/unit/src/typed-events.cpp#L39),
[owner delivery](../../projects/engine/tests/all-tests/src/core/runtime-adapter.cpp#L1923),
[routed input order](../../projects/engine/tests/all-tests/src/core/runtime-adapter.cpp#L1244),
[native resize compatibility](../../projects/modules/graphics/opengl/tests/acceptance/src/native-opengl.cpp#L2231),
[audio completion and looping](../../projects/modules/audio/miniaudio/tests/unit/src/audio.cpp#L144),
[cache lifetime and reentry](../../projects/engine/tests/all-tests/src/core/runtime-adapter.cpp#L1457),
[partial upload](../../projects/engine/tests/all-tests/src/core/runtime-adapter.cpp#L1643),
[quiescence error precedence](../../projects/engine/tests/all-tests/src/core/runtime-adapter.cpp#L2384).

Start with E01's current-view source, immediate simulation publication and both UI
bindings. E04/E05 are separate bounded producer units. E06 requires an audio
observation owner; E07/E08 require commit-safe observation and failure handling;
E09 requires named lifecycle milestones and terminal-delivery rules; E10 needs
transition deduplication and contained reporting failures. E03 remains a separate
optional integration refactor.

## Evidence and verification scope

All 65 entries in the original inventory were reconsidered under the revised
publication criteria. A fresh pattern screen covered **397 tracked production C++
files** under `projects/engine` and `projects/modules`, excluding `tests` and
`examples` path components. The suffixes were `.h`, `.hpp`, `.inl`, `.ipp`, `.tpp`,
`.cpp`, `.cc` and `.cxx`. Event transport, callbacks, lifecycle, state/publication,
completion and native/diagnostic observations supplied overlapping screens.
Responsible producers, interfaces and integration callers were traced for the
decisions above. This is a source-based architecture audit, not a semantic review
of every line in every screened file.

The concrete transport screen still finds one production publication family,
native framebuffer resize, bridged to named and typed channels. No concrete
engine/module subscriber was found outside event infrastructure. Application
callers supplied evidence of manual UI integration; tests supplied contract and
regression scenarios. Future subsystem designs and third-party internals were
outside this source audit.

The original engine/module baseline differs from this revision only in UI header
comments. Source behavior used here is consistent with the recorded revision;
the new selection and conversion analysis are the material change. Artifact
verification covers CSV structure/decisions, references and source line targets,
baseline consistency and whitespace. No builds, native execution, full site
generation, downloads, devices or external services were used.
