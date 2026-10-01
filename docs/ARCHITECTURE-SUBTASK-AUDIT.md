# Subtask audit of completed architecture work

1 October 2026. This audit starts at saved checkpoint 57
(`68e6c7d9190d8653e17f653dc90e7e0a66bad5e6`), then incorporates A11 in 58.
The source at that checkpoint matches user-pushed 57,
`5e4adfdc94cec7590916282c876762ada4d0e123`; unapplied 58–59 were carried forward
onto that remote history without changing their final tree. Checkpoints 60–61
continue native upload and text-compatibility review. Checkpoints 62–64 add
controlled worker faults and concurrent event/invalidation combinations.
Checkpoints 65–66 continue native program construction/reflection and retained
owner review, including failure cleanup into runtime/cache dependents.
Checkpoints 67–69 review cache allocation and resource-category unwinding; A14
corrects FontMgr publication/default selection and adds scoped failure sources.
Checkpoints 70–72 prepare native registry/program allocation faults and allocator
retention through shutdown/weak release; A15 rejects texture query errors before
using their outputs. Font-atlas and combined runtime failure evidence remain open.
Checkpoints 73–76 prepare baked-font upload/cleanup, every texture/flat/mesh
registration position and two combined runtime failure scenarios in both modes.
A16 removes an allocating worker-group snapshot from shutdown recovery.
The user requested review of every completed task/subtask and its dependents,
particularly batch-ending completion claims, before advancing task 9.

**The audit is still open.** Each row records bounded source inspection in this
pass. “Checked source” is not exhaustive correctness sign-off or executed
acceptance. “Partial” names unfinished review/evidence; “Fix prepared” names
corrected source with unexecuted regression coverage. No CMake configuration,
compilation, tests, native demo or assistant remote write occurred.

## Batch-ending completion claims

| Checkpoint | Claim being reviewed | Review focus |
| --- | --- | --- |
| 15 (`ef4ec51`) | Execution tasks 1–4 complete in source | Dispatch/event/worker ownership, rollback and dependency-aware shutdown. A1/A5/A7/A10/A11 show why source completion did not establish correctness. |
| 20 (`14d5a81`) | Timing task 5 complete in source | Both loops, scheduler arithmetic, input clocks/backlog and newest useful frame; A2 rechecked. No additional defect found in these inspected timing paths. |
| 25 (`87ec70e`) | Residency task 6 complete in source | Cache release, native adoption, stages, idle collection and context loss. Category/failure evidence remains partial. |
| 30 / 34 | Partial pipeline foundations/native integration | These explicitly left integration open; do not reinterpret them as full task completion. |
| 43–45 (`2c30675` / `3492be9` / `7f042e9`) | Tasks 7–8 implemented in source | Parameter/state/domain/reload integration, retired entry points, CPU text/packets and both frame lifecycles. Compatibility/failure review remains open. |
| 51 / 54 / 57 | Convergence/recovery/audit checkpoints | Coverage preparation and delivery did not close the audit. A10 protected a pump owner; A11 addresses a separate payload owner. |

These are review anchors, not evidence that the time limit caused a particular
edit. The audit inspects current implementations and later callers against the
requirements in [the work order](ARCHITECTURE-WORK-ORDER.md).

## Requirement-by-requirement record

### Task 0

Inspected paths: Repository metadata, ledger, commit/patch history and declaration layout.

| Subtask | Result | Source evidence / remaining review |
| --- | --- | --- |
| 0.1 | Checked source | Active branch/instruction search and saved tree checked; no applicable AGENTS.md found. |
| 0.2 | Checked source | Original e8c9788 base retained; remote 57 tree matched saved local 57; unapplied 58–59 preserved and carried onto that history. |
| 0.3 | Partial | Identity verified; RenderFrame/writer and STBFont field order corrected in 61 and CODE-STYLE reconciled with the 140-column root config. Full formatting/declaration review remains; clang-format 23+ is unavailable. |
| 0.4 | Checked source | Original/incremental bases, unique numbers and saved checkpoints checked; patch replay is a separate static check. |

### Task 1

Inspected paths: platform-dispatcher.*, simulation-dispatcher.*, EngineContext, GameRuntime and runtime-adapter request scenarios.

