# Engine and module event integration

Replace repeated application forwarding with documented engine/module event
contracts, and publish useful committed state and outcomes from their actual
owners. Begin with coherent window observation, renderer viewport integration and
automatic UI sizing. Extend the established pattern to runtime lifecycle,
shutdown negotiation, module service phases and independent service observations.

**Planning baseline:** `ccf514018faa9a5c5f0750c21c0da321c0f82e96`, 2026-10-18.
Implementation has not started. This plan owns execution order, development units
and acceptance checkpoints. The [design assessment](../../../reports/event-coverage/design.md)
owns the architectural rationale and proposed semantics; its
[event catalogue](../../../reports/event-coverage/event-catalogue.csv) supplies the
stable event IDs used below. Current bus contracts remain in the
[event-delivery guide](../../site/runtime/event-delivery.md).

## Selected scope and compatibility

The implementation scope is the catalogue's **18 new contract families** grounded
in current engine/module responsibilities. Preserve existing `WindowResized`
(COMP01). The four capability-dependent families remain future work; this plan
does not promote monitor hotplug, HID, platform lifecycle or output-device recovery
into the initial implementation scope.

Publish through typed channels and a Context-owned notification endpoint, with
explicit endpoint injection for independently usable services. Categories organize
meaning; they do not introduce separate buses. Cross-module contracts require
public documentation. Runtime pressure starts as an internal, discoverable
observation. No selected publication uses worker delivery or changes the existing
worker delivery adapter.

Keep ordinary `dispatch` behavior compatible. Add isolated notification rather
than changing fail-fast dispatch globally. Required integration uses immediate
delivery on its actual owner. Queued platform/simulation observation never proves
that a required operation completed. Keep direct joins, queue closure, accepted-work
settlement, final native destruction, game hooks and request-specific results.

Existing UI constructors remain **manual by default**, including Context
constructors. Add an explicit automatic view option/owned attachment and migrate
the tutorial/demo to it. Automatic attachment requires an initialized view source
and the correct simulation owner; failure rolls back attachment rather than
silently choosing a mode based on startup timing. Input/fonts-only construction
remains independent of Runtime. Public `set_view` validates its arguments, then
explicitly detaches automatic sizing before applying the manual view; automatic
callbacks use a private apply operation. Automatic UI time advancement is a
separate later opt-in.

Native focus is optional. Existing portable window adapters still provide sizes;
without the additional capability, focus is unknown/unsupported. Do not add a
pure virtual focus requirement to every fake/standalone adapter, or confuse
`Input::System::supports_focus()` keyboard routing with native window focus.
Default-unsupported virtual extensions preserve source compatibility but change
ABI; rebuild the affected libraries and consumers together.

Initial native observation remains **sampled and poll-gated**. Full input backlog
can delay GLFW close/focus/resize observation. Neither maintenance events nor
shutdown negotiation implicitly splits native pumping from input publication.
Host shutdown commands remain independent of input capacity.

## Dependency order

The first integrated result is one active macro task with small sequential
checkpoints. Later rows are roadmap units, activated individually. The recommended
runtime order avoids overlapping edits to the central run/teardown loops; service
lanes need only the common facilities they actually use.

