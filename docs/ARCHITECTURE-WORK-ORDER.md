# Cheryl Engine — Second-pass work order

30 September 2026. **Implementation in progress; see ARCHITECTURE-WORK-LEDGER.md for checkpoints.**

Planning snapshot: [`e8c9788f63cf4688e144b84feeb8f9aabf62540f`](https://github.com/cppcooper/cheryl-engine/commit/e8c9788f63cf4688e144b84feeb8f9aabf62540f), branch `refactor-runtime-render-resource-architecture`, [PR #9](https://github.com/cppcooper/cheryl-engine/pull/9). PR #8 is closed without merging. The branch and PR already have the recommended names; another rename is unnecessary.

## Task list

1. **Thread dispatch:** rename the platform queue and provide simulation-thread delivery.
2. **Events:** persistent registrations, safe unregister, ordered delivery, and a small event-bus implementation.
3. **Workers:** an independently owned pool with real worker groups and enforceable execution policies.
4. **Execution integration:** connect the delivery adapters, worker asset preparation, and shutdown sequence.
5. **Simulation timing:** configurable variable/fixed stepping, bounded recovery, and the larger-delta recovery option.
6. **Resource residency:** audit shared ownership, rename the cache context, and collect retired GPU resources during idle rendering.
7. **Pipelines and materials:** separate draw-processing rules from resources and parameters.
8. **Render submissions:** resolve assets before publication, migrate immediate drawing, and preserve the batching extension point.
9. **Convergence:** update documentation, prepare regression coverage, and keep runtime acceptance explicit.

Each task below has numbered subtasks, a completion condition, and a discovery boundary. The boundaries are places to expand the work order before dependent work proceeds; they are not automatic permission checkpoints.

Checked subtasks record implemented/prepared source scope, not audit sign-off or
executed acceptance. The explicit requirement/dependency audit of tasks 0–8 is in
[ARCHITECTURE-SUBTASK-AUDIT.md](ARCHITECTURE-SUBTASK-AUDIT.md); its listed source
review is finished at checkpoint 90. Task-9 source work is unblocked; executed
acceptance (9.5) and separately requested PR metadata (9.7) remain open.
Finding history is in [ARCHITECTURE-EARLIER-TASK-AUDIT.md](ARCHITECTURE-EARLIER-TASK-AUDIT.md).

## What the chronological review settled

I read these four files in creation order alongside the supplied discussion, including the replies between files:

1. [cheryl-engine-required-decisions.md](https://chatgpt.com/api/library/files/libfile_501760ef5e648191bb6dbc642c6d6094/download): original questions and proposed defaults.
2. [cheryl-engine-architecture-decisions-after-review.md](https://chatgpt.com/api/library/files/libfile_ad7603958c2481918be6d10cbb1344f2/download): immediate delivery, persistent registrations, graceful workers, strong caches, and global acquisition versus owned loading.
3. [cheryl-engine-decisions-before-second-pass.md](https://chatgpt.com/api/library/files/libfile_f8f25ef5b67081919a10b030eaff683e/download): independent delivery adapters, the bus pattern, and pipeline/material definitions.
4. [cheryl-engine-final-decision-addendum.md](https://chatgpt.com/api/library/files/libfile_1434134287dc8191a1afaf27063f4f7f/download): **WorkerGroup is required now**, the platform queue needs renaming, and **variable catch-up within fixed mode is required now**.

The final addendum overrides earlier suggestions to defer groups or hybrid recovery. Likewise, the earlier mandatory RAII-subscription proposal is superseded: EventSystem owns normal registrations until explicit unregister. Discarding a registration identifier must not unsubscribe it.

Other settled constraints remain: keep the EventSystem singleton; keep polling independent of simulation; consume the whole available polling backlog at each update; do not replay Events/Text; render the latest complete published frame; retain strong asset caches without automatic eviction; perform native GL deletion on its context thread; retain one active global cache/provider domain; make the renderer independent of high-level asset classes; stub batching/sorting rather than implement it fully.

## Planning-snapshot findings that affect the order

The reviewed execution code is identical at the planning snapshot to `54a91b6`; the additional commit only raises the CMake minimum to 3.28. C++23 remains selected.

- `EventSystem` snapshots callbacks outside its lock, but has no unregister mechanism or alternate delivery target.
- `PlatformTaskQueue` already owns move-only work, returns futures, drains detached batches, and cancels pending work on the platform thread. Preserve those useful contracts during its rename.
- `GameRuntime` has one variable update per sequential cycle and a hard-coded concurrent cadence of 16,667 microseconds. Its update delta currently comes from the input accumulator's wall-clock interval.
- `RenderFrame` and `OpenGLRenderer` still understand Sprite, Tileset, Graphic, and STBFont. `DrawStyle::material` is actually a Shader handle.
- Geometry/image binding is now independent, and images retain no binding unit. Typed material-to-native binding still needs integration.
- Deferred retirement and strong caches already exist. Collection currently occurs in `clear()`/`render()`, leaving an idle-loop maintenance gap.
- Owned loading, CPU preparation/GPU upload, transient shader stages, and successful-replacement shader reload already exist. Extend and preserve these; do not rebuild them as missing features.

## Second-pass implementation choices

These are the proposed concrete choices within the agreed architecture:

- Use **PlatformDispatcher** and **SimulationDispatcher**. Both keep specialized APIs; EventSystem only sees an optional delivery callable.
- Extract a small owned **EventBus** registry behind the existing singleton's default bus. This permits isolated lifecycle checks and contains registration state. No named-bus manager or cross-bus routing is needed.
- Let EngineContext own a configurable root pool, with an explicit alternative for an application-supplied pool. WorkerGroup handles share its capacity. The dedicated simulation thread remains outside the general pool.
- Start with a **1/60-second fixed interval**, **one fixed update per scheduler cycle**, and **DropExcessLag** recovery. All are configurable. VariableCatchUp supports one larger recovery update directly, or after configured bounded fixed updates. Propose a configurable **100 ms recovery cap**; excess above the cap is dropped and reported, not retained as perpetual debt.
- Use separate small **Pipeline**, **Material**, **PipelineDefinition**, and **MaterialDefinition** contracts. Initially definitions are typed C++ values consumed by explicit bootstrap/building APIs. File serialization is an extension point, not a new manifest system in this work.
- Publish resolved **DrawPacket2D** data. Text layout also happens before publication, preserving the current ASCII limits without leaving font knowledge in the renderer.

## 0. Establish the implementation baseline

- [x] **0.1** Recheck the active branch, source tree, and applicable repository instructions before editing; incorporate any changes since this snapshot.
- [x] **0.2** Record the original task base before source changes. Preserve an existing original base if continuing an unfinished patch series; this planning SHA does not overwrite it.
- [x] **0.3** Configure the local identity as `cppcooper <cppcooper@users.noreply.github.com>`. Follow the repository format and place declaration members above methods.
- [x] **0.4** Maintain a work-order ledger: task status, discovery additions, commit checkpoints, original base, last delivered patch base, and next unused patch number.

**Boundary D0 — repository drift:** if source has advanced, add reconciliation subtasks under the affected task before implementing its planned contract. Never replace current work with an older decision-file snapshot.

## 1. Establish thread dispatch boundaries

**Start in:** `core/engine/platform-queue.*`, `engine-context.*`, `game-framework/game-runtime.*`.

- [x] **1.1** Rename PlatformTaskQueue/files/accessors to PlatformDispatcher and `platform_dispatcher()`. Update aggregators, demo, regression sources, and documentation together.
- [x] **1.2** Preserve FIFO detached-batch drains, owner-thread checks, owned captures, future results, failure propagation, and close-time cancellation. Work submitted during a drain belongs to a later drain; delivery does not become inline merely because the caller is already on the owner thread.
- [x] **1.3** Add SimulationDispatcher with owned work, explicit open/close state, safe submission rejection, wake-up, and cancellation/result semantics. Its drain belongs to the simulation owner in both runtime modes.
- [x] **1.4** At an update boundary, drain the detached simulation-work batch, then transfer the complete polling backlog immediately before the update. Document this order; do not promise a total ordering between independently produced mailbox work and input records.
- [x] **1.5** Use lifetime-safe submission handles so saved delivery adapters reject work after the runtime closes rather than dereferencing destroyed dispatchers.

**Boundary D1 — queue lifetime:** inspect shutdown races, callback destruction, reentrant posting, and owner-thread futures. Add shared submission-state or explicit error-reporting subtasks if needed. Do not create an Executor base class to solve them.

**Complete when:** requests can reach either owner thread with explicit lifetime and failure behavior, and the same conceptual simulation boundary exists in sequential mode.

## 2. Complete event registration and delivery

**Depends on:** task 1 for thread adapters; the registry itself can be developed independently.

- [x] **2.1** Put registry/subscription state into EventBus; keep the singleton EventSystem as the default access layer. Document an isolated bus and explicitly explain that a bus does not select a thread.
- [x] **2.2** Return a bus-qualified registration identifier. The bus strongly owns callback entries until unregister or bus destruction. Keep immediate delivery on the dispatcher's thread as the default; one-shot registration is optional, explicit behavior.
- [x] **2.3** Add unregister/invalidation and an invocation-entry handshake. A queued callback must check active state atomically with entering execution; checking a boolean and then calling is insufficient.
- [x] **2.4** Distinguish removal from waiting for an already-running callback. Proposed API: nonblocking `unregister()` prevents new invocations; an explicit completion barrier permits safe destruction of borrowed callback targets. Handle self-unregister without deadlock and reject waiting on one's own callback. Queued cancellation alone does not protect a raw `this` already in use.
- [x] **2.5** Preserve owned payloads and ordered delivery. Serialize enqueue order for concurrent producers within a delivery stream. Immediate callbacks retain registration order within one dispatch; document nested dispatch and overlapping producer calls rather than promising impossible global completion order.
- [x] **2.6** Give queued delivery a small owned/type-erased callable and an explicit rejection contract. Immediate listener exceptions may propagate; asynchronous errors need an observable error sink/result path rather than a discarded future. Keep string/`std::any` channels for this round.
- [x] **2.7** Provide an ordered worker-delivery adapter: one serial drain per stream on WorkerGroup. A general pool's FIFO dequeue does not guarantee callback completion order. Other independent streams remain concurrent.

**Boundary D2 — unsubscribe and ordering:** walk through self-removal, removal of another listener, nested dispatch, simultaneous producers, target closure, and callback failure. Add separate in-flight accounting/serial-delivery subtasks before adopting the API. Surface any required change to the persistent-registration contract.

**Complete when:** a persistent listener can be removed safely, queued work cannot start after invalidation, in-flight target destruction has an explicit solution, and ordering/error guarantees are stated precisely.

- [x] **2.3a / 2.4a** Serialize invalidation with registry removal for concurrent close and unregister. Checkpoint 35 closes the detached-entry publication gap while releasing callback captures outside locks. Audit and execution limits are recorded separately.
- [x] **2.6a** Retain the copied queued payload through posting, together with its error ticket. Checkpoint 58 prevents target rejection from running a reentrant payload destructor under the posting lock; returning-false and throwing-target sources are prepared, unexecuted. Depends on 2.5/2.6 and applies to the task-4 delivery adapters.

## 3. Build WorkerPool and WorkerGroup

**Start in:** a small portable worker module; platform policy code stays behind platform-specific adapters. The Orthanc JobQueue supplies design ancestry, not transplanted implementation.

- [x] **3.1** Define pool ownership and group handles. A pool owns OS threads; a group owns workload policy/accounting. Jobs retain group state without depending on the submitting handle's lifetime. Separate physical pools remain constructible.
- [x] **3.2** Implement move-owned jobs, futures, exception capture, predicate-based sleeping on the queue mutex, explicit close, graceful drain, and join. Reject submissions after close. Destroy task captures outside scheduler locks.
- [x] **3.3** Give groups maximum concurrency and scheduler priority/weight, plus explicit CPU eligibility and topology/locality policy. Separate hard requirements from preferences and expose effective capabilities/policy; do not silently ignore an unsupported hard requirement.
- [x] **3.4** Implement fair selection among eligible groups, FIFO selection within a group, cap enforcement, and per-group accepted/running/completed accounting. These policies deliberately replace a single unconditional global FIFO; parallel jobs still need not finish in order.
- [x] **3.5** Add native affinity enforcement for supported targets and an explicit unsupported path elsewhere. Prefer stable eligible workers; avoid repeated policy changes when unnecessary. If workers switch execution domains, apply/restore policy safely before another group's job runs.
- [x] **3.6** Account for actual available CPUs, restricted environments, invalid/empty masks, affinity failures, and worker startup failure. CPU placement can support locality; it does not promise that particular data remains in L1/L2. NUMA memory placement and automatic cache optimization remain explicit capabilities/extensions.
- [x] **3.7** Add group close/drain and pool shutdown. Ordinary accepted work finishes. Cooperative stop tokens may be offered to jobs that opt in; no forced termination. Detect self-join/self-drain and document that blocking on child jobs in the same saturated pool is unsupported without a later dependency scheduler.

**Boundary D3 — enforceable policy:** before finishing the scheduler, demonstrate on paper how two overlapping affinity groups, different weights, and a concurrency cap use the same physical capacity. Add worker-policy guards, topology discovery, fairness, or capability-reporting subtasks where required. Group policy must affect execution, not merely decorate jobs.

**Complete when:** groups are real workloads sharing a root pool, their supported constraints are enforced, unsupported constraints are visible, and graceful drain has no hidden self-wait path.

Checkpoint 37 prepares parallel cap isolation and shared-worker effective-mask
restoration scenarios. This broadens task-3 coverage without closing its native
failure, startup, or executed acceptance gates.

## 4. Integrate execution services and shutdown

**Depends on:** tasks 1–3.

- [x] **4.1** Add central execution configuration and EngineContext-owned/injected pool support. Engine shutdown drains its groups; it does not close an unrelated application-owned shared pool. Avoid eagerly reserving a large machine-wide pool by default.
- [x] **4.2** Install event adapters through composition. EventBus/EventSystem headers must not include WorkerPool, PlatformDispatcher, or GameRuntime. Adapter callbacks retain safe submission state, not borrowed queue pointers.
- [x] **4.3** Replace the generic asset-preparation example with WorkerGroup preparation followed by PlatformDispatcher upload. Own Loader and PreparedAssets through the handoff. Simulation checks completion without waiting on GPU work.
- [x] **4.4** Define stopping as a staged protocol: quiesce producers/subscriptions; stop new group submissions; resolve or cancel simulation requests; keep resources and platform dispatch alive while accepted work finishes; join owned workers/simulation; finish/cancel remaining platform requests; recycle frames; then clean up game, input, and graphics.
- [x] **4.5** Pump platform requests while waiting for jobs that can depend on platform completion. Do not block the platform thread in a worker join while a worker waits on its platform future. State whether each accepted cross-domain request drains or resolves as cancelled.
- [x] **4.6** Preserve the original failure during cleanup; cover partial initialization, closed targets, worker errors, and externally retained group/delivery handles.

**Boundary D4 — cross-thread dependency:** trace preparation → upload → completion → shutdown, including a stopped simulation. Add a shutdown coordinator/nonblocking completion subtask if joining would strand a request. Separate pool drain semantics from dispatcher cancellation semantics explicitly.

**Complete when:** accepted jobs settle before their dependencies disappear, platform-affine continuations can progress during shutdown, and injected ownership has no accidental global teardown.

## 5. Generalize simulation timing

**Start in:** `game-runtime.*`, `tick-context.h`, InputAccumulator/TickInput, and runtime/input documentation. **Depends on:** task 1's update boundary.

- [x] **5.1** Extract a scheduler with injected clock inputs for deterministic regression scenarios. Configure variable pacing separately from fixed simulation delta and input polling spacing. Both runtime modes use the same timing policy.
- [x] **5.2** Split simulated delta from observed wall time. TickContext must report simulation delta, update kind, and enough elapsed/dropped-time information to explain recovery. TickInput durations remain observation-based; its elapsed interval no longer dictates fixed delta.
- [x] **5.3** Implement normal fixed accumulation with a positive configurable step and bounded updates. DropExcessLag removes excess whole-step debt while retaining the sub-step remainder. Variable mode continues reporting elapsed real time.
- [x] **5.4** Implement VariableCatchUp. During overload, permit one larger update directly, or after a configured bounded fixed prefix. Limit its delta when configured and report discarded excess. A 500 ms stall must never implicitly create 30 mandatory updates.
- [x] **5.5** Make the clock used by input-driven movement explicit. Keep raw down/hold durations for observation. Provide a clearly named simulation-time contribution helper if needed: map observed down-time proportion onto the selected simulation delta; with a zero observation interval, use current held state. This is a derived control policy, not reconstructed history. Update demo usage accordingly.
- [x] **5.6** Consume the whole available backlog at every actual update, including recovery. Later updates receive new polls or persistent state without replayed edges, relative deltas, Events, or Text. A cycle with no update leaves its backlog available.
- [x] **5.7** Prepare/publish at most the newest useful frame after a bounded update sequence. Keep rendering on the latest complete publication. Document suspension, invalid configuration, timing-cap consequences, and collision/trigger risks of large deltas. Add the requested optimization-configurer TODO; interpolation remains separate.

**Boundary D5 — simulation and observation clocks:** work through a short tap inside a 500 ms stall, an update with no new polls, input arriving between recovery calls, and an update that takes longer than the configured step. Add timing/conversion subtasks before changing demo movement. Never silently redefine the existing held-duration contract.

**Complete when:** fixed and variable modes, direct larger-delta recovery, bounded fixed recovery, and polling all have independent, explicit contracts in both runtime modes.

- [x] **5.1a** Make polling deadline saturation safe across the signed clock range. Checkpoint 36 removes the overflowing span subtraction and prepares limit/spacing coverage; the negative-epoch branch has arithmetic review only.

## 6. Confirm resource residency and maintenance

**Start in:** `templates/asset-mgr.h`, ResourceProvider, OpenGLResourceLifetime, renderer, and runtime platform loops.

- [x] **6.1** Rename ProviderBoundCache to AssetCacheContext and explain its provider/loading-owner guard. Preserve one active global provider domain, synchronized publication, teardown exclusion, and retained reader handles.
- [x] **6.2** Trace ownership for image/texture, geometry, VAO/VBO, linked program, font atlas, material/pipeline, sprite/tileset, and frame packets. Cache release must not destroy externally retained logical resources; last-owner release must retire each native handle exactly once.
- [x] **6.3** Keep residency policy explicit: cache retention, explicit clear/replacement/provider teardown, external retention, final retirement. Add no weak-cache conversion, LRU, or automatic unused-asset eviction.
- [x] **6.4** Add a backend-neutral retirement-maintenance operation and call it on the platform loop after frame recycling/other work, before waiting. Avoid duplicate expensive collections in clear/render; idle rendering must still collect.
- [x] **6.5** Ensure pending retirement can wake maintenance, or use a documented bounded maintenance wait. A loop waiting indefinitely on a full polling batch must not leave retired resources uncollected forever.
- [x] **6.6** Audit deletion and failure guards, provider release/rebind, retained resources after renderer closure, and mixed resource domains. Native deletion must require the owner and its actual current context; published-handle destruction may originate elsewhere.

**Boundary D6 — lifetime gaps:** inspect idle retirement, context loss, reentrant cache deleters, and new material-held resources. Add a notifier, domain identity, or cleanup-order subtask only where the audit demonstrates a gap. Budgeted streaming remains separate.

**Complete when:** the ownership invariant holds across all resource categories and retirement makes progress independently of new render frames or input polls.

- [x] **6.6a** Reject native texture/buffer storage and mipmap/layout failures before logical publication. Check pending errors before generation, keep guarded ownership through generation/registration, restore atlas unpack alignment before rejection, and retire tracked handles on unwind. Checkpoint 60 prepares controlled recording coverage; native execution remains open.

## 7. Establish pipeline and material contracts

**Start in:** Shader/GLSLProgram, ResourceProvider, ShaderMgr, Image/Geometry2D, and render pass/style types.

- [x] **7.1** Define PipelineDefinition (program sources, vertex expectations, topology, blend/depth/cull behavior, parameter contract) and MaterialDefinition (pipeline reference, resource bindings, defaults). Keep the 2D scope modest.
- [x] **7.2** Build backend-compatible pipelines/materials through explicit bootstrap APIs. Compiled stages stay transient; linked programs remain retained executable resources. Start with typed definitions; document a future separate definition-file parser outside generic manifest discovery.
- [x] **7.3** Define copied parameter values and ownership by pass/material/draw. Support engine semantics plus pipeline-specific scalar/vector/matrix/sampler values without common code selecting GLSL names. Specify required/optional values, type validation, and override precedence.
- [x] **7.4** Implement independently bound geometry and material resources. Remove Geometry2D's Image dependency. Texture units/sampler bindings belong to draw/material binding; sharing one image between materials must not require changing a cached image's binding-unit state.
- [x] **7.5** Give fixed render state one clear authority and define pass constraints. Validate geometry layout/topology and program/image domain compatibility. Prevent depth/blend/cull state leaking between draws or passes.
- [x] **7.6** Preserve successful-replacement shader reload with immutable logical generations. Existing frames retain their old pipeline/material generation; failed replacement retains the previous one. Do not mutate a published frame's material defaults or parameter definitions.

**Boundary D7 — actual material variety:** trace a normal sprite, alpha font, and a two-texture effect with time/color/intensity parameters. Add parameter-schema, sampler-binding, state-ownership, or recipe-publication subtasks if one cannot fit cleanly. Do not force every program to accept sprite uniforms or introduce a general hot-reload transaction.

Checkpoint 26–30 completes typed definitions (7.1), copied parameter resolution
and validation (part of 7.3), independent geometry/image binding and image-free
unit ownership (part of 7.4), and immutable definition/default snapshots (part of
7.6). The remaining work is explicit:

- [x] **7.2a / 7.3a** Add OpenGL builder mappings/reflection and value uploads. Optional active uniforms need defaults/reset semantics so absent values cannot retain previous draws' data.
- [x] **7.4a / 7.5a** Bind resolved material resources, validate layout/topology/native domains, and apply all fixed state under explicit pass constraints. Sampler units are explicit in each resolved request.
- [x] **7.6a** Publish successful pipeline/material replacements through bootstrap/reload; retain old frame generations and preserve the previous generation on failure.

The existing shader2d sprite/font sources have the same six engine/sampler roles;
font alpha comes from the atlas swizzle. The prepared two-image effect schema
uses its own time/color/intensity keys, with no forced sprite contract. Checkpoints 39–44 add fixed-state/geometry checks, recipe publication/reload,
packet/frame consumers, and immediate-draw retirement. Executed acceptance remains open.

Checkpoint 31–34 adds explicit native pipeline/material builders, cached linked
uniform locations, exact reflection/storage/Vertex2D attribute checks, copied
value uploads, and image-domain/unit validation before binding. Active optional
custom uniforms require a default/reset, and uncontracted active uniforms fail.
Linked compilation stages now detach after linking; retaining a program no longer
retains its stages. Those checkpoints prepared six recording-native scenarios without execution.
Checkpoints 40–44 supply the remaining state, geometry, reload, and frame integration.

**Complete when:** program processing rules and material resources/defaults are separate, geometry/image binding is independent, and custom materials can supply different parameter sets safely.

## 8. Resolve render packets and migrate immediate drawing

**Depends on:** tasks 6–7; uses task 5's publication cadence.

- [x] **8.1** Define DrawPacket2D with retained geometry/material generation, resolved vertex range, copied transform/parameters, and authored-order information. Use backend-neutral resource contracts; validate ranges before publication and again where backend limits require it.
- [x] **8.2** Add asset-facing submission/resolution helpers outside the renderer. Convert Sprite/cell, Tileset/Tile/TileAnimation, and Graphic into packets. Preserve per-instance animation state and the existing triangles versus independent-strip ranges.
- [x] **8.3** Resolve text to glyph packets using immutable metrics on the preparation side. Provide retained glyph geometry/atlas handles rather than borrowed references. Keep current ASCII/fallback behavior documented; Unicode input capture does not imply Unicode font shaping.
- [x] **8.4** Migrate the Font abstract contract toward layout/submission. Resolve FFont's stored print state and unchecked format cast as an explicit legacy migration subtask; do not expand this into new FFont loading/features. Preserve its meaningful contract or record a separate compatibility decision.
- [x] **8.5** Replace Graphic::draw, Tile/TileAnimation::draw, and font immediate printing with submission. Retain Asset2D as shared geometry/image composition where useful. Retire Draw2D/iDraw/DrawInfo only after their architectural responsibilities have moved; call counts do not establish legitimacy.
- [x] **8.6** Remove high-level asset includes/visitation from RenderFrame and OpenGLRenderer. Keep frame capacity reuse, latest-complete handoff, and safe recycling; resolve text and assets before publishing a complete frame.
- [x] **8.7** Add minimal ordering/batch metadata and TODOs. Authored order is the default, particularly for UI and alpha content. Stub later compatibility keys/reordering policy without implementing a sorter, batch merger, or extra GPU draw machinery.

Checkpoint 42–44 moves asset/range selection and const Font layout to CPU submission
helpers, then removes the immediate interfaces. FFont retains width and alternate-bank
layout through typed options; placement/rotation comes from the caller model. No
FFont loading features or Unicode shaping were added. Ordering remains authored.

**Boundary D8 — migration completeness:** review every immediate entry point and its replacement, especially abstract Font and FFont. Add distinct adapters/layout subtasks before removing interfaces. Confirm that packet resolution owns resources and cannot call GL on simulation.

**Complete when:** rendering consumes resolved packets without Sprite/Tile/Font knowledge, simulation has no immediate backend-draw path, and preserved interfaces have an explicit role.

## 9. Converge documentation and validation

- [x] **9.1** Update runtime/frame, input timing, asset/render, asset-loading, and resource-lifetime documents as each task lands. Add compact event-delivery and worker-policy documents with concrete registration, affinity-group, shutdown, and recovery examples.
- [x] **9.2** Add meaningful regression sources alongside implementation: pending/in-flight unsubscribe, ordered worker events, pool/group shutdown and limits, timing recovery/input clocks, cache retention/idle retirement, pipeline parameters/state, and packet/text ownership.
- [x] **9.3** Use controlled clocks, latches, and recording adapters; avoid sleeps as race proofs. Explain scenario/setup/action/result in human-readable tests. Integrate into the existing aggregate target rather than proliferating executables.
- [x] **9.4** Perform permitted static checks: formatting, whitespace, header dependencies, ownership/lock review, API/implementation correspondence, and documentation consistency. No compilation or test execution is authorized by this planning request.
- [ ] **9.5** When explicitly requested, compile normal/sandbox configurations, execute the aggregate regressions, and exercise real sequential/concurrent demos, timing policies, material/shader reload, affinity capabilities, idle collection, and shutdown/failure paths. Keep this acceptance gate open until results are recorded.
- [x] **9.5a** Compile/link current normal and sandbox Release configurations. Checkpoint 91 records successful builds of all default targets, including the normal demo.
- [x] **9.5b** Execute complete aggregate suites in both configurations. Checkpoint 91 corrects two inconsistent cancellation expectations; 332 normal and 330 sandbox cases pass without skips. Real Linux affinity mask cases execute; driver/demo/font acceptance remains open in [ARCHITECTURE-VALIDATION.md](ARCHITECTURE-VALIDATION.md).
- [ ] **9.5c** Exercise real sequential/concurrent demos, timing/polling/backpressure, resize/close and slow presentation.
- [ ] **9.5d** Exercise real driver/context/material reload, retained resources, idle retirement and shutdown/failure combinations.
- [ ] **9.5e** Complete real font parsing/rasterization, stb allocation failure and rotated FFont rendering acceptance; keep preexisting FFont feature TODOs out of scope.
- [ ] **9.5f** Record remaining actual OS policy rejection/restriction behavior and reconcile all native acceptance results before closing 9.5.
- [x] **9.6** Preserve coherent local commits and their ordering/authorship. Use detailed Adds/Updates/Revises/Deletes/Fixes messages; deliver uniquely numbered incremental mailbox patches at meaningful chunks. At implementation completion, verify and deliver one cumulative `cheryl-engine.patch` from the preserved original base, without squashing or repeating earlier commits in later incremental patches. Do not push unless requested.
- [ ] **9.7** Keep PR #9's description aligned with completed behavior and pending acceptance. Metadata updates are a separately requested action; the recommended branch/title are already in place.

Source-preparation checks above record implemented documents/fixtures and permitted
checks of changed source and reviewed paths. The completed-task source audit closes
at checkpoint 90; these checks do not establish executed acceptance. See [ARCHITECTURE-CONVERGENCE-REVIEW.md](ARCHITECTURE-CONVERGENCE-REVIEW.md)
for coverage, native failure gaps, and the local PR description draft.

**Boundary D9 — convergence:** inspect the complete startup/update/shutdown path after the execution and rendering migrations meet. Insert any necessary cross-task fixes as named subtasks with their own completion evidence. Distinguish source completion from executed acceptance; existing build reports are historical evidence, not validation of future changes.

**Complete when:** source and documentation agree, planned regression coverage is prepared, the commit/patch ledger is complete, and every unexecuted acceptance check is explicitly marked.

## Rules for expanding the work order

At each boundary, record **finding → added subtask → dependencies → completion evidence**. For example, `4.7: pump the platform dispatcher while draining asset jobs; precedes worker join; demonstrate that a worker waiting on upload can finish`.

Routine implementation discoveries stay within this work and precede affected dependents. Reopen discussion only if a finding changes an agreed contract or substantially expands scope; describe the exact conflict and a concrete proposed resolution. Do not quietly defer WorkerGroup, hybrid recovery, subscription safety, or high-level-asset removal from the renderer.

The main dependency order is **1 → 2/3 → 4**, **1 → 5**, and **6 → 7 → 8**, with **9 maintained throughout and finalized last**. Execution is not scheduled here; these dependencies describe implementation order.

Full batching/sorting, automatic residency budgets, work stealing/DAG jobs, privileged real-time scheduling, automatic topology optimization, render interpolation, physics, and complete Unicode text shaping remain separate work unless a recorded discovery demonstrates a direct requirement.

## Review anchors

Repository findings above are anchored to the reviewed snapshot, principally [EventSystem](https://github.com/cppcooper/cheryl-engine/blob/e8c9788f63cf4688e144b84feeb8f9aabf62540f/src/core/subsystems/event-system.cpp), [platform queue](https://github.com/cppcooper/cheryl-engine/blob/e8c9788f63cf4688e144b84feeb8f9aabf62540f/src/core/engine/platform-queue.cpp), [runtime](https://github.com/cppcooper/cheryl-engine/blob/e8c9788f63cf4688e144b84feeb8f9aabf62540f/src/core/game-framework/game-runtime.cpp), [render frame](https://github.com/cppcooper/cheryl-engine/blob/e8c9788f63cf4688e144b84feeb8f9aabf62540f/include/cheryl/core/rendering/render-frame.h), [renderer](https://github.com/cppcooper/cheryl-engine/blob/e8c9788f63cf4688e144b84feeb8f9aabf62540f/src/backends/opengl/renderer.cpp), [asset cache](https://github.com/cppcooper/cheryl-engine/blob/e8c9788f63cf4688e144b84feeb8f9aabf62540f/include/cheryl/templates/asset-mgr.h), and [implementation status](https://github.com/cppcooper/cheryl-engine/blob/e8c9788f63cf4688e144b84feeb8f9aabf62540f/docs/RUNTIME-IMPLEMENTATION-STATUS.md).

Worker-policy discovery must distinguish requested CPU policy from what the OS allows: Linux [cpuset documentation](https://cdn.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html#cpuset) describes effective CPU restrictions; Windows [SetThreadGroupAffinity](https://learn.microsoft.com/en-us/windows/win32/api/processtopologyapi/nf-processtopologyapi-setthreadgroupaffinity) documents processor-group constraints. These support capability reporting rather than a universal affinity promise.