| Subtask | Result | Source evidence / remaining review |
| --- | --- | --- |
| 1.1 | Checked source | PlatformDispatcher files/accessor, aggregators, runtime/demo/tests searched; historical mentions are distinguished from live references. |
| 1.2 | Checked source | Detached FIFO vector, owner/draining gates, separate promise/capture ownership and cancellation after unlock inspected. |
| 1.3 | Checked source | Open/bind/drain/close, owner cancellation and unbound startup rollback inspected in both runtime modes. |
| 1.4 | Checked source | Both loops drain simulation work before consuming the whole backlog at each actual recovery/update boundary. |
| 1.5 | Checked source | Shared submission state and owned scheduler wake captures inspected; saved endpoint rejection survives object destruction. |

### Task 2

Inspected paths: EventBus/EventSystem, event-delivery.*, private pump seam and event-bus regression sources.

| Subtask | Result | Source evidence / remaining review |
| --- | --- | --- |
| 2.1 | Checked source | Owned registry and EventSystem forwarding inspected; a bus does not choose a thread. |
| 2.2 | Checked source | Strong listener registry and bus-qualified weak identifier inspected; ignored identifiers remain persistent. |
| 2.3 | Checked source | Atomic entry/running handshake and registry-before-listener invalidation inspected; A1 remains incorporated. |
| 2.4 | Checked source | Invocation completion on exception, nested self-wait rejection and invalidation-before-wait inspected. |
| 2.5 | Checked source | Per-listener posting, owned copies, active-entry handshake, invalidation during preparation and ticket/capture release scopes inspected. 63 prepares simultaneous producers, held-copy invalidation and native-policy pump cancellation/reentry. Global producer interleaving and error-sink lifetime stay explicitly separate; fixtures are unexecuted. |
| 2.6 | Fix prepared | A11 pins the copied payload inside the ticket; false/throw target and destructor-redispatch sources added in 58. |
| 2.7 | Checked source | Serial pump, shared copied streams, independent streams, closure and A10 cancellation ownership rechecked against pool capture release. |
| 2.3a / 2.4a | Checked source | A1 rechecked: invalidation precedes detachment under registry ownership; user captures release after unlocking. |
| 2.6a | Fix prepared | copied payload and error ticket stay owned locally through posting; original copy/target exceptions remain observable. |

### Task 3

Inspected paths: WorkerPool/WorkerGroup, worker-affinity.* and worker-pool regression sources.

| Subtask | Result | Source evidence / remaining review |
| --- | --- | --- |
| 3.1 | Checked source | Owned physical threads, retained group/job state and weak saved-pool endpoint inspected. |
| 3.2 | Checked source | Run/fail promise split, predicate sleeping, rejection and job capture release before accounting completion inspected. |
| 3.3 | Checked source | Caps, bounded weights/priority, requested/effective policy and explicit unsupported hard topology inspected. |
| 3.4 | Checked source | Eligible-group weighted selection, FIFO removal and cap/accounting under one scheduler mutex inspected. |
| 3.5 | Checked source | Set/readback and required cached-mask revalidation inspected through the private per-pool adapter. 62 prepares query/set/mismatch, preferred fallback failure/success and recovery paths; real native execution is open. |
| 3.6 | Checked source | Restricted/empty/duplicate policy and constructor ownership/rollback inspected. 62 prepares discovery failure and rejection after a thread enters; join, original error and capture release are expected. 63 traces policy failure into published event-pump settlement. Synthetic source preparation only; actual OS rejection and context/asset/runtime acceptance remain open. |
| 3.7 | Checked source | Close-before-drain, pending/running completion, serialized join and current_pool self-wait rejection inspected. |

### Task 4

Inspected paths: EngineContext, GLFW factory, both GameRuntime shutdown paths, ASSET-LOADING example and runtime-adapter scenarios.