| Unit | Event IDs | Prerequisite and independently reviewable result |
| --- | --- | --- |
| V — window and simulation view | DEV01, SYS07; retain COMP01 | Establish thin notification/attachment facilities, then replace viewport and application UI sizing forwarding. Detailed active work below. |
| L1 — lifecycle observation | SYS01, SYS04, SYS05, SYS06 | V's owner/lifetime pattern and reliability gate; add milestones, concurrent Ready gate and terminal finalizer around existing stop/close behavior. |
| L2 — negotiable shutdown | SYS02, SYS03, DEV02 | L1 lifecycle authority/finalizer; add one-policy controller, consumable native close and required main-window bridge. Reuse SYS04 for committed shutdown. |
| M1 — platform maintenance | SYS09 | Required-binding reliability; schedule after L1 so teardown boundaries are stable. Replace recurring renderer forwarding while preserving final collection. |
| M2 — simulation service step | SYS08 | V's selected view/owner and required bindings; schedule after L1. Add opt-in UI time advancement. Independent of M1 and L2. |
| D1 — sampled gamepad availability | DEV03 | Stable identity/notification endpoint and completed-poll staging; observe the existing convenience slot. |
| O1 — runtime pressure | SYS10 | Stable notification endpoint; stage simulation timing state for platform publication. No shutdown-controller dependency. |
| R1 — asset catalogue | RES01 | Thin notification endpoint, checked identity, owned registrations and ordered catalogue commits. |
| R2 — executed upload outcome | RES02 | Thin notification endpoint and attempt/effects bookkeeping. Does not require R1 or a registry transaction framework. |
| R3 — font catalogue | RES03 | Immutable snapshot, synchronized watch/seed and designated publication owner. No UI rebinding. |
| A — standalone playback outcomes | AUD01 | Thin notification endpoint plus checked voice generations, terminal snapshots and a bounded audio outbox. No Runtime dependency. |
| G — graphics domain lifecycle | GFX01 | Thin notification endpoint and a portable retained domain token. No physical device-loss/recovery dependency. |
| I1 — optional runtime audio pump | No new family | A + M1; explicitly attach pumping/final publication to runtime maintenance. AUD01 is complete without this integration. |
| I2 — asset domain provenance | Extend RES01 | R1 + G; carry native-domain provenance separately from catalogue identity where supported. |

R1/R2/R3/A/G can be developed independently once the thin common API is stable;
they do not depend on shutdown negotiation, UI clocks or the required runtime
binding machinery. Share immutable state/watch mechanics only where they fit.
Audio needs bounded transition storage; graphics needs two lifecycle transitions;
upload needs an owned attempt outcome. Do not make them all adopt a universal
history/replay framework.

## Active macro task V

Deliver DEV01 and SYS07 end to end, keeping COMP01 behavior and existing manual
UI use. Small supporting commits may be reviewed separately; the macro is complete
only when producer, required renderer consumer, simulation projection and both UI
consumers work together. Do not remove a direct integration before its replacement
and failure protection exist.

- [ ] V1: Add isolated typed notification with independent error reporting; retain existing dispatch semantics.
- [ ] V2: Establish Context notification lifetime, checked identities and the two view sources with ordered watch/seed.
- [ ] V3: Establish required Seed/Change binding reliability and acknowledged attachment lifetime.
- [ ] V4: Implement coherent window observation and migrate renderer viewport seed/changes to its required binding.
- [ ] V5: Project one selected snapshot into simulation and TickContext on both runtime paths.
- [ ] V6: Add explicit automatic view attachments to TGUI/RmlUi and migrate tutorial/demo sizing.
- [ ] V7: Complete focused controlled/runtime/UI and public-consumer acceptance; establish native claims separately.

### V1 and V2 notification and source ownership

Extend the [bus owner](../../../projects/engine/include/cheryl/core/subsystems/event-bus.h)
and [implementation](../../../projects/engine/src/core/subsystems/event-bus.cpp)
with a distinct isolated typed notification operation. Attempt subsequent valid
listeners after an optional immediate listener throws. Keep existing queued error
sinks, invalidation and old dispatch behavior. Notification errors must not change
a committed mutation's result. Reporting must not recursively publish through the
same bus.

Extend [Context](../../../projects/engine/include/cheryl/core/engine/engine-context.h)
and its [implementation](../../../projects/engine/src/core/engine/engine-context.cpp)
to own the endpoint, the authoritative selected-window publication source and
SimulationView source, and their lifecycle. Window/GLFW supply native observations,
not a competing independently revised current source. Do not add native SDK or
UI dependencies to Engine. Allocate checked context/source/session identity at
session reservation, before adapter initialization; do not wait for RuntimeReady.
Prepare identity before committing admission so an allocation/exhaustion failure
does not leave a half-admitted session.
A checked wrapper may reuse the existing identity allocator while rejecting its
invalid/exhausted result. Keep diagnostic fields and semantic identity distinct.