| Subtask | Result | Source evidence / remaining review |
| --- | --- | --- |
| 4.1 | Checked source | Lazy owned pool, injected root isolation and context groups inspected; waits release the execution mutex. |
| 4.2 | Checked source | Event headers remain independent; composition adapters own safe endpoints/groups; A11 applies to all rejection paths. |
| 4.3 | Checked source | Loader/PreparedAssets ownership through WorkerGroup to platform example inspected; nested futures are read only when ready. |
| 4.4 | Checked source | Both cleanup sequences inspected through simulation owner close, accepted CPU completion, platform closure and frame/game/input/renderer cleanup. |
| 4.5 | Checked source | Platform pumping while waiting for simulation/CPU dependencies inspected; dispatch failure cancels, maintenance failure preserves pumping. |
| 4.6 | Partial | first failure, partial init and retained endpoints inspected; A16 removes a shutdown group-copy allocation. 75 prepares asset-init failure with accepted upload/cancelled simulation plus worker upload failure observed in game cleanup. Combined policy/startup and partial-adapter failure review remains. |
| 4.7 (ledger discovery) | Checked source | GlfwOpenGLConfig.execution forwarding checked in the shared factory assembly used by owned and borrowed input overloads. |

### Task 5

Inspected paths: SimulationScheduler, PollingBacklog, TickContext, InputAccumulator/TickInput, demo, timing docs and scheduler/tick/runtime fixtures.

| Subtask | Result | Source evidence / remaining review |
| --- | --- | --- |
| 5.1 | Checked source | Clock-input scheduler, independent polling configuration and common timing options in both loops inspected. |
| 5.2 | Checked source | Simulation delta/kind/drop versus observation interval inspected with short-tap and zero-interval source fixtures. |
| 5.3 | Checked source | Positive-step/bounded loop, whole-step debt drop, remainder retention and signed clock guards inspected. |
| 5.4 | Checked source | Direct/hybrid overload, explicit prefix and cap/drop accounting checked against deterministic 500ms scheduler fixtures. |
| 5.5 | Checked source | Observed down-time fraction/held-state fallback and named demo movement helper inspected; raw observations remain separate. |
| 5.6 | Checked source | Whole-vector backlog transfer, baseline advance and ordered record transfer inspected; later recovery consumes no repeated transients. |
| 5.7 | Checked source | One preparation per batch and current/ready/free/retired slot transitions inspected; suspension/cap/interpolation documentation checked. |
| 5.1a | Checked source | A2 max-minus-spacing arithmetic rechecked; negative epoch remains arithmetic evidence, not an executed input fixture. |

### Task 6

Inspected paths: AssetMgr/AssetCacheContext, ResourceProvider teardown, native lifetime/provider/texture/VAO/program, renderer and resource documentation.

| Subtask | Result | Source evidence / remaining review |
| --- | --- | --- |
| 6.1 | Checked source | Single provider/loading owner, releasing gate, teardown and rebind inspected; no eviction policy introduced. |
| 6.2 | Partial | cache/composite/frame/native owners inspected; 73 prepares glyph upload rejection, atlas throw/null and copied metrics/resource retention; 76 prepares file/size rejection before upload. 74 adds texture/flat/mesh registry growth failures to 70 program allocation coverage. Bake failure and broader combined native failure evidence remain. |
| 6.3 | Fix prepared | A14 routes FontMgr through retained publication and commits a prepared default path atomically. 67 adds actual per-map node/bucket request rejection, replacement retention, reentry and allocator-preserving clear; unexecuted. |
| 6.4 | Checked source | Independent maintenance in both idle loops and accepted-work shutdown inspected; partial renderer init is excluded. |
| 6.5 | Checked source | 10ms platform wait cap includes full backlog/no first frame; blocking callback/presentation latency is explicitly outside the bound. |
| 6.6 | Partial | native ownership/context guards inspected; A12/A13/A15/A16 correct publication/query/shutdown recovery. 65/67/68/70 prepare program/cache/provider/allocator faults; 73/74 add atlas/texture/all VAO registration positions; 75 adds allocation/upload/runtime cleanup combinations. Bake, combined policy/startup and real context loss remain open. |
| 6.6a | Fix prepared | A12 checks generation/storage/mipmap/layout errors; A15 checks texture limit/alignment/anisotropy queries before output use. 60/71 prepare synthetic retirement/alignment/query failures, without real driver execution. |

### Task 7

Inspected paths: Pipeline/Material definitions and implementation, parameters, native builder/reflection, MaterialMgr, renderer and prepared pipeline/frame fixtures.

| Subtask | Result | Source evidence / remaining review |
| --- | --- | --- |
| 7.1 | Checked source | Immutable definition snapshots and enum/schema/layout/topology validation inspected. |
| 7.2 | Fix prepared | A13 checks construction/status/reflection before executable publication. 65 prepares native/logical/file failures and retained-program reflection recovery; 70 rejects actual registry/control-block allocation and retains allocator ownership through weak release. Native execution and broader 6.6 evidence remain open. |
| 7.3 | Checked source | Copied values, defaults < pass < material < draw, hidden-invalid layer validation, engine ownership and missing/type rules inspected. |
| 7.4 | Checked source | Image-free geometry, explicit per-binding units and guarded texture bind/unbind inspected. |
| 7.5 | Checked source | Range/domain/pass preflight precedes state changes; complete supported blend/depth/cull state and A6 clear mask inspected. |
| 7.6 | Checked source | Build-before-replace, immutable defaults/pipeline generations and retained frame owners inspected. |
| 7.2a / 7.3a | Checked source | Linked uniform/attribute mappings and active optional reset/default values inspected; missing active engine values reject. |
| 7.4a / 7.5a | Checked source | Effective fallback samplers participate in complete unit/domain preflight; native geometry limits are rechecked. |
| 7.6a | Checked source | Demo F5 platform rebuild/later simulation adoption and retained-frame/reload/failure fixture inspected. |

### Task 8

Inspected paths: DrawPacket/frame writers, CPU submission, Font/STBFont/FFont, old FFont entry points, asset migration, renderer, demo and prepared submission/frame fixtures.

| Subtask | Result | Source evidence / remaining review |
| --- | --- | --- |
| 8.1 | Checked source | Packet resource/generation retention, copied parameters, overflow-safe ranges and authored order inspected. |
| 8.2 | Checked source | CPU sprite/static/animated tile/Graphic range resolution and retained strip/triangle geometry inspected. |
| 8.3 | Checked source | Const layout, glyph resources, model/scale placement and ASCII/tab/newline/fallback source fixtures inspected. |
| 8.4 | Checked source | Original and 43 print/draw compared with layout/submission and 44 removal. Bank widths and line advance retained; one caller rotation/local newline, safe fallback, CR and space behavior explicitly recorded in ASSET-RENDER-BOUNDARIES. Rotated multiline source coverage prepared in 61, unexecuted. |
| 8.5 | Checked source | Live sources searched for retired immediate drawing/printing and DrawInfo/iDraw/Draw2D; remaining draws are low-level contracts. |
| 8.6 | Checked source | Asset-free frame/playback, indexed writers, retained vector capacity, latest complete slot and platform recycle paths inspected. |
| 8.7 | Checked source | Per-pass insertion order and ordered playback inspected; sorting/merging remain explicit TODOs. |

## Dependent paths

Dispatch/event ownership was traced into platform/simulation/worker adapters,
both update loops, saved handles, initialization rollback and shutdown pumping.
A11 follows rejected task destruction into the copied payload's destructor,
beyond the error sink already guarded by a local ticket.

Timing was traced into whole-backlog transfer, observation/derived simulation
input, demo movement, recovery, publication and idle maintenance. A selected batch
does not grow during execution; maintenance wakeups do not synthesize simulation
updates. Frame publication/recycle state transitions were read under their owner
and scheduler locks, including partially prepared frames after failure.

Residency and pipelines were traced through material defaults, native domains and
units, CPU asset/text packets, immutable reload generations and platform recycling.
The coordinated frame/reload/preparation/render failure fixture was inspected in
both modes. This is source inspection, not proof of every producer interleaving.

Checkpoint 65 follows failed program construction into ShaderMgr/MaterialMgr:
both build before cache replacement, so exceptions preserve the prior generation.
Both runtime cleanup paths preserve their first failure, finish accepted workers
while pumping platform requests, recycle frames/deinitialize game resources,
collect, then attempt renderer teardown. Maintenance failure preserves platform
pumping. A missing current context leaves tracked retirement pending until recovery
or abandonment; untracked native remnants require platform context destruction.
This is a bounded source trace, not a combined asset/worker/runtime fault fixture;
4.6 remains partial. Font geometry-then-atlas ownership was also read: an atlas
failure releases the local geometry owner, with category-specific failure injection
still outstanding.