Implement only the WindowState and SimulationView source/watch support needed
here: copied immutable state, monotonic revision, nonrecursive bounded fanout and
owner-serialized register/seed. A seed is current state, distinguishable from a
change; queued consumers reject older/equal revisions. Attachments protect callback
targets and independent error sinks until invalidation is acknowledged. No callbacks
from destructors, generic replay, source catalogue or category broker.

**Checkpoint:** throwing optional listeners do not suppress later listeners;
existing fail-fast tests still hold; detach/close wins invocation entry; revision
exhaustion is explicit; late watch cannot miss a change between register and seed;
nested revisions preserve surviving per-subscription order without an endless
reentrant drain. A service closing cannot close the shared Context bus. These
contracts must be settled before downstream services extend the common API.

### V3 required binding reliability

Install required bindings transactionally with protected targets and a stable list
prepared at attachment time. The source marks an active Seed/Change operation;
wrappers validate source/session/sequence and mark **entered before effects**.
Following notification, invoke the same required operation directly only if its
entry gate was never reached. No phase-time allocation is needed to enumerate
required bindings. A forged/duplicate public publication must not run a required
engine operation.

Store operation failure on the binding. Attempt all required operations, then fail
ordinary dependent progression on the first required-operation failure. Do not
retry an entered operation after partial effects. Publication/optional-listener
failure remains an observation error if required fallback succeeds. During
shutdown, continue cleanup and preserve the established primary failure.

Apply this protection to **initial seed as well as change**: renderer initialization
and newly attached UI must not silently miss their baseline. Registration, required
entry and target protection either all install or all roll back. Self-detach must
not wait on its own invocation; cross-owner detach must acknowledge invalidation
before target destruction. Detach immediately disables future invocation and
fallback; only structural removal waits for a safe list boundary. Retain retired
nodes while an already-entered invocation finishes. New bindings join the next
detached fanout. An owner-synchronous watch still applies its protected seed before
returning: that authorization belongs only to the new binding and does not replay
or recursively enter the global change fanout.

**Checkpoint before removing any direct call:** controlled failure before listener
snapshot/delivery/entry reaches every required binding once through notification
or fallback; a partial throwing binding is never retried; optional failure cannot
skip required integration; required failure prevents dependent update/render;
initial seed and late attachment have the same guarantees.
Manual override/self-detach during fanout must prevent a later automatic entry or
fallback from overwriting the manual view.

### V4 window observation and viewport migration

Extend the [portable Window owner](../../../projects/engine/include/cheryl/core/display/window-interface.h)
and [GLFW implementation](../../../projects/modules/platform/native-glfw/src/core/display/window.cpp)
with portable sampling/dirtiness and optional native focus observation. Context's
selected-window source owns committed state and revisions. Keep logical size,
framebuffer pixels, known/supported/change masks, identity/revision and observation
time coherent. Presentation/scale/mode fields wait for truthful capabilities.

Sample after native callback completion and failure checks at a safe platform
boundary. Explicit owner mutations can also dirty observation without another
input poll. Preserve the named-then-typed raw resize producer. Never use an outer
legacy resize payload as latest state after nested mutation.

In [GameRuntime](../../../projects/engine/src/core/game-framework/game-runtime.cpp),
install/seed the required immediate renderer binding at the existing post-init
viewport boundary. Once protected, remove the old initial set_viewport and repeated
comparison calls in both modes. Apply only changed framebuffer pixels; logical-only
or focus-only changes do not set viewport. There is one viewport integration path.

If a new WindowState observer mutates the native window, finish current fanout,
mark dirty and skip native rendering until a later safe pass selects/applies fresh
state. Never recursively drain until the listener stops resizing. Native callbacks
and source locks must not contain new notification listeners.

**Checkpoint:** exact initial viewport once; pixel/logical/focus-only changes;
duplicate silence; zero extents; unsupported focus; no-frame progress; nested raw
resize converges to final live state; new-listener mutation suppresses stale
rendering; old raw ordering/exceptions remain; failed required application prevents
render; detach before renderer destruction. Keep ordinary maintenance direct here.

### V5 simulation projection and bootstrap

Replace separately evolving logical/pixel handoff fields with one owned selected
WindowState snapshot in GameRuntime. Derive both TickContext dimensions from that
selection. Initialize SimulationView on its actual owner before its first mailbox
drain/update: sequential after the baseline, concurrent immediately after owner
binding using the copied platform baseline. Publish changed projected dimensions
before the matching game hook. Focus-only source changes do not advance the view
projection revision.

This slice does **not** require a public RuntimeReady event. L1 later wraps the
already established baseline/owner point in a handshake and first-update gate.
This avoids a circular dependency between view initialization and readiness.

**Checkpoint:** both runtime modes prove event/TickContext agreement with one
logical/pixel pair; baseline watch is usable before first update; recovery steps
select correctly; cycles with no selected step produce no new view commit; late
watch seeds current state without replaying a step or querying a live native
window from simulation.

### V6 UI attachment and application migration

Extend each module's Session header/implementation with explicit automatic view
attachment using the source reachable from Context. Use immediate required sizing
on the supported simulation owner. Build session state before installing the
attachment; roll back cleanly if seed/application fails. Preserve manual defaults,
standalone input/fonts construction and custom subviews. After owner/argument
validation, `set_view` changes an automatic session to manual by acknowledged
detach; event application uses a private operation and never detaches itself.
Rejected arguments preserve automatic attachment. If native manual application
then fails, report that direct failure and remain manual; do not silently reattach
or claim rollback of partial toolkit effects.

The [tutorial controller](../../../projects/apps/tutorial-game/src/ui-controller.h)
and [TGUI](../../../projects/apps/demo/src/tgui-demo.cpp)/[RmlUi demo](../../../projects/apps/demo/src/rmlui-demo.cpp)
already defer UI creation to update. Enable their automatic sizing there, then
remove only repeated dimension caches/comparisons/set_view forwarding. Preserve
input routing, manual time advancement, HUD mutation, scene recording and adoption
order. Do not move UI construction to concurrent platform game.init or silently
migrate toolkit owner affinity.

**Checkpoint:** both toolkits receive a seed before layout/recording, late sessions
start at the current revision, manual override detaches, a no-runtime host remains
manual, owner mismatch rejects clearly, no double sizing and no borrowed callback
survives toolkit/Context teardown. Keep a manual consumer example alongside the
automatic application path.

### V7 acceptance and documentation boundary

Use the owner/fixture map below; avoid aggregate duplicates. First establish the
portable and controlled runtime behavior, then the two UI integrations and public
header/consumer boundary. Native callback/viewport claims need actual Linux native
acceptance. Do not label source completion or a controlled fake as native success.
Unavailable native coverage must remain explicit and does not prevent independent
source/service work that does not depend on that claim.

Once source contracts stabilize, synchronize the event-delivery and UI integration
guides and affected tutorial/demo instructions in a documentation maintenance
checkpoint. New declaration comments belong with their implementation. Keep the
historical audit unchanged; resulting current contracts belong in subject guides.
The macro's completion record must distinguish source work, focused executable
acceptance and native acceptance rather than using one undifferentiated checkmark.

## Later runtime units

### L1 lifecycle observation and Ready gate

Implement SYS01/SYS04/SYS05/SYS06 around existing irreversible stop/failure and
compatibility-close behavior. Add the lifecycle snapshot/envelope, bounded owned
terminal outcome and outer run finalizer. Establish concurrent bound-owner/baseline
handshake: worker reports initialized, waits, platform commits/publishes Ready,
then releases toward running or stopping even if notification fails. Immediate
Ready observers cannot synchronously wait on the gated simulation owner.

Publish Starting once for committed shutdown, including partial-startup failure;
include reached-ready/initialized-capability information. Quiescing follows
simulation/mailbox completion and an attempt of game.quiesce, before accepted CPU
work settles. Attempt module quiescing even if the application hook failed.
Stopped follows ordered cleanup attempts, with copied statistics and primary versus
secondary failure; it cannot promise every throwing cleanup succeeded.

Keep producer stop/subscription relay eligible for owned module attachment; keep
application-held native UI references and final toolkit close explicit. No module
lifecycle event replaces joins, queue closure or accepted-work barriers.