Checkpoints 67–68 inspect TextureMgr, SpriteMgr, TilesetMgr, FontMgr, Loader,
Graphic and the ObjectReservation/managed CPU-buffer path. Texture, sprite and
tileset managers already used retained publication; FontMgr did not (A14).
ObjectReservation completes range/control allocations before construction and
retains the unclaimed slot on constructor failure. Sprite/Tileset member unwinding
releases their local geometry/image owners; Loader publishes metadata only after
all uploads. Partial completed cache entries are permitted and remain usable for
retry. STBFont geometry survives through atlas creation and releases on its failure;
font bake/atlas and native adoption/later logical allocation injection remain open.
Checkpoint 70 injects a scoped memory resource into the same registry and private
program builder used by production. Five fixtures prepare failed growth with live
and pending entries, free-slot reuse, shutdown/abandonment, untracked fallback,
later adopted-program allocation failure, and allocator retention through the last
weak control owner. Production uses the fixed new/delete entry allocator and its
existing make_shared path. No global allocator override is used. Checkpoint 71
adds two texture-query fixtures with untouched failed-query outputs: anisotropy
fails before parameter/image use; binding-limit/unpack failures retire the handle
without changing unpack alignment.

Checkpoint 73 extracts the unchanged final glyph/atlas upload sequence into a
private production helper shared by STBFont::load_font and the fixtures. Three
scenarios inspect CPU-owner release after rejected glyph upload, local glyph-owner
release after atlas throw/null, and copied pixels/vertices/metrics retained by a
successful font. Checkpoint 76 adds invalid-size/missing/empty-file rejection before
any provider upload. These sources need no installed font or native window; they
do not execute stb baking or establish safety for malformed font data.

Checkpoint 74 pads only spare registry capacity, then rejects an actual growth
request at texture, flat VAO/buffer and all three legacy mesh registrations.
The current untracked ID is discarded immediately; earlier adopted IDs retire
after constructor unwinding. Sorted ID/kind records and repeat collection prepare
one-owner/no-duplicate checks without assuming a vector growth factor.

Checkpoint 75 traces the submission-close barrier into EngineContext finishing.
Once closed, make_worker_group cannot mutate the member vector or owned pool;
a span keeps those member owners available while waiting outside the mutex.
It removes the previous fallible vector copy (A16). Group drain and owned-pool
shutdown contain no intentional storage allocation in the inspected path. Two
combined scenarios prepare asset-init allocation failure with a pending worker
upload and cancelled simulation request, and failed worker upload observed during
game cleanup with a later renderer cleanup error. Both modes retain adapters until
accepted work settles and preserve the first observed failure. These are synthetic
provider failures, not native OS startup/affinity or driver acceptance.

The CPU managed-buffer helper's preexisting manager-lifetime synchronization TODO
remains explicit; this pass does not claim concurrent manager destruction is safe.

## Remaining audit work before moving on

1. Finish format/declaration-order review (0.3). Confirmed frame/writer/STBFont field
   drift is corrected; full formatting has not been verified.
2. Complete resource category construction/failure evidence (6.2/6.6/7.2):
   font bake failures and combined native/context loss remain distinct cases.
   A12–A16 and 67/70/73–76 prepare cache/registry/program allocation, category/atlas,
   query and combined allocation/upload/runtime failures. Every new fixture remains
   unexecuted; 73's baked-data entry does not exercise font parsing/baking.
3. Finish combined producer/failure fixture review (4.6), including context/asset
   settlement after policy or startup failure and partial shutdown dependencies.
   FFont compatibility is recorded; rotated rendering remains acceptance work.

The bounded source review of the identified event ordering/invalidation and worker
native-boundary branches is now recorded. Eleven scenarios prepared in 62–63 add
controlled combinations and policy/startup faults; none has run. They do not prove
all producer interleavings, actual OS rejection, race freedom, or complete runtime
integration. Those limits stay open under acceptance and the remaining combined
failure review rather than being hidden behind a completed source row.

Old FFont file-read/validation TODOs predate its layout migration; the work order
explicitly excludes new FFont loading/features. They are not silently counted as
fixed. The private adapter now prepares controlled rollback and policy-failure sources;
these are not an executed native fault test.

Build/type/link checks, aggregate regressions, real runtime/native acceptance and
PR metadata remain open 9.5/9.7 gates. Those are additional gates, not substitutes
for finishing this source review. Task 9 remains on hold until this audit is
finished. See [A1–A16](ARCHITECTURE-EARLIER-TASK-AUDIT.md) for finding history.