**Acceptance gate:** both modes; no false Ready on pre-start stop/partial init or
worker-start/bind failure; worker gate always released; required cleanup attempted
once; pending platform-dependent work progresses through teardown; final observer
failure cannot mask run result; rejected second run creates no second lifecycle.

### L2 negotiable shutdown and native close

Develop the one-policy controller against controlled adapters, then integrate
SYS02/SYS03/DEV02 as one complete native-close feature. Maintain Running/Pending/
Committed/Quiescing/Stopped state and lifetime-safe request/decision endpoints.
Host requests work independently of native window capabilities. Keep stop()/tick
stop unconditional. An ordinary observer holds no decision authority.

Prepare a temporary per-request policy listener before admitting Pending. Its
callback/error sink captures request ID and policy generation; stale decisions
and late delivery errors cannot affect a later request. With no installed policy,
autoaccept after Requested fanout unwinds. An installed policy keeps Pending until
an explicit decision; delivery failure/detach cancels with a typed reason.
Duplicate intent coalesces; force/failure supersedes; there is no implicit timeout.

Add consumable/resettable native intent and a runtime-owned required main-window
bridge with missed-entry fallback. Consume/acknowledge close and replace loop exit
checks with committed runtime shutdown state. Older boolean-only adapters retain
ordinary default acceptance; reject cancellation policy for unsupported native
intent rather than advertise a false capability. Host-origin negotiation remains
available even where native cancellation is unsupported.

**Acceptance gate:** default close-before-update compatibility; Pending keeps
owners serviced; cancel then fresh close ID; duplicates/stale responses/errors;
policy failure/detach; admission failure; force/stop/fault supersession; endpoint
safe after destruction; missed optional publication cannot lose the close bridge;
real native consume/reset and repeated cancel accepted separately. Do not claim
native cancellation after implementing only the controller.

### M1 platform maintenance

Implement SYS09 and install a private required renderer attachment only after
renderer readiness. Replace ordinary recurring maintenance forwarding, preserving
service progress with no frame, slow simulation, input saturation and shutdown
waits. Keep the attachment until final release, detach before backend deinit.

For FinalCollection, complete module fanout first, then invoke renderer collection
directly as the final owner barrier. The ordinary renderer binding defers that
reason; its gate cannot suppress the barrier. Preserve normal required failures
and first-failure cleanup semantics. This phase never implicitly pumps GLFW.

**Acceptance gate:** collection progresses under all existing service conditions;
later module retirement is seen by final collection; partial init and collector
failure retain cleanup progress; no duplicate collection or skipped dependency
servicing.

### M2 simulation service step

Implement SYS08 once per selected scheduled/recovery step after input/view
selection, before game.update. It describes an attempted step, not completed game
work. Add explicit automatic UI time attachment; mid-step creation starts next
step. Manual update_time rejects while automatic time is attached; explicit detach
returns to manual advancement. Recheck stop before the game hook and count updates
only on hook entry.

Migrate timing separately from sizing. TGUI currently handles input before time;
RmlUi advances time before input and may call user code. Preserve manual order by
default and make each opted-in application order change explicit.

**Acceptance gate:** fixed/recovery steps once, no-step cycles absent, no past-delta
replay, manual/automatic exclusivity, callback stop/failure prevents dependent
update, input/preemption and scene adoption remain explicitly ordered.

### D1 and O1 state observations

D1 stages convenience-gamepad availability until reconciliation and complete poll
publication succeed. Assign slot identity/reconnect epoch, seed initial presence
and retain poll correlation. Source closure is not physical disconnect; no claim
of multi-device enumeration. Preserve neutralization and ordered input records.

O1 stages simulation lag state for coherent platform publication of the current
diagnosed lag/backpressure mask and recovery. Preserve diagnostic thresholds/logs,
contain errors in noexcept observation paths and avoid scheduler-lock callbacks.
Neither unit changes gameplay policy.

**Acceptance gates:** failed poll cannot publish premature gamepad availability;
reconnect/seed/source-close and consumption alignment are explicit; pressure
enter/recover and cross-owner staging work without per-sample spam or turning
observation failure into runtime failure.

## Independent resource and media lanes

Activate these as small service units after the common endpoint/identity/error
contracts are stable. They need their own direct caller lifecycle as well as event
fanout; registering a callback alone does not complete a family.

| Unit | Implementation boundary | Acceptance boundary |
| --- | --- | --- |
| R1 / RES01 | Per-key generations, catalogue identity/revision, immutable watch state and ordered Published/Replaced/Reset after cache plus derived metadata commit. Cache hits stay silent; destructor cleanup is not Reset. | Retained old generations, exact keys, explicit reset, reentrant/concurrent commits, no listener under lock, successful reload cannot become “previous preserved” after observer failure. Base-store mechanics and affected manager/teardown wiring ship together. |
| R2 / RES02 | Distinct attempt identity and reusable batch correlation; Succeeded/FailedBeforeEffects/Partial/EffectsUnknown with validity; notify outside noexcept completion bookkeeping. | Direct return/exception/future remains authoritative; preparation/pre-execution cancellation emits nothing; zero counts do not hide registry effects; unknown effects stay explicit. No hard dependency on R1. |
| R3 / RES03 | Coherent retained named/default/fallback snapshot; revision-zero baseline; successful configure commits a revision; arbitrary permitted callers stage to one publication owner. | Concurrent configure/read/watch coherent; failed configure preserves prior state; owner-correct fanout; existing UI font imports remain unchanged. No live widget/native-alias rebinding. |
| A / AUD01 | First establish checked generations, immutable terminal snapshots and EOF/stop/restart/close/reclaim arbitration; then bounded outbox, loss reporting, cutoff pump and final post-close publication. | Logical terminal outcome/current-generation recovery, reserved terminal capacity, failed restart arbitration, overflow/gap visibility, retained EOF, no mixer/lock/destructor callback; standalone offline use without Context. Identity preparation alone is not AUD01 completion. |
| G / GFX01 | Portable retained two-transition domain token; Available flush after full initialization, Unavailable at actual shutdown/abandon through operation-final handling. | Pre/post-deactivation failures distinguished; later context-release failure preserves invalidation/primary error; abandonment idempotent; token outlives facade; no loss/recovery or old-handle resurrection claim. |

I1 adds explicit owned runtime pumping only after standalone A and M1 exist. It
must support final post-close publication before detach; it does not transfer
ownership of arbitrary application audio systems to Runtime. I2 adds domain-bound
generation provenance after R1 + G; unsupported providers stay explicit and
catalogue lifetime never becomes a proxy for native usability. Neither integration
introduces another event family or automatic resource adoption.

## Coverage and public consumption

The [testing guide](../../site/development/testing.md) owns runner procedures.
Actual CMake routing matters: old event-bus/runtime-adapter/resource files under
`all-tests` are compiled into **acceptance-engine**, not tests-engine.
[Engine routing](../../../projects/engine/tests/CMakeLists.txt).
New tests belong in the selected suite's own source directory.

| Changed boundary | Existing coverage to extend | Concrete runner |
| --- | --- | --- |
| Bus/source/binding | New narrow tests under Engine tests/unit/src; existing immediate typed identity/nested/errors and event_bus/queued delivery cases. | New focused and immediate typed cases: tests-engine. Legacy bus and queued typed delivery: acceptance-engine. |
| Runtime view/lifecycle/shutdown | Controlled window/input/renderer and execution_shutdown cases in core/runtime-adapter.cpp; portable display/runtime-contract sources. | Runtime adapters: acceptance-engine. Small display/runtime contracts: tests-engine. |
| UI attachment/time | Each module's tests/unit/src/session.cpp and runtime.cpp; extend existing shared-input/toolkit lifetime in projects/tests/ui-coexist/src/coexist.cpp with shared view-source attachment. | tests-ui-tgui; tests-ui-rmlui; tests-ui-coexist where both enabled. |
| Native resize/close | Existing native_opengl resize/nested/failure/runtime cases plus new consume/reset observations. | acceptance-opengl, executable tests-acceptance-opengl; not tests-native-glfw. |
| Catalogue/upload | asset_cache/material_cache/asset_preparation/execution_shutdown fixtures, plus shader_assets. | Cache/runtime/resource legacy fixtures: acceptance-engine. Shader assets: tests-engine. |
| Font catalogue | font_service embedded/snapshot/invalid/concurrent and existing UI font_catalogue cases. | tests-engine; selected tests-ui-tgui/tests-ui-rmlui checks preserve static adoption. |
| Audio terminal source | audio_mix and audio_stream in miniaudio tests/unit/src/audio.cpp; deterministic offline mixing. | tests-audio-miniaudio; no device needed for logical outcomes. |
| Graphics token | Controlled opengl-lifetime.cpp startup/context/deletion recorder fixtures; native retained/shutdown cases. | tests-opengl first; acceptance-opengl separately for actual native behavior. |

CTest prefixes use build target names, e.g. `acceptance-engine.`; executable names
can differ. Select focused suites instead of aggregate duplicates. Linux
`tests-native-joystick` is a separately gated syscall-wrapper mapping runner and
does not establish physical hotplug support.

Extend public first-include and linked consumers when adding headers/symbols:
`consumer-cengine` (including `consumer-headers-cengine`) and affected NativeGLFW,
UI, miniaudio and OpenGL module consumers. Preserve neutral-header guards: Engine
gains no toolkit/native dependency; UI gains no GLFW/OpenGL dependency while its
own toolkit remains intentionally public. Add no transitive native link
requirement. Preserve standalone module entry points and manual consumers; add event-aware usage without
requiring every module at once.

Use existing installed/relocated SDK consumers when public exports, installation
or ABI changes require that evidence, batching affected static/shared compositions
at the interface checkpoint. A root combined build does not establish independent
or installed consumption. Existing Windows/macOS/Wayland native deferrals remain
separate and do not gate Linux source work.

Builds, native test executables, full documentation generation, downloads and
devices require the authorization in the
[verification rules](../../for-agents/testing.md#verification-and-resource-use).
Reuse compatible configurations and caches. This planning task authorizes none of
those executions. Ready pending execution belongs in the testing queue only after
its implementation and prerequisites exist; do not add speculative requests now.

## Deferred capability boundaries

| Candidate | Required next scope before implementation |
| --- | --- |
| DEV04 DisplayTopologyChanged | Refreshable inventory, connection identity, native-handle invalidation and fallback/rebinding. Keep the existing [monitor plan](../long-term/README.md#monitor-hotplug) and deferred status. |
| DEV05 InputDeviceTopologyChanged | Portable enumeration/connection identities and real backend lifecycle observation; coordinate with [HID](../long-term/README.md#deferred-hid-integration) and native input ownership work. |
| DEV06 AudioOutputStateChanged | Actual output/device/route observation, capability limits and any recovery policy. Logical PlaybackEnded does not imply that service exists. |
| SYS11 PlatformLifecycleChanged | Real OS suspend/resume/session-ending adapters, owner marshalling and forced/deadline policy; focus/minimize is insufficient. |
| Independent native pumping | Bounded all-path pending input staging, visible overflow, whole-poll FIFO/capacity and explicit routing/capture latch/spacing semantics across pumps. Never add an extra GLFW pump under full backlog without those contracts. |

Presentation/density/observed-mode extensions to WindowState, live font adoption,
graphics loss/recovery, complete playback history, residency policy and automatic
asset hot reload remain separate capabilities. Keep application saves/gameplay and
absent service domains out of the engine event plan.

## Progression and completion

The next engineering objective is **V1–V7 as the integrated view macro**, beginning
with V1–V3 reliability before removing direct calls. This planning request does not
start implementation. Technical acceptance checkpoints are evidence requirements,
not repeated user approval prompts; existing authorization persists within the
selected scope. A material contract contradiction revises the plan before dependent
code accumulates.

Activate later units individually, keep one owning progress checklist per active
macro, and mark implementation versus executable/native acceptance separately.
When a macro completes, preserve the durable contract in its subject owner, retain
unresolved acceptance with its prerequisites and reconcile this plan with STATUS
and the planning catalogue. Do not turn the historical assessment into a progress
journal.
