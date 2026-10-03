# Develop review and development plan

## Review scope and evidence

Reviewed remote `origin/develop`, fetched on 3 October 2026, at
`6d1ec8830c12d77b5f952160d8efc51f8c0ca74a` (Adds UI integration strategy and development plan).
The checkout matched that commit and had no pre-existing uncommitted changes.
This is a source review and implementation plan. No implementation, build,
compilation, or test execution was performed. Proposed checks below are future
acceptance work and require the authorization specified in AGENTS.md.

The review covers first-party headers, implementation sources, CMake, shaders,
asset metadata, test sources, and architecture/planning documentation. Dependency
implementations in `extern/` and the imported portability header `posh.h` are not
an independent third-party audit. The findings are not an exhaustive proof of
correctness, portability, or race freedom.

The TODO inventory is reproducible with
`git grep -n -i todo origin/develop -- include src tests`.
There are **13 TODO comments: 12 in include/src and one in tests**. README and
planning-document references are not additional code TODOs. The `unimplemented`
comment in `posh.h` is not one of these 13.

Existing recorded execution evidence is in
[architecture-validation.md](../development/architecture-validation.md): 349 normal
Release cases and 335 sandbox Release cases passed on the recorded earlier C++
snapshot. Those are historical results, not results of this review. Sanitizers,
physical GPU/compositor behavior, full Wayland execution, arbitrary fonts, and
exhaustive interleavings remain evidence boundaries.

## State of affairs

Cheryl has a working architectural foundation for a C++23 desktop 2D engine:

- EngineContext composes display, presentation, renderer, resource provider, input,
  dispatch, and worker ownership. Backend dependencies have explicit boundaries.
- GameRuntime implements sequential and concurrent execution, bounded simulation
  recovery, whole-poll input transfer, retained frames, and ordered shutdown.
- Input has semantic State, ordered physical Events, committed Unicode Text,
  capture leases, and keyboard focus with per-request epochs. These facilities
  should be reused rather than redesigned as part of UI integration.
- Rendering resolves simulation state into retained packets and immutable material
  generations. OpenGL enforces resource domains, owner threads, current contexts,
  deferred retirement, and cleanup after partial initialization.
- Asset preparation separates CPU decoding/validation from upload. Typed manifests
  supply grid, sprite, animation, tile, and autotile metadata. Cache readers retain
  handles across publication and replacement.
- Dispatchers, event delivery, and reusable worker groups have substantial lifecycle
  and failure contracts. Typed events and advanced topology are extensions to these
  contracts, not evidence that delivery or worker execution is absent.

The remaining work falls into three groups: correctness and contract weaknesses;
facilities needed by a concrete consumer such as UI or tile maps; and optional
engine expansion. Those groups should not acquire dependencies in reverse order.

### Incomplete implementations and constraints

| Area | Observed state | Consequence and planned treatment |
| --- | --- | --- |
| Byte-manager release | Weak liveness tokens precede raw manager dereferences in two deleters. | Teardown may overlap release; resolve ownership before extending allocator use (U2). |
| Memory transactions | Collections have individual locks, but checkout/return/merge/cull span collections. | Container locks do not establish operation atomicity; audit and define the supported concurrency contract (U2). |
| Legacy FFont | Checks opening, but uses default fstream mode and unchecked native-short reads. | U3 repairs incomplete reads and deprecates the class. The owner confirms the original atlas is unavailable; recovering the widths format is no longer required work. |
| Memory diagnostics | Empty stats divides 0 by 0; byte formatter has exact-boundary and suffix errors. | Diagnostics can misreport resource usage; independent corrections belong in U3. |
| Fonts | Immutable printable-ASCII atlas/layout; UTF-8 bytes become individual fallbacks. | Committed Unicode input is not Unicode rendering. Layout/shaping and editing are distinct facilities (U11). |
| Tile metadata | Queries, weighted candidate metadata, and animation targets exist. | No world-neighbor selector or deterministic selection service (U10). |
| Draw scheduling | Renderer plays authored packet order; compatibility fields are unfinished. | Preserve that correct baseline until an ordering contract and evidence justify optimization (U12). |
| Shader diagnostics | Reflection queries exist; printing methods still write to stdout. | Diagnostic transport is incomplete, not reflection itself (U6). |
| Asset loading | One active global provider/loading owner; upload may publish some entries before failure. | Explicit documented limit, not a promised atomic reload. Budgeting/multiple domains/transactional reload require separate decisions (U8). |
| Platform/display | One active rendering window; monitor snapshots; hide_cursor; monitor content scale. | No generic clipboard, cursor shapes, per-window scale-change service, or IME lifecycle (U9). |
| UI rendering | Position3UV2 geometry, images, materials, ordered passes, and fixed blend/depth/cull state exist. | No explicit generic per-draw scissor, indexed/color-rich UI mesh contract, render targets, stencil/mask, or image-update API. Select requirements through a consumer probe (U9). |
| Worker topology | Linux affinity and required/preferred policy are implemented; cache/NUMA capability is unavailable. | Required unsupported policy rejects. Keep capability reporting honest; topology is optional (U14). |
| 3D | `assets/types/3d/mesh.h` contains only guards and an author/date comment. | A public placeholder is not a 3D scene/submission pipeline. Decide scope before implementation (U14). |

### Findings beyond TODO comments

These are source findings or explicit audit questions; their severity is not based
on whether current tests happen to include them.

1. **Numeric utility header is malformed.**
   [string-numbers.h](../../include/cheryl/math/string-numbers.h) ends with an
   unmatched `#endif`. It also defines non-inline `parse_floats` in a header;
   integer parsing checks `ec` but not complete consumption, and `stod` does not
   check the parsed end position. Correct inclusion/linkage and define whether
   trailing characters, whitespace, signs, non-finite values, and overflow are
   accepted (U3). A successful aggregate build need not exercise this header.
2. **Resize delivery can throw through a native callback.**
   [window.cpp](../../src/core/display/window.cpp) calls EventSystem dispatch from
   `on_framebuffer_size`; immediate EventBus callbacks may throw. Allocation can
   also fail during dispatch. The input adapter already captures callback failures
   and rethrows from update. Establish a corresponding safe window/platform failure
   boundary, including synchronous resize callers (U4).
3. **Exception construction promises more than it can safely provide.**
   [exceptions.cpp](../../src/internal/exceptions.cpp) marks stack capture,
   formatting, and exception constructors noexcept although string construction,
   formatting, and trace resolution can allocate. Failures can terminate while
   trying to report another failure. Fixed trace streams also need explicit
   overflow/reset handling; the same review applies to
   [log.cpp](../../src/core/logging/log.cpp). Treat this as a failure-path audit,
   not a claim that every ordinary exception fails (U4).
4. **Build target does not express its dependency contract.**
   [CMakeLists.txt](../../CMakeLists.txt) gives `cherylGL` only Threads as a linked
   dependency; the demo repeats graphics/logging/native dependencies, and all-tests
   recompiles LIB_SOURCES rather than consuming the library. Includes are directory
   global. No install/export package is defined. A downstream consumer needs a
   separate acceptance case; the aggregate cannot establish package usability (U7).
5. **Logging configuration is inconsistent and integration is sparse.**
   [compile-time-logging.hpp](../../include/internals/compile-time-logging.hpp)
   defines CTWriteMask unconditionally; [block.h](../../include/cheryl/templates/block.h)
   then redefines it to zero. This disables its memory macro logs and can affect
   later macro expansion in that translation unit. Direct CELog calls bypass that
   mask. Runtime, input, asset publication, workers, and native teardown mostly
   propagate/store errors without operational logs (U5–U6).
6. **Some failures intentionally become secondary or silent.** Runtime cleanup
   retains the first failure while continuing cleanup; renderer destructor fallback
   abandons native handles; other noexcept cleanup paths catch failures. Preserve
   these semantics, but expose secondary failures and abandonment without making
   reporting throw, recurse, or wait under subsystem locks (U4–U6).

## The 13 TODOs

Locations below are at the reviewed commit. T identifiers are inventory entries;
U identifiers are development units defined later.

| ID | Source location | Analysis and resolution |
| --- | --- | --- |
| T1 | [singleton.h:11](../../include/cheryl/templates/singleton.h) | call_once protects construction only. First differing argument sets select a race winner; get_existing is not a construction synchronization mechanism. Audit both CTS/CTU access paths and all argument-bearing consumers; define explicit initialization/retrieval and operation ownership without removing architecturally useful interfaces (U1). |
| T2 | [ffont.cpp:50](../../src/assets/types/2d/ffont.cpp) | Opening is already checked. U3 repairs binary/input-only reading and complete-read rejection. The subsequent scope decision deprecates FFont in favor of supplied font files through STBFont; preserve legacy behavior without inventing the missing atlas or a new widths format. |
| T3 | [glslprogram.h:64](../../include/cheryl/backends/opengl/glslprogram.h) | Reflection data already exists. Decide whether “register events” means an event consumer actually needs records; use an explicit diagnostic sink/record boundary and keep optional event delivery separate from graphics querying (U6). |
| T4 | [pool.hpp:171](../../include/cheryl/core/resources/objects/pool.hpp) | Object handles retain PoolState, but its backing-byte deleter still captures a raw manager with a weak token. Solve the underlying release ownership once, and adopt it here (U2). |
| T5 | [mem-mgr.hpp:24](../../include/cheryl/core/resources/memory/mem-mgr.hpp) | Define zero-allocation stats deliberately, preferably zero counts with an unavailable utilization percentage or explicitly documented zero. No NaN/inf output (U3). |
| T6 | [managed-block.hpp:13](../../include/cheryl/core/resources/memory/managed-block.hpp) | A locked token cannot keep the Manager object alive or stop teardown. Retain shared release/bookkeeping state; define closing behavior and noexcept final release (U2). |
| T7 | [mem-mgr.h:9](../../include/cheryl/core/resources/memory/mem-mgr.h) | Enumerate cross-container invariants and lock order before promising concurrency. Include inherited BlockManagement and object-pool transitions, not just Manager methods (U2). |
| T8 | [event-bus.h:20](../../include/cheryl/core/subsystems/event-bus.h) | String/any channels leave payload agreement to callers. Add typed channel identity and constrained payload submission while preserving bus-qualified registration, invalidation, delivery, and error contracts (U13). |
| T9 | [draw-packet.h:28](../../include/cheryl/core/rendering/draw-packet.h) | Retained generation, geometry range, parameters/images, topology, and pipeline state determine compatibility. Define reorder-safe regions/barriers first; never sort transparent or UI packets merely by material (U12). |
| T10 | [tileset.h:45](../../include/cheryl/assets/types/2d/tileset.h) | Provide neutral neighbor sampling, signature computation, seeded weighted choice, missing-rule behavior, and simulation-time animation substitution. Avoid embedding a world implementation into asset metadata (U10). |
| T11 | [simulation-scheduler.h:15](../../include/cheryl/core/game-framework/simulation-scheduler.h) | Scheduler already has explicit bounded timing policies. Collect workload data before offering suggestions; never silently change fixed-step, input retention, or recovery semantics (U12). |
| T12 | [stbfont.h:26](../../include/cheryl/assets/types/2d/stbfont.h) | Decode scalar input, then shape/map glyph runs with fallback and cluster information. Expanding an ASCII array cannot supply shaping, bidi, graphemes, or dynamic atlas residency (U11). |
| T13 | [math.cpp:350](../../tests/executables/gtest/math/math.cpp) | Deferred byte-format tests belong with the production boundary/suffix fix. Correct the 1024 comparison and missing TiB entry; cover zero, adjacent boundaries, and representable larger values (U3). |

## In-file documentation work

Architecture documents are useful and unusually explicit about threading and
lifetime. They do not replace contracts at public declarations. Conversely, a file
with brief boilerplate is not automatically poorly documented if it only aggregates
headers. Prioritize callers' decisions and failure boundaries.

| Files | Missing or misleading in-file explanation | Planned action |
| --- | --- | --- |
| `core/logging/log.h`, `logger.h`, `internals/compile-time-logging.hpp`, `internal-logs.h` | No unified contract for lazy construction, directory/rotation defaults, sink versus logger filtering, blocking async queue, close timeout, fallback/off state, or compile mask scope. | U5 documents configuration and lifecycle with a minimal example, including behavior before init and after close. |
| `internals/exceptions.h`, `src/internal/exceptions.cpp`, `core/logging/log.cpp` | “single threaded … UB” comment conflicts with thread-local storage; truncation, allocation failure, and noexcept guarantees are unclear. | U4 documents actual guarantees and trace limitations alongside the repair. |
| `templates/block.h`, `memory/mem-mgr.h/.hpp`, `managed-block.hpp`, `objects/pool.h/.hpp` | Existing comments explain local bookkeeping but do not establish the whole-operation concurrency and destruction contract. | U2 adds invariant/lock-order and handle-release documentation before exposing concurrency claims. |
| `templates/singleton.h` | CTS/CTU names and examples do not clearly distinguish constructor accessibility, initialization races, later thread safety, and shutdown ordering. | U1 documents the supported contract for both variants. |
| `assets/types/2d/ffont.h`, `src/assets/types/2d/ffont.cpp`, `stbfont.h` | Constructor example omits the provider; width encoding, bank layout, atlas dependency, and fallback behavior are insufficiently specified. | U3 documents FFont's deprecation and preserved legacy limitations, and STBFont's caller-supplied font-file contract. |
| `math/bytes.h`, `math/string-numbers.h` | Unit boundaries, rounding, accepted syntax, full consumption, and failure behavior are not specified. | U3 gives a concise contract and examples. |
| `core/resources/fileio/file-mgr.h/.cpp` | Header explains the independent index; source incorrectly mentions discovery “by the asset loader,” which now performs its own scan. Incremental indexing, missing roots, ordering, and borrowed lookup lifetime need clarity. | U15 documents current behavior; decide refresh support only if a consumer needs it. |
| `core/resources/fileio/fonts-system.h`, `src/core/resources/fileio/fonts-list.cpp` | Public declarations lack skipped-root/error, preference, enumeration, and collection-face selection semantics. | U15 documents discovery versus default selection and checks what FontMgr actually supports. |
| `core/display/window-interface.h`, `display-system-interface.h`, `src/core/display/window.cpp` | Ownership is stated, but platform affinity, monitor snapshot freshness, resize event delivery/failure, scale changes, and borrowed window lifetime need local contracts. | U4/U9 document current guarantees and capability additions. |
| `assets/types/3d/mesh.h`, `src/main.cpp` | Placeholder and global signal/trace bootstrap have little statement of purpose or scope. | U14 records the placeholder contract; U5/U7 explain bootstrap ownership and static-library linkage implications. |

U15 includes an inventory pass over the remaining exported asset definition,
submission, parameter, camera, input, dispatch, and utility declarations. Add
purpose, ownership, valid thread, preconditions, failure/publication guarantees,
and units where absent. Keep algorithm comments for non-obvious invariants. Do not
replace useful existing comments with uniform boilerplate or perform formatting
cleanup as part of documentation work.

## Logging boundaries and thresholds

### Foundation that must be resolved first

The engine has CELog, engine, and memory names, rotating file and console sinks,
an asynchronous spdlog pool, and lifecycle synchronization. Reuse that foundation.
Decide whether additional categories are separate logger instances or structured
subsystem fields; each instance currently creates its own rotating file.

Required common fields: subsystem, operation, outcome, session/domain ID, thread
role, and applicable asset key/generation, event/registration ID, group/request ID,
or timing/backlog count. Prefer logical IDs to pointers. Diagnostics should not
include committed text, clipboard contents, entire event payloads, shader source,
or absolute user paths by default.

At error boundaries, log once where the engine decides to recover, cancel, fall
back, or terminate a session. Lower layers attach context and propagate failures.
Do not log the same exception at every catch or in every exception constructor.
A callback/job exception returned to a future is caller-owned; report engine
policy failures, and offer diagnostics for otherwise unobserved engine-owned work.
Expected close/rejection and superseded frames are not errors.

Logging must not add allocation-dependent or blocking work to native callbacks,
noexcept deleters, memory transactions, or scheduler critical sections. Copy bounded
metadata or update counters under locks and emit after unlock. Provide a minimal,
nonthrowing fallback for bootstrap/teardown failure. The current async overflow
policy blocks, so calling the normal logger while holding these locks is unsafe
as a general integration policy. Review sink failure, queue saturation, recursion,
static destruction, and flush/close ordering before integrating broadly.

### Required subsystem records

| Boundary and source | Required records and level | High-volume diagnostics |
| --- | --- | --- |
| Context/runtime (`engine-context.cpp`, `game-runtime.cpp`) | INFO session begin/end, mode and selected timing/input policies; ERROR terminal phase failure and each distinct secondary cleanup failure; WARN abnormal incomplete cleanup. | DEBUG startup/shutdown phases and aggregate slot/backlog status; TRACE sampled slot transitions. Preserve first exception. |
| Simulation/input handoff (`simulation-scheduler.cpp`, `polling-backlog.cpp`) | WARN sustained/repeated lag drops or prolonged backpressure with duration/count; INFO recovery summary after a degraded period. | Counters for updates, dropped lag, waits, supersession, skipped publication; periodic DEBUG summaries rather than per-tick INFO. |
| Dispatch (`platform-dispatcher.cpp`, `simulation-dispatcher.cpp`) | DEBUG open/bind/close with accepted/cancelled counts; WARN sustained queue growth; ERROR engine-owned request failure when no higher owner reports it. | TRACE opt-in request flow with stable correlation. Expected shutdown cancellation stays DEBUG. |
| Workers (`worker-pool.cpp`, `worker-affinity.cpp`) | INFO pool capacity/capabilities; WARN requested preferred-policy fallback or later degradation; ERROR required-policy failure/startup failure; DEBUG group creation/drain summaries. | Pending/running/completed/policy-failure counters and queue latency; sampled TRACE job selection. No payload logging. |
| Events (`event-bus.cpp`, `event-delivery.cpp`) | DEBUG register/invalidate/close counts; ERROR via the established error-owner boundary for failed/cancelled active queued delivery. | TRACE names/IDs/target, never any payload. Explicit unregister/close discards are DEBUG and should not duplicate error-handler logs. |
| Display/GLFW (`display-system.cpp`, `window.cpp`, `glfw-context.cpp`) | INFO initialization/window/context selection; ERROR native failures with code/operation; DEBUG mode, size, scale, and attachment changes. | TRACE coalesced resize details. Install/chainscope GLFW error reporting before init without throwing from its callback. |
| Input (`input-system.cpp`, `input-routing.cpp`, `input-capture.cpp`) | INFO adapter capability summary; DEBUG attachment, capture/focus changes and device availability; ERROR deferred callback/publication failure; WARN unsupported requested capability. | Count records and capture changes; do not log text or every physical key by default. |
| Assets/files/fonts (`asset-loader.cpp`, managers, `fonts-list.cpp`) | INFO batch begin/end with counts/duration; WARN recoverable optional-root/default-font fallback; ERROR required parse/decode/upload failure at loading owner, including partial publication status. | DEBUG key/cache/reload/generation decisions; TRACE per-file timing opt-in. Missing optional roots and ordinary cache misses are not warning storms. |
| Render/shaders (`renderer.cpp`, `program-builder.cpp`, `glslprogram.cpp`, `pipeline.cpp`) | INFO backend/version/capabilities; ERROR compile/link/upload or frame-domain failure; WARN successful compilation diagnostics where supplied; DEBUG reflection/reload summaries. | TRACE sampled packets/state switches. Native debug callback is capability-gated (OpenGL 3.3 baseline does not guarantee KHR_debug), filtered, bounded, and nonthrowing. |
| Native lifetime (`resource-lifetime.cpp`) | DEBUG collection/shutdown counts; WARN/ERROR abandonment with reason and handle count, according to whether recovery succeeded; ERROR cleanup failure through nonthrowing fallback. | TRACE logical handle lifecycle sampled; counters instead of a log on every destructor. |
| Memory (`block.h`, `mem-mgr.hpp`, `pool.hpp`) | ERROR invalid ownership/return invariants at the selected error owner; INFO explicit preallocation summary; WARN sustained pressure if a budget is introduced. | DEBUG periodic allocation/reuse/cull statistics; TRACE block split/merge only on opt-in. Ordinary no-neighbor/no-contiguous results must cease being WARN; per-request/merge INFO is too noisy. |
| Logging itself (`log.h`, `osink.h`) | Minimal fallback reports sink/init/flush/close failure without recursively calling the failed logger. | DEBUG configuration snapshot through a safe external diagnostic path. |

### Proposed compile-time profiles

These are defaults to implement, not current behavior. Threshold means the least
severe retained level. Preserve WARN/ERROR/CRITICAL in all normal profiles; off is
an explicit application choice. CRITICAL is for unrecoverable invariants or process
failure, not every rejected request.

| Profile | Retained levels | Mask in existing severity-bit representation |
| --- | --- | --- |
| Debug/developer | TRACE and more severe | `0x3f` |
| RelWithDebInfo/support | DEBUG and more severe | `0x1f` |
| Release/shipping | INFO and more severe | `0x0f` |
| Explicit minimal distribution | WARN and more severe | `0x07` |

Use target-wide configuration, independently overridable by subsystem when useful.
Memory/render/input hot-path TRACE can be disabled in support builds even when
other DEBUG remains. Remove include-order overrides. Apply the policy to direct
formatted calls as well as stream macros, and guard expensive arguments against
runtime filtering. Do not infer that spdlog's active-level setting strips ordinary
method calls. Avoid inconsistent macro-expanded template definitions across
translation units; record the consumer-header configuration contract in U5/U7.

### Proposed runtime defaults

| Profile | Logger gate | File sink | Console sink |
| --- | --- | --- | --- |
| Developer | DEBUG | DEBUG | INFO |
| Support/capture | DEBUG | DEBUG | WARN |
| Release | INFO | INFO | WARN |

TRACE requires explicit category selection, bounded capture, and a duration/size
limit. Runtime filtering cannot restore code removed at compile time. The logger
gate must admit the lowest level needed by either sink; console WARN must not
force the file's INFO/DEBUG records to disappear. Memory starts at WARN in Release
and INFO in developer sessions, with DEBUG statistics on request. Other subsystems
use the table defaults, with repeated degradation warnings rate limited and a
suppressed-count/recovery summary. Application configuration should override these
values; do not rely on incidental spdlog defaults.

## Ordered development units

Each unit is a prospective coherent commit or short series. The order resolves
contracts before dependent implementation. Independent units may proceed after
their own prerequisites; discovery gates must be resolved before dependent work.
Implementation began on 3 October 2026 at the user's subsequent request. Build,
compilation, test execution, and remote pushes remain unauthorized.

### U0. Establish scope and acceptance baselines

- [x] Classify each finding as a confirmed defect, contract audit, required consumer
  facility, or optional extension; retain the source evidence above.
- [x] Reconcile this plan with todo.md and the existing UI integration plan. Mark
  already implemented frame/input/resource foundations as established, while
  retaining their consumer acceptance checks.
- [x] Select first concrete consumers and supported platforms. Keep audio, network,
  physics/world, 3D, topology, and automatic residency optional unless required.
- [x] Define future check configurations and permissions; retain historical results
  as historical rather than reopening completed architecture work.

The decisions and classification below complete this planning unit. A checked
planning/implementation subtask records its deliverable; it does not imply its
future executable acceptance checks have run. U1/U2a/U3a remain source-only changes
with execution evidence pending.

#### Finding classification

A confirmed defect here means the failure mechanism is established by source;
it does not imply a fresh execution reproduced it. Contract audits identify a
missing guarantee or unresolved design. A required consumer facility is necessary
for a named consumer in this plan. Optional extensions are not prerequisites to
current engine use. Documentation corrections are tracked with their contract
audits unless they merely describe an already established implementation.

| Review entry | Classification | Scope and owning unit |
| --- | --- | --- |
| T1 singleton | Confirmed publication/access defect plus initialization contract audit | U1 implements synchronized publication, usable CTU construction, and explicit configuration. Legacy compatibility and Type operation ownership are stated rather than inferred. |
| T2 FFont | Confirmed unchecked-read defect plus deprecated legacy facility | U3 repairs incomplete reads and marks FFont deprecated. Recovered widths do not restore the missing atlas; semantic format recovery is removed from required work. Existing interfaces and legacy behavior remain. |
| T3 reflection diagnostics | Required consumer facility | Engine diagnostic consumers need records rather than direct stdout. Queries already exist; event delivery is a consumer choice in U6. |
| T4/T6 byte handle release | Confirmed lifetime defect | U2a removes raw-facade dereferences; it does not close operation concurrency. |
| T5 empty stats | Confirmed arithmetic defect | U3a reports an unavailable percentage for zero allocation. Shared-domain snapshot acceptance remains with U2b. |
| T7 memory transitions | Contract audit with concrete race/rollback hazards | U2b must cover shared-domain transactions, detached destruction, failure rollback, and construction tracking before promising concurrency. |
| T8 typed channels | Optional extension with a representative engine-event consumer | U13 preserves existing named/any, registration, and delivery contracts. |
| T9 batching/order keys | Optional measured optimization | U12 requires order-safe regions and workload evidence; current authored order remains valid. |
| T10 tile selection | Required facility for a tile-map consumer | U10 uses a neutral neighbor sampler and deterministic selection; no world/entity framework is implied. |
| T11 timing advisor | Optional measured optimization | U12 provides suggestions without replacing explicit timing/input contracts. |
| T12 Unicode layout | Required facility for Unicode-rendering consumers | U11 is separate from current committed-scalar input and the ASCII demo. No shaping-library dependency is selected yet. |
| T13 byte boundaries | Confirmed arithmetic/unit defect with deferred regression work | U3a corrects boundaries/suffixes and supplies regression sources. |
| Finding 1 numeric header/parser | Confirmed inclusion/linkage defects plus parse-syntax contract audit | U3 must fix the unmatched endif and header definition, then specify complete-consumption/range behavior. Still open. |
| Finding 2 resize callback | Confirmed exception-containment gap | U4 defines native callback capture and normal platform reporting, including explicit resize calls. |
| Finding 3 exception/trace failure | Confirmed allocating-noexcept hazard plus bounded-formatting contract audit | U4 must resolve failure reporting, truncation, and stream reset. The stale comment is independently removed. |
| Finding 4 CMake consumer contract | Required facility for an independent downstream engine consumer | U7 must propagate dependencies and test the actual library; install/export scope is an explicit packaging decision. |
| Finding 5 logging masks/integration | Confirmed configuration defect plus required diagnostic facility | U5 resolves masks/gating/lifecycle before U6 adds logs. Proposed profiles are design defaults, not measured performance claims. |
| Finding 6 secondary/suppressed failures | Required diagnostic facility around an intentional error-ownership contract | U4–U6 retain the first exception while making distinct cleanup/fallback outcomes observable. |
| ASCII atlas and committed-text limits | Declared capability limit | Current demo remains a valid baseline. Unicode shaping/editing/IME work is required only for the corresponding U9/U11 consumer scope. |
| Single provider and partial upload publication | Declared capability limit plus resource contract audit | U8 documents retry/publication and decides required changes before UI/resource consumers depend on them. |
| Missing UI mesh/clipping/platform/routing facilities | Required facilities for the scoped UI boundary probe below | U8/U9 add the missing neutral contracts, reusing current render/input foundations. Advanced effects remain conditional. |
| Missing 3D implementation, topology adapters, audio/network/world/physics, additional backends, automatic residency | Optional extensions | U14 keeps these separate; an actual application requirement can promote a prerequisite at a discovery gate. |
| Imported portability/dependency internals | Outside this first-party review | No independent third-party audit or replacement is proposed. |
| In-file documentation inventory | Contract audits and documentation tasks | Singleton, memory, exception, font, numeric, display, file/font discovery, logging, and bootstrap/placeholder comments follow U1–U5/U9/U14/U15 as mapped in the documentation table. |
| Sanitizers, physical GPU/compositor, complete Wayland, other OS/font/device behavior | Acceptance-evidence gaps | Add targeted evidence when authorized; absence of coverage does not itself prove an incomplete implementation. |
| Asset-manifest metadata gaps | External data/authoring dependencies | asset-manifest-todo.md remains authoritative; neither generic engine code nor matching sheet dimensions supply missing artwork semantics. |

#### First consumers and platform scope

The initial correctness and diagnostic consumers are concrete existing code:

1. [demo.cpp](../../tests/executables/demo.cpp), in sequential and concurrent modes,
   exercises input/focus, font/geometry storage, material reload, presentation,
   and shutdown. It is the first native application acceptance consumer.
2. [runtime-adapter.cpp](../../tests/executables/gtest/core/runtime-adapter.cpp)
   provides the controlled in-memory game/display/input/provider/renderer graph.
   It is the first portable contract consumer; failures, dispatch, publication,
   and retained generations can be checked without assuming a driver result.
3. The planned independent U7 application must link the actual exported library
   target, without compiling LIB_SOURCES or repeating the demo's dependencies.
   It establishes downstream usability independently of the aggregate tests.

The first new UI-boundary consumer is a **library-neutral submission/input probe**
extending the recording graph: overlapping translucent panels, a scrollable region,
a textured image, an ASCII label, and a keyboard-focused editing target while
controller gameplay continues. This selects required contracts before committing
a widget-library-specific adapter. The existing UI strategy still proposes TGUI
as the likely first concrete adapter; that tentative choice is retained, not turned
into a dependency or an assertion that a TGUI integration has been accepted.
Toolkit/version and representative widget requirements must be settled before
adapter-specific U9 work; neutral correctness work does not require that choice.

For the first probe, the discovery decisions are explicit:

| Question | Initial decision | Dependency consequence |
| --- | --- | --- |
| Multiple resource domains/windows? | One active provider/rendering window, matching the current supported contract. | No multi-domain cache redesign is a prerequisite. A later adapter requirement reopens U8 first. |
| Mutable textures or dynamic atlas growth? | Create owned CPU image data and immutable backend images; publish replacement handles for changed images. Use the existing ASCII atlas for the first probe. | Dynamic image creation already exists. Mutation/update ordering and atlas growth remain U8/U11 decisions; no in-flight handle is changed implicitly. |
| Complex clipping/effects? | Rectangular clipping with explicit logical-to-framebuffer coordinates is required for the scrolling region. No stencil, filters, or offscreen render targets in this first proof. | Scissor is a real U9 gap. Advanced operations are conditional extensions, not baseline repairs. |
| Vertex colors/indexed geometry? | Colored/translucent geometry needs an explicit neutral color representation. Do not require indexed storage for correctness if expanded triangles suffice. | Resolve color/layout and retained geometry contracts in U9; evaluate indexing with the selected adapter rather than assuming it is necessary. |
| Routing? | Reuse current keyboard focus and immutable record views. Check pointer overlap/capture and modal ownership as distinct missing contracts. | Preserve State/Events/Text; any routing expansion precedes consumer dependence. |
| Unicode/IME? | Existing committed scalars and ASCII presentation are the baseline. Composition/preedit, grapheme-aware editing, bidi, and shaping are not claimed. | No IME interface or shaping dependency is required for the first probe; those requirements trigger U9/U11 before a Unicode editor. |
| Clipboard/cursor/DPI? | The later native widget proof must explicitly request capabilities; unavailable operations must be reported as unavailable. The recording probe can provide deterministic capability responses. | Neutral platform service and per-window scale requirements remain U9; monitor content scale is not a substitute for a per-window scale-change contract. |

The primary native acceptance target for this implementation sequence is **Linux
with GLFW/X11 and OpenGL**, matching the recorded environment. Normal and sandbox
Linux configurations both remain in scope; the latter retains portable engine
contracts but excludes the Gainput/windowed demo paths. The renderer's advertised
baseline remains OpenGL 3.3; the recorded native run used Mesa llvmpipe core 4.5 and
does not establish minimum-version, physical GPU, or compositor coverage.

The in-memory contract tests should remain portable C++23. Existing platform
branches and unsupported-capability reporting must be preserved. Windows, macOS,
a complete Wayland-only configuration, physical GPU/device behavior, and topology
extensions are later acceptance targets with no fresh support claim from this
work. This specifies where the current work is to be accepted; it does not remove
or redefine the repository's other platform interfaces.

#### Acceptance configurations and permissions

| Future check | Configuration and purpose | Current status |
| --- | --- | --- |
| Focused correctness regressions | Singleton publication/retry/access; late byte release; numeric boundaries; isolated zero-total stats; memory transaction/rollback/construction tracking once U2b is implemented. | Added sources where noted; nothing compiled or executed. Empty stats requires a genuinely isolated domain/process, not merely a fresh facade. |
| Normal aggregate | Existing Linux normal Release recipe in architecture-validation.md, including the actual demo and aggregate. | Future authorization required; prior 349-case result belongs to the earlier recorded snapshot. |
| Sandbox aggregate | Existing Linux CHERYL_SANDBOX_BUILD Release recipe; generic runtime/provider/input contracts without the windowed demo/Gainput adapter. | Future authorization required; prior 335-case result belongs to the earlier recorded snapshot. |
| Native acceptance | Usable GLFW/X11 display; CHERYL_NATIVE_GL_TESTS opt-in; repeat both demo modes, resize/focus/reload/close, and targeted native failure cleanup. | Future authorization and environment required; missing opt-ins/fixtures are not acceptance passes. |
| Real-font failures | Explicit usable TTF and CFF fixtures for the existing allocation-failure cases. | Future authorization and fixture paths required; skip/failure distinctions remain as documented. |
| Memory sanitizers | Existing Debug ASan/UBSan setup; a separate TSan configuration for concurrency, avoiding incompatible sanitizer combinations. | Not run. TSan configuration is an explicit U2b prerequisite, not an assumption about current CMake. |
| Actual library consumer/header checks | Independent U7 executable and relevant header/multiple-translation-unit cases linked against the library contract. | Not yet implemented; an aggregate that recompiles sources cannot replace it. |
| Logging acceptance | Compile-time profile exclusion, runtime/sink gating, saturation/fallback, producer shutdown, and normal-session noise checks after U5/U6. | Not yet implemented; runtime filtering cannot restore stripped calls. |
| Documentation/source checks | Source references, local links, git diff --check, reviewed ownership/contract changes, and remote-baseline versus current TODO inventory. | Permitted without compilation; used for this sequence. |

No build, compiler invocation (including configure-time compiler probes), test
execution, or remote push is authorized. Adding regression sources and static
checks is permitted. Historical results remain unchanged in architecture-validation.md;
new changes need their own recorded execution before acceptance is claimed.

**Discovery boundary:** A new consumer requiring multiple domains, image mutation,
complex clipping, or IME must revise U8/U9/U11 before its implementation or dependent
application work begins. If a source audit reveals a correctness prerequisite, add
it at the earlier unit boundary, as done for shared-domain transactions and ObjCtor
tracking in U2b. Optional extension work does not bypass incomplete correctness
contracts.

### U1. Define singleton initialization and ownership contracts — T1

- [x] Inventory argument-bearing singleton construction, nonconstructible retrieval,
  get_existing, private-constructor CTU examples, and teardown consumers.
- [x] Decide explicit initialize/get behavior, repeated/conflicting configuration
  handling, constructor failure retry, and synchronized publication/retrieval.
- [x] Preserve owned Loader and its root-mismatch contract; avoid reintroducing global
  assumptions into new consumers. Audit CTS and CTU independently.
- [x] Document later operation affinity/thread safety and shutdown order.
- [x] Plan short regression cases for concurrent first configuration, retrieval
  during initialization, failed construction, and CTU accessibility.

**Acceptance:** Caller-visible initialization is deterministic or explicitly
restricted; retrieval does not race publication; useful interfaces remain.
**Discovery boundary:** If consumers require independently owned instances or a
reinitializable service, record that as a separate ownership unit rather than
forcing it into generic singleton behavior. U2 must not depend on static teardown
order to make byte release safe.

### U2. Resolve memory transactions and final-release lifetime — T4, T6, T7

- [x] Enumerate registry/section/pool/stale/release invariants and all transitions,
  including PoolState and inherited BlockManagement methods.
- [x] Choose operation serialization or a proven lock-order transaction scheme;
  define concurrency with close, stats, cull, and object destruction.
- [x] Move byte release bookkeeping into safely retained state, or another design
  that actually serializes close with final release. Remove the two weak-token/raw
  manager release races together.
- [x] Specify what happens to outstanding storage after facade destruction and
  ensure final-release paths cannot throw through shared_ptr deleters.
- [x] Keep callbacks, object destruction, backing frees, and logging outside locks.
- [x] Plan interleaving cases for release/close, split/return/cull, duplicate returns,
  zero/live slots, and failure rollback; plan sanitizer coverage when authorized.

**Acceptance:** Both handle paths use the same safe byte release contract; every
multi-container operation preserves stated invariants and lock order.
**Discovery boundary:** If inherited containers or static object-construction
bookkeeping cannot support the chosen contract, split out that required repair
before adding consumers. Do not stop at replacing a weak token while leaving
cross-container races or throwing final deleters.

### U3. Complete independent file and numeric correctness units — T2, T5, T13

- [x] Fix human_readable boundaries, TiB-and-larger suffix order, zero/rounding, and
  necessary self-contained includes; add the deferred boundary cases with it.
- [x] Define and implement empty stats and verify totals/utilization snapshots
  against the U2 concurrency contract. The zero-denominator repair is independent.
- [x] Resolve FFont's architectural purpose before extending its file format: the
  owner confirms the original atlas is unavailable and chooses class deprecation.
  Use STBFont for system or bundled font files; retain existing legacy interfaces.
- [x] Open binary input-only and reject incomplete/failed reads before geometry/provider side effects.
- [x] Mark FFont deprecated, document preserved native-short/bank/atlas/trailing-data
  limitations, and remove semantic format recovery from required implementation.
- [x] Repair numeric-header guard/linkage defects; decide complete parse syntax and
  retain meaningful distinctions between invalid input and out-of-range values.
- [x] Plan malformed/truncated font fixtures, zero upload on rejection, exact byte
  boundaries, and independent header/multiple-translation-unit numeric checks.

**Acceptance:** Incomplete/unreadable legacy widths are rejected before upload;
FFont is deprecated with its existing behavior and interfaces preserved. Supplied
font files use STBFont, diagnostic output is finite/accurate, and numeric utilities
have a usable explicit contract. Deprecation does not claim semantic validation of
arbitrary full-size legacy files.
**Discovery boundary:** The recovered widths fixture cannot supply the unavailable
atlas. Do not infer a portable encoding, universal width limits, or replacement
artwork. Class removal and bank-interface migration require a separate decision;
Unicode shaping and font selection/cache extensions retain their own boundaries.
Commit these separate fixes independently.

### U4. Establish native callback and failure-reporting safety

- [x] Define deferred resize/native failure ownership and where it is checked in
  platform pumping and explicit resize calls. Never unwind through C callbacks.
- [x] Audit error construction/trace formatting for allocation, truncation, repeated
  calls after stream failure, and noexcept claims; choose bounded fallback behavior.
- [x] Preserve runtime first-failure semantics while retaining/reporting subsequent
  cleanup failures with phase context.
- [x] Define a nonthrowing reporting contract for destructors, native callbacks,
  logger failure, and allocation exhaustion; avoid a logger dependency cycle.
- [x] Plan throwing-resize-listener, partial startup, primary-plus-cleanup-failure,
  exhausted/truncated trace, and fallback reporting acceptance cases.

**Acceptance:** Failure reporting does not replace the original failure, throw from
native callbacks/deleters, or silently hide a distinct cleanup failure.
**Discovery boundary:** If safe polling needs a new platform failure interface,
settle it before U9 adapters. If trace/exception ABI changes are required, make
that an explicit contract decision before broad call-site changes.

### U5. Make logging configuration and lifecycle explicit

Prerequisites: U1 initialization contract and U4 fallback contract.

- [x] Choose category representation, initialization owner, logical correlation IDs,
  supported destination configuration, and file/rotation defaults.
- [x] Replace unconditional/global include-order masks with target-consistent
  profiles; apply gating to formatted and streaming calls, including expensive args.
- [x] Add explicit runtime logger/file/console defaults and overrides; document
  compile-time versus runtime behavior.
- [x] Decide blocking versus bounded/drop behavior for queues and separate critical
  fallback reporting from ordinary async delivery. Count/report dropped diagnostics.
- [x] Define startup failure and close/flush ordering after all producers stop;
  preserve existing reopen and retained-resource semantics.
- [x] Plan acceptance for each compile profile, disabled side-effect expressions,
  include-order consistency, per-sink levels, saturation, sink failure, and shutdown.

**Acceptance:** Configuration is independent of include order, required failures
remain reportable, and logging cannot create subsystem deadlocks.
**Discovery boundary:** If existing singleton/log lifecycle cannot satisfy bootstrap
or embedded-host ownership, finish that focused change before integration. Do not
insert more calls into memory transactions while the queue can block there.

### U6. Integrate subsystem diagnostics — T3

Prerequisite: U5; U2 must establish safe memory logging locations.

- [ ] Add runtime/platform/native lifetime startup, failure, abandonment, and
  shutdown records first, using the logging-boundary table.
- [ ] Add worker policy and dispatch/event delivery diagnostics without duplicating
  future/error-handler ownership. Introduce stable logical IDs where necessary.
- [ ] Add asset batch/publication/reload records, including partial upload results.
- [ ] Replace direct shader stdout output with records derived from existing
  reflection queries; provide optional diagnostic-event delivery only if required.
- [ ] Add input/display capability and focus/device transition records without
  capturing user text. Rate limit resize and repeated degraded-state warnings.
- [ ] Reclassify memory expected misses and hot-path INFO/WARN records; provide
  counters/periodic summaries for memory, timing, queues, and frame supersession.
- [ ] Gate native graphics debug callbacks by capability and severity; do not make
  KHR_debug mandatory for OpenGL 3.3.

**Acceptance:** Failure scenarios identify operation/phase/domain and outcome;
normal frames are quiet; sink failure never changes engine error propagation.
**Discovery boundary:** Unexpectedly noisy boundaries require aggregation or level
changes before continuing. Missing IDs/metrics are small explicit prerequisites,
not permission to refactor unrelated scheduling or resource interfaces.

### U7. Make the engine target consumable

- [x] Audit public/private dependency and include propagation for normal/sandbox
  configurations. Determine which native libraries are platform-specific.
- [x] Express dependency requirements on the library target; add a stable consumer
  target name and choose install/export/package scope.
- [x] Add an independent consumer that links the actual library rather than
  recompiling its sources; plan header self-containment checks.
- [x] Review signal/trace bootstrap ownership in src/main.cpp and static-library
  inclusion behavior; opt-in tooling should not depend on incidental linkage.
- [x] Keep optional backend/input/UI dependencies separate where the selected
  packaging boundary requires it.

**Acceptance:** A minimal downstream application can consume the declared engine
contract without repeating the demo's dependency list.
**Discovery boundary:** If generic public headers pull native types/dependencies
through umbrella includes, choose supported entry points before package export.
Do not use this unit as broad CMake cleanup. Cross-platform checks are separate
from claiming current Linux evidence covers other systems.

### U8. Resolve resource extension requirements before consumers

- [x] Document the supported one-provider domain and partial batch publication at
  declarations, including retry behavior and prepared-data ownership.
- [x] Decide whether the chosen consumer needs atomic batch reload, multiple
  domains, dynamic images/updates, eviction budgets, or none of these.
- [x] For required changes, define retained generations, in-flight frame safety,
  owner-thread upload, request cancellation, and publication rollback first.
- [x] Design metrics/budget accounting before automatic residency policies.

**Acceptance:** Consumer resources have explicit creation/update/publication and
retirement rules; unsupported capabilities are explicit.
**Discovery boundary:** Mutable resources may invalidate retained-frame snapshots.
Choose versioned replacement or ordered updates before exposing an update API.
Avoid extending singleton caches as the foundation for multiple domains.

### U9. Complete generic UI-facing facilities, then prove an adapter

Prerequisites: U4–U8 as applicable; use the existing
[cheryl-ui-integration-plan.md](cheryl-ui-integration-plan.md) for adapter milestones.

- [ ] Build a requirements matrix for the selected first adapter against current
  render/resource/input/platform contracts. No dependency/library choice is made
  by this review.
- [ ] Specify clipping coordinates, scaling, color/indexed geometry, blending,
  pass/layer order, and supported effects. Add only consumer-required neutral
  operations, with retained-data and backend-domain guarantees.
- [ ] Extend keyboard-only routing if required with pointer capture, modal priority,
  propagation, and controller ownership; preserve raw record order/immutability.
- [ ] Add neutral clipboard/cursor/per-window DPI or scale-change/text-input services
  with capability reporting; do not expose GLFW handles to adapters.
- [ ] Establish independent optional integration targets through U7. Implement a
  representative scrollable/text/image UI through generic facilities.
- [ ] Probe concurrent frames, focus changes, resize/scaling, resource teardown, and
  a second independent consumer before claiming multi-library extensibility.

**Acceptance:** The adapter does not issue graphics calls or consult GLFW/Gainput
from simulation; it uses existing capture/frame facilities and required additions.
**Discovery boundary:** Every abstraction bypass becomes a specific engine-boundary
finding. Optional stencil, filters, render targets, IME, or a second library is
scheduled only if consumer requirements justify it; avoid universal widget APIs.

### U10. Add deterministic tile selection — T10

Prerequisites: stable metadata contract; no dependency on UI or batching.

- [ ] Define neighbor sampler, edge/unknown terrain handling, Wang/bitmask mapping,
  weighted candidate policy, seeded randomness, and missing-signature result.
- [ ] Implement selection independently of a particular world/entity representation.
- [ ] Resolve animated targets using simulation-owned elapsed time and explicit
  animation phase semantics, then submit ordinary retained packets.
- [ ] Plan boundary/missing-rule, deterministic weights, timing, and manifest checks.

**Acceptance:** Identical world samples, seed, and simulation time produce identical
resolved tile results without graphics-thread world access.
**Discovery boundary:** Unknown artwork action/facing/layout data remains blocked
on metadata listed in asset-manifest-todo.md; do not infer semantics from dimensions.
If world connectivity semantics are missing, decide them before a map API spreads.

### U11. Add Unicode text layout and glyph resources — T12

Prerequisites: consumer scope from U0/U9, resource requirements from U8.

- [ ] Define input encoding/error replacement, scalar versus cluster indices,
  fallback fonts, shaping/bidi scope, line breaking, and layout result ownership.
- [ ] Select the shaping/rasterization integration only after those requirements;
  distinguish atlas generation from text shaping and application editing.
- [ ] Define glyph IDs/metrics/runs, dynamic atlas growth or replacement, retained
  frame generations, and platform upload dispatch.
- [ ] Retain ASCII compatibility; route Unicode callers through decoded/shaped runs.
- [ ] Plan multilingual/fallback/invalid UTF-8/cluster and retained-atlas acceptance;
  IME/preedit and grapheme-aware editing require their own consumer contracts.

**Acceptance:** Multi-byte text is not rendered as one fallback per byte, and glyph
runs remain valid across in-flight rendering and atlas changes.
**Discovery boundary:** If editing/IME requirements affect cluster indices or platform
services, settle them before publishing the layout API. A larger STB array alone
cannot close this TODO.

### U12. Add measured optimization facilities — T9, T11

Prerequisites: U5/U6 metrics; U9/U11 semantics if those consumers are in scope.

- [ ] Collect representative update, draw/state-switch, publication, backlog, and
  workload data without verbose per-frame logging.
- [ ] Define reorder-safe regions and compatibility keys from retained resources,
  parameters, image units, geometry ranges/topology, and all relevant state.
- [ ] Keep authored order as default; batch compatible contiguous work first.
  Implement sorting only within explicitly reorder-safe regions.
- [ ] Define an advisory timing report/configurer that explains recommendations and
  leaves selected fixed-step/recovery/input contracts unchanged until opted in.
- [ ] Plan visual/order/material-generation acceptance and comparative measurements;
  a capability key does not prove a batching speedup.

**Acceptance:** Optimized rendering preserves authored semantics, and timing
suggestions are based on evidence without automatic policy substitution.
**Discovery boundary:** If profiles show another bottleneck, defer batching. If
clipping or atlas changes expand compatibility keys, resolve those before caching
or sorting keys across frames. Commit renderer and timing facilities separately.

### U13. Add typed events without changing delivery ownership — T8

- [ ] Define channel/type identity, payload ownership, type mismatch policy, and
  compatibility with existing named/any users.
- [ ] Add typed registration/submission with compile-time payload constraints.
- [ ] Reuse persistent registrations, invalidation gates, waits, platform/simulation
  delivery, worker FIFO streams, and required queued error handlers.
- [ ] Migrate a small representative native/engine channel; retain existing APIs
  according to their architectural contract.

**Acceptance:** Typed payload errors cannot reach callbacks as unchecked any casts;
existing cancellation and shutdown behavior is preserved.
**Discovery boundary:** If external plugins need serialization or versioned schemas,
that is a separate protocol concern. Do not entangle typed dispatch with networking
or replace the established event lifetime model.

### U14. Scope optional engine expansion explicitly

- [ ] Decide whether 3D mesh/scene submission is intended now; document the current
  placeholder and define CPU mesh, material, resource, and retained draw contracts
  before implementation.
- [ ] Decide whether cache-domain/NUMA adapters are needed; retain explicit unsupported
  capability/required-policy rejection until real adapters and host checks exist.
- [ ] Record audio, networking, world/entity/physics, serialization, and alternative
  render backends as separate roadmaps driven by application requirements.
- [ ] Keep device-loss recovery, physical GPU/OS input fidelity, platform portability,
  and sanitizer/Wayland coverage in the evidence backlog.

**Acceptance:** Optional work has a named consumer and bounded scope, or is explicitly
deferred; placeholders do not imply supported facilities.
**Discovery boundary:** A required application facility promotes its foundational
contracts earlier in the plan before dependent game code accumulates.

### U15. Complete in-file documentation and reconcile the roadmap

- [ ] Apply the in-file documentation inventory, incorporating contracts settled in
  preceding units. Useful contract comments accompany each unit rather than wait
  until the end.
- [ ] Explain ownership, affinity, capability limits, failure propagation, numeric
  units, and borrowed-view lifetime at public declarations.
- [ ] Correct stale local comments/examples; retain architecture links for broader
  rationale and avoid cosmetic reformatting.
- [ ] Update todo.md and the UI plan to reflect completed work, deferred choices,
  new required prerequisites, and metadata-owner dependencies.
- [ ] Reconcile each T1–T13 and review finding with a completed unit or explicit
  remaining task. Do not delete unresolved comments merely because a plan exists.

**Acceptance:** Source callers can determine supported behavior without reading the
whole repository, and planning documents agree with implemented contracts.
**Discovery boundary:** Documentation that cannot state a guarantee exposes a
contract task. Record it, classify whether it blocks correctness, and resolve it at
the next safe development boundary rather than inventing a guarantee.

## Sequencing, commits, and validation

Recommended progression:

1. U0 and U1 resolve scope and initialization; U2 resolves memory ownership and
   concurrency. Independent U3 fixes can proceed as their format choices settle.
2. U4 establishes safe failure reporting; U5 configures logging; U6 integrates it.
   U7 independently establishes consumer packaging.
3. U8 resolves resource requirements, then U9 proves UI-facing boundaries. U10 tile
   selection and U13 typed events can proceed independently on stable contracts.
4. U11 supplies Unicode layout once consumers/resource requirements are known;
   U12 follows measured workloads and settled ordering semantics.
5. U14 remains explicitly optional. U15 accompanies each unit and closes the review.

Commit each coherent unit or subunit independently, with the required Adds/Updates/
Revises/Deletes/Fixes prefix and per-command cppcooper identity. Do not squash the
series, push, include unrelated user work, or implement speculative facilities
because they are adjacent to another change. The review document and documentation index are recorded as the initial planning
unit before implementation.

Future verification should pair each unit with the smallest meaningful contract
cases, then use existing aggregate and native acceptance where the change affects
runtime/backend behavior. Consumer linking/header checks, sanitizer interleavings,
font fixtures, native callback failure, and logging saturation need distinct
checks; aggregate counts alone do not cover them. Build/test execution requires an
explicit request. Documentation-only validation consists of source-reference,
TODO-count, link, and diff checks and does not require compilation.

At every discovery boundary, preserve shared working-tree changes, record the new
finding and its dependencies, revise remaining tasks, and stop dependent work if
it would require treating unfinished user code as a stable foundation. Continue
only clearly independent work. A gate is passed by an explicit contract and its
appropriate evidence, not by the absence of immediate callers or failures.

## Implementation progress

### Initial development slice — 3 October 2026

The first slice resolves singleton publication/initialization and byte-manager
release ownership before expanding allocator consumers. Independent byte/statistics
fixes may follow. UI, shaping, multiple resource domains, and topology remain
consumer-scoping decisions at their existing boundaries. No GUI dependency or
additional supported platform is selected by this slice.

1. Record the reviewed plan and current execution constraints (preparation for U0;
   the full U0 checklist was not completed in this first slice).
2. Trace and remove the stale exception comment independently; keep allocation,
   noexcept, and trace-overflow work in U4.
3. Resolve singleton publication, private-constructor access, explicit configuration
   rejection, retry after failed construction, and caller ownership documentation
   (U1). Add focused regression sources without compiling or running them.
4. Audit memory transactions and static object-construction bookkeeping (U2).
   Split safe retained release ownership from transaction repairs if the audit
   exposes a larger correctness prerequisite. Do not claim concurrency support
   until the complete transaction contract is established.
5. Complete the independent byte/statistics corrections (U3) only after their
   observable output contract is stated; record any additional required work.

Each implementation unit records its source review and unexecuted acceptance work.
Consumer-specific or unavailable legacy-file-format decisions remain explicit
boundaries instead of speculative implementation.

### Stale exception comment — complete

Whitespace-insensitive blame traces the comment to `52b19bdb`, the original
exception files added on 25 September 2024. The header at that commit has no
matching function prototype beneath the comment; its implementation already uses
thread-local trace buffers and returns an owned std::string. The stale comment is
removed. The available history does not establish that a removed prototype was
its original referent. This does not close U4's allocation/noexcept or bounded
trace-formatting work. Validation: history/source inspection and diff checks;
no build or tests were needed for the comment removal.

### U1 — implemented; execution checks pending

Singleton_CTS and Singleton_CTU now publish completed construction through an
acquire/release atomic pointer. Nonconstructing get/get_existing calls no longer
read a unique_ptr concurrently with construction; they reject/return null until
publication. Explicit initialize(args...) accepts one successful configuration and
rejects repeats, including competing calls. Constructors that throw permit retry.
CTU checks and constructs in its befriended context, so private constructors work;
argument forwarding also accepts move-only configuration.

Compatibility get(args...) still selects the first successful constructor and
ignores later arguments. Its documented restriction is to configure argument-bearing
instances on their owner before starting producers, using initialize when repeated
configuration should reject. Loader retains its existing root-validation wrapper
and owned instances; FFont and logger declarations describe explicit setup. Default
cache/memory/pool/EventSystem/input/logger construction remains lazy. Later Type
operations and static teardown need their own ownership/quiescence contract; an
atomic published pointer is not a teardown lifetime pin.

Added regression sources for explicit/repeated/competing initialization, pending
publication, failed-constructor retry, private/default CTU constructors, and move-only
arguments. Source/diff review only; compilation, regression execution, and sanitizer
checks are pending authorization. U4 remains responsible for exception construction
failure safety; U5 remains responsible for complete logger bootstrap/lifecycle.

### U2a — retained byte release implemented; execution checks pending

Manager now supplies a retained ByteReleaseContext over the existing shared void
bookkeeping. Public return_chunk delegates to it; managed-block and object backing
deleters capture that context instead of a weak token plus raw manager pointer.
Known-owned final releases are noexcept; invalid bookkeeping or an allocation
failure during return terminates, consistent with the existing trusted object
release contract. The legacy lifetime_token interface remains a liveness observation
and does not authorize manager access during destruction.

This removes T4/T6's raw-facade lifetime dependency without claiming general
concurrent manager operation. Regression sources cover a retained context after
facade destruction, duplicate public return, handle release after destruction,
and a single release overlapping facade teardown. Checks performed: source ownership
trace, absence of raw-manager captures in both deleters, and diff checks. Build,
regression execution, and sanitizer evidence remain pending.

### U2b — required transaction and construction-state work discovered

The audit confirms these prerequisites before any concurrency promise or expanded
allocator use:

- All Manager growth-policy specializations share BlockManagement<void>::State.
  A per-facade mutex cannot serialize the byte domain. Define the operation gate
  on the shared domain and preserve read-snapshot consistency across collections.
- Checkout, partial return, whole return, merge, and stale/cull operations inspect
  and edit multiple separately locked collections. Define transaction invariants,
  failure rollback, and reentrant operation rules before replacing these paths.
- release_culled clears final owner records while collection locks are held. Object
  backing cleanup can invoke T destructors and then byte release. Detach retired
  owners under locks and destroy them outside both typed and byte operation gates;
  define lock order and prevent reverse acquisition through callbacks.
- ObjCtor<T>::constructed is a shared unsynchronized unordered_map. Its lifetime
  and construction/destruction synchronization must outlive backing-handle cleanup.
  User constructors/destructors must not run under its metadata lock.
- Allocation failures during multi-container insertion can leave a partially
  transitioned partition. Transaction work must include rollback or prepared
  node ownership, not merely an outer mutex.

These are required U2 subtasks. U2a's completed ownership contract is stable input
to them; T7 stays open. Independent U3 numeric/statistics corrections can proceed
without assuming these transactions are complete.

### U3a — byte formatting and empty statistics implemented

human_readable chooses its unit from the integer byte count (including values
just below a large power that round upward when converted to double), promotes
exact powers of 1024, includes TiB through YiB, and preserves one-decimal formatting.
A value just below a unit boundary may round to 1024.0 in the lower unit. Boundary
regression sources now cover representable adjacent powers and large suffixes;
the deferred math test TODO is closed.

Memory stats reports `not in use: n/a (no allocations)` when the shared byte domain
has zero total allocation, avoiding division by zero and an invented utilization
percentage. Nonempty-domain output keeps its previous percentage format. This
small diagnostic fix does not assume U2b's transaction audit has been completed.

Validation: source arithmetic/format review, reference and diff checks. Regression
sources are uncompiled/unexecuted. An isolated empty-domain stats execution check
is still needed when authorized; ordinary manager instances share global bookkeeping,
so creating a fresh facade alone does not establish an empty test domain.

### U0 — planning baseline completed after sequencing correction

The initial plan-recording commit was incorrectly treated as sufficient for U0;
it did not complete U0's classification, roadmap reconciliation, or concrete
consumer/platform scope before U1 implementation. This correction completes those
planning deliverables in the U0 section and records the existing UI foundations in
the UI strategy. The first consumers are the current native demo, portable recording
graph, independent library-link consumer, and a scoped neutral UI boundary probe.
Widget toolkit/version selection remains an explicit boundary before adapter-specific
work, with TGUI retained as the existing strategy's tentative first adapter.

U0's four checklist entries are now checked for the actual planning work delivered,
not for future builds/tests or full UI implementation. Historical validation is
unchanged. This correction changes planning documents only; local references,
links/anchors, checklist completeness, and diffs are statically checked. No build,
compilation, test execution, or new implementation work is performed in this unit.

### U1–U4 continuation — implementation sequence

U1's existing source implementation is retained after review. Its checklist now
records the completed audit/design/documentation/regression-source work; executable
acceptance remains pending. The audited consumers are: owned/legacy Loader (immutable
root plus root-checking wrapper), FFont (explicit data initialization), Log/Logger
(custom handlers before writers, otherwise lazy defaults), default asset caches,
EventSystem, input adapter, byte managers, object pools, and the spdlog pool initializer.
None requires reinitializing the generic singleton. Provider teardown uses noncreating
get_existing reads under its existing loading-owner domain. Type operations retain
their independent affinity/synchronization rules. CTS and CTU have separate storage,
private-constructor access and retry paths; new code must not treat atomic publication
as ownership during static destruction.

The remaining implementation is ordered as follows:

1. U2: stage only changed bookkeeping nodes while locking the shared domain's
   collections as a unit; transfer prepared nodes without allocation at commit.
   Allocate backing owners outside collection locks, and detach culled owners
   before invoking their destructors. Preserve public/protected lookup and return
   interfaces, with legacy raw collection edits requiring external quiescence.
2. U2: retain and synchronize object-construction tracking independently of static
   teardown. Different slots may progress concurrently; construction/destruction
   of the same slot is explicitly rejected, and user code executes outside the
   tracking lock. Backing owners retain the exact tracking context they use.
3. U3: complete numeric inclusion/linkage and full-consumption contracts. Preserve
   legacy FFont encoding and reject incomplete reads. The subsequent owner decision
   in U3d replaces further format recovery with class deprecation.
4. U4: capture native resize callback failures and report them at normal platform
   operations, then make trace/exception reporting bounded and nonthrowing under
   allocation failure. Preserve the first runtime error and report distinct
   secondary cleanup outcomes through an allocation-independent fallback.

Each coherent change is committed separately. No compiler/configuration probes,
product tests, or remote pushes are performed without the requested authorization.

### U2b — atomic block transitions and cull retirement

Implemented the shared transaction path for byte and typed checkout, whole and
partial returns, and inherited merge/fill/cull methods. Transactions acquire all
five collection locks with the deadlock-avoiding standard locking algorithm.
Replacement nodes and hash capacity are prepared before live records change;
commit transfers nodes without allocating. Allocation failure during preparation
therefore preserves the logical partition. Preallocation commits individual owners.
Backing allocation, final retired-owner destruction, and diagnostic formatting run
outside collection locks. Statistics now copy one jointly locked snapshot.

Protected lookup/fill and raw static collection interfaces remain available. They
retain their individual operation purpose; custom derived compound operations must
use the transaction mechanism or externally quiesce the domain. Facade destruction
requires its direct callers to finish; already captured release contexts operate
without touching it. Synchronization does not authorize concurrent access to the
same checked-out bytes or objects.

The transition audit also exposed overflowing growth/alignments and typed
length-times-size multiplication. Added representability checks required before
allocation/splitting. This is required request validation within U2, rather than an
extension of the allocator interfaces. Greedy growth conservatively rejects a
scaled value at the maximum size boundary before floating-to-integer conversion.

Added source cases for concurrent split/return/cull/statistics, unrepresentable
requests, and a final backing deleter that reacquires the same collection locks.
Existing seeded partition, cull cancellation, duplicate return, typed reservation,
and constructor-failure cases remain applicable. No compilation or tests were run.
The missing isolated allocation-failure fixture must still exercise set-node/hash
capacity preparation when executable acceptance is authorized; the structural
prepare-before-commit audit does not substitute for that acceptance case.

**Next boundary:** ObjCtor still shares an unsynchronized static map and can retain
an iterator across reentrant user construction. That required repair remains U2c;
block transactions alone do not complete U2's object lifetime contract.

### U2c — retained object construction tracking

Replaced ObjCtor's protected raw boolean map with a retained, synchronized Context.
The raw/constructing/live/destroying phases claim a slot before user code and reject
same-slot overlap. Constructors and destructors run outside the mutex; tracking
publication reacquires by address instead of retaining an iterator across reentrant
map growth. Tracking allocation precedes construction, and failed construction
returns that slot to raw state. Batch semantics preserve earlier successful slots
for the owning caller to clean up. Erase rejects a live/busy range without deleting
any of its records.

This is an explicit internal tracking contract change: unrestricted protected map
writes cannot provide synchronized claims or retained shutdown lifetime. Public
static construct/destroy/erase entry points remain. PoolState, its element handles,
backing owners, allocator-aware operations, and legacy AssetMgr handles now retain
and use the exact construction context during final cleanup. No static lookup is
required after those owners are created. ObjectReservation's independently tracked
claims keep their existing per-handle construction/destruction contract.

Added source cases for distinct-slot concurrency, busy-slot rejection, and map
growth from reentrant construction. Existing constructor failure, unclaimed ranges,
and facade-lifetime cases remain relevant. U2's source tasks are implemented; its
allocation-failure injection, executable concurrency, and sanitizer acceptance are
still unexecuted. Static diff/call-site review was performed; builds/tests were not
authorized. The installed formatter cannot read the repository's clang-format 23+
configuration, so changed code was matched to the existing style manually.

### U3b — complete-token numeric parsing and header linkage

Removed the unmatched numeric-header endif and made parse_floats inline. Integer
and floating parsers now require full consumption, including rejection of trailing
space/data and embedded NULs. Syntax errors remain invalid_args; representability
errors are bad_request, including integer overflow previously folded into syntax.

Preserved parser selection and useful existing syntax rather than introducing a
new grammar: integer input is base-10 from_chars, with no leading whitespace/plus
and a minus only for signed parsing. Floating input follows stod's leading-space,
sign, locale, and special-value rules, but rejects any unconsumed suffix. Automatic
selection still chooses floating parsing only for a '.', 'e', or 'E' marker; it
does not add base detection or special-value classification. The in-file contract
states those differences.

Added independent first-include numeric regression sources, a second translation
unit for linkage, width/range boundaries, and malformed complete-token cases.
Static checks only; compilation, linkage execution, and tests remain unexecuted.

### U3c — safe legacy reads; semantic format discovery boundary

Followed FFont path history back to `0329cff` (September 25, 2024). That first
loader already read 256 native shorts and looked up whitefont.png. No tracked
original widths asset, writer, or format specification was found. The current
native acceptance fixture serializes the same native-short array with widths
128 and 64; it proves a useful current consumer, not a universal width bound or
portable encoding.

Changed opening to binary input-only and reject any incomplete/failed full-array
read before geometry allocation, cache lookup, or provider upload. Added missing,
empty, short, and one-byte-truncated source cases asserting zero uploads. Existing
successful native fixture semantics are preserved, including ignored trailing data.
No builds/tests were run.

**Historical stop at the U3 format boundary, superseded by U3d:** endian/portable
integer representation, allowed widths, exact-size/trailing-data rules, and original
atlas metadata needed an authoritative fixture/writer or a chosen versioned replacement format. They were
not inferred from the synthetic test. [legacy-ffont.md](../resources/legacy-ffont.md)
records the evidence. The later owner decision deprecates the class and removes
semantic format recovery from required implementation; it does not validate the
legacy encoding or reconstruct its missing artwork.

### U4a — deferred resize failures and emergency reporting contract

Added iWindow::check_native_failure() as a platform-owner boundary with a default
no-op for adapters without throwing native callbacks. GLFW window callbacks are
noexcept and retain the first resize failure, including allocation/dispatch errors.
Pending failure suppresses further listener work while dimensions continue to track
native updates. The check consumes/rethrows through ordinary C++ calls: GLFW input
update before publication, runtime after any input adapter pumps, should_close, and
explicit resize/mode entry and completion. Window destruction reports an unconsumed
failure without throwing. The new virtual boundary changes the window ABI; existing
source adapters retain default behavior, and consumers must rebuild together.

Introduced logger-independent emergency report/preserve helpers. They use a bounded
1024-byte stack record and C stdio, with no C++ allocation, formatter, symbolizer,
listener callbacks, or logger initialization. Output is best effort; truncation or
I/O failure cannot replace the original exception. Reporting belongs outside locks.
This is the fallback contract U5 can use for logger/destructor/allocation failures;
it does not promise reliable delivery under process termination or a broken stderr.
Input destruction now reports its caught cleanup exception instead of discarding it.

Added a native source case invoking the actual registered resize callback with a
throwing listener, checking first-failure retention, size updates, consume-once,
explicit resize, and input-update reporting. Existing callback registration is
restored and the persistent event registration is removed at scope exit. No native
execution, builds, or tests were run. Exception/trace formatting and runtime secondary
cleanup reporting remain the next U4 units.

### U4b — bounded traces and nonthrowing exception diagnostics

Both exception and logger traces now write into a fixed diagnostic stream buffer,
reserve a terminator byte, mark truncated text, copy only the written prefix, and
reset buffer/stream state before reuse. Symbol resolution still uses backward and
may allocate. The public stack_trace call catches failures and returns an empty
string without allocation; callers treat an empty trace as unavailable information.

Exception trace evaluation and formatting now occur inside the same catch boundary.
The previous noexcept helper evaluated an allocating trace before its guard could
run and then called allocating formatting without any guard. Every generated error
now prepares owned bounded category/location/line/info first, attempts full trace
and formatting, and preserves that fallback on failure. Exception copies also keep
that fallback if copying the full string fails. Constructor argument expressions
outside these functions and the language runtime's allocation for a thrown object
remain outside this guarantee.

**Contract decision:** use the same owned string plus 512-byte fallback storage on
all compilers, replacing the MSVC-specific allocating std::exception message path.
Derived constructor signatures and catch types remain; the base layout and trace
noexcept contract change, so all consumers must rebuild together. The fallback is
an owned terminated prefix, never a borrowed temporary or recursively constructed
exception. Reporting it requires neither the logger nor trace resolution.

Added bounded overflow/reuse, owned fallback copy/move/truncation, repeated trace,
and emergency-output regression sources. An isolated allocation-exhaustion harness
is still required to verify actual trace/format/copy failures when execution is
authorized; directly exercising fallback storage does not claim that coverage.
Static checks only; no compilation or tests were run.

### U4c — cleanup phase reporting and final callback audit

Runtime sequential/concurrent/unstarted shutdown and dependency pumping now preserve
the first exception and report each distinct later exception with its cleanup phase.
Simulation-worker closure reports outside the scheduler mutex, and merging worker
failure into platform failure also happens after releasing that mutex. Pending window
failures are checked after pumping even when stop was requested, and once more during
cleanup. Input detachment reports any callback failure left unconsumed behind a window
failure. Marked the existing guarded input callbacks noexcept.

The native callback audit also found the GLAD procedure-address callback could let a
context adapter exception unwind into C. It now captures first failure and returns
null; renderer startup rethrows that failure after loading, before considering the
unsupported-version diagnostic. Its later context-release failure reports separately.
Added a source case for procedure lookup plus release failure; the existing generated
loader source was inspected to establish its first-null lookup behavior and restore
the only affected process-wide function pointer in that fixture.

Logger/renderer destruction, logger registry cleanup, owned byte/object release,
object backing cleanup, and EngineContext destruction use the emergency reporter
for caught failures. Byte final release goes directly through the retained transaction
path even for invalid bookkeeping, avoiding ordinary logger lookup at teardown.
The existing terminate policy for a failed trusted release or unsettled owned workers
is preserved, with a bounded diagnostic emitted first. Native/user destructors that
violate noexcept can still terminate before a surrounding guard can run.

Added source checks for primary-plus-cleanup phase output in both runtime modes,
deferred native failure during a stopping pump, bounded emergency records, and
unchanged primary exception identity. Generated Cheryl errors expose their bounded
cause/location summary for emergency output instead of the trace prefix. The full
contract is in [failure-reporting.md](../runtime/failure-reporting.md).

**U1–U4 status:** U1, U2, U3, and U4 source/planning tasks are implemented. U3d resolves
the legacy-font decision through deprecation; semantic format recovery is no longer
required work. Checked boxes record source/planning completion, not executable
acceptance. New regression sources have not been compiled or run. Required outstanding
acceptance includes isolated allocation-failure/exhaustion cases, the empty shared byte
domain case, multiple-translation-unit linkage, native callback/startup cases, and the
concurrency/sanitizer matrix established in U0. Do not begin dependent U5/U7/U9 work by
treating those unexecuted checks as passing evidence.

Discovered U5 follow-up: repeated distinct exceptions from a failing shutdown maintenance
pump can emit repeated emergency records. Keep bounded emergency output independent of
ordinary logging thresholds; decide aggregation/rate policy in the logging unit without
silently discarding a new phase or cause.

### U3d — deprecate FFont; preserve legacy interfaces

The owner identified the original Bitbucket fontMetrics.dat, confirmed that its
corresponding atlas is unavailable, and chose class deprecation. Retrieved the file
at Bitbucket commit `5c28678cb0bc425bad51ff39d04e53cc104bb50f`: it is 512 bytes;
interpreted as little-endian signed 16-bit widths, values range from 0 to 113.
This recovers fixture evidence but cannot supply the atlas or establish universal
format semantics. The source and checksum are recorded in
[legacy-ffont.md](../resources/legacy-ffont.md).

The coherent unit follows these tasks and boundaries:

1. Establish the supported font path before changing legacy contracts. STBFont
   already reads a caller-supplied font file and bakes its metrics/alpha atlas;
   system-font discovery is optional, and bundled fonts use the same loader.
2. Mark the class deprecated with a STBFont migration message. Preserve construction,
   singleton initialization, resource/layout interfaces, typed bank selection,
   native-short encoding, ignored trailing data, and complete-read rejection.
3. Document the missing atlas and differences in advances, line height, draw scale,
   and alternate-bank behavior. Close required format recovery through this explicit
   scope decision; do not mark semantic validation as implemented.
4. Retain regression sources and recorded historical acceptance. Statically inspect
   the attribute, contract references, and diff; compilation/tests remain unexecuted.

These tasks are implemented. Existing CMake flags suppress deprecated-declaration
warnings for GNU/Clang; the public attribute lets consumer warning policies diagnose
use. No warning-policy change is part of this unit. FFont remains usable with
compatible caller-supplied metrics/artwork, and its runtime behavior is unchanged.

**Discovery boundary:** Class removal and bank-interface migration need a separate
decision. Collection-face selection, size-aware caching, and Unicode shaping remain
with the font/resource consumer units; they do not require reconstructing FFont.
All U1–U4 executable acceptance remains pending authorization.

### U5 continuation — configuration before subsystem integration

The unattended work session continues with logging foundations. Earlier U1–U4
source work is retained; its unexecuted checks are not passing evidence. Builds,
compiler/configuration probes, tests, and remote pushes remain unauthorized.

The audit found a required ordering constraint: removing block.h's include-order
mask would activate stream construction and blocking logging in locked memory
lookups. That prerequisite must be repaired together with the mask policy.

1. U5a: establish one target-consistent compile profile, preserving the existing
   severity bits and explicit application overrides. Remove the block.h override
   and logging from memory leaf operations that can run under collection locks.
   Keep their lookup/split interfaces and expected-miss behavior. Add runtime
   filtering before stream/format argument evaluation, a guarded formatted entry
   point, and nonthrowing stream emission. Direct function arguments retain C++'s
   ordinary evaluation rules; document that distinction rather than promise that
   a function can prevent evaluation of its already supplied arguments.
2. U5b: specify runtime logger/file/console defaults and explicit configuration
   before publication. Preserve independent sink gates, reopen restoration, and
   the retained-file close boundary. Choose category records within the existing
   cheryl/engine/memory loggers; new subsystem fields do not imply more files.
3. U5c: establish Cheryl-owned async queue lifetime without replacing the host's
   global spdlog pool. Resolve saturation, drop counters, backend error reporting,
   and shutdown/reentrancy before using ordinary logging at engine boundaries.
   Native callbacks, memory locks, and noexcept cleanup retain U4's emergency path.
4. U6: integrate a bounded first slice only after the preceding contracts are
   stable. Prefer runtime/native ownership records and shader diagnostics from
   existing reflection data; do not duplicate caller-owned future failures.

Each completed subunit is committed independently with its acceptance sources
and static review recorded. Existing global includes and source-recompiling tests
remain U7 work; only compile-policy propagation needed by U5 is changed here.

**Discovery boundaries:** Optional destinations require a sound close-completion
contract before adding null sinks to the existing retained-file model. Queue loss
must not silently erase required failure reporting. A queue/embedded-host ownership
choice that cannot be justified by the current contracts stops dependent U6 work.
Subsystem integration must not rely on initialization, ordering, or callback
behavior that the logger does not actually guarantee.

### U5a — target-consistent compile policy and guarded emission

Added a public logging configuration target with developer/support/release compile
profiles and an explicit severity-mask override. The engine and source-recompiling
tests use the same definitions; consumers inherit the engine target's policy.
The legacy CTWriteMask alias remains supported target-wide. Policy is captured by
the header once, validated, and not rewritten by block.h.

Removed logging from memory split/neighbor helpers that can execute with collection
locks held. Their expected misses and byte-aligned remainders retain the same return
behavior without introducing stream allocation, lazy logger construction, or queue
waits. Lookup, split, and macro interfaces remain. U6 owns unlocked aggregate memory
diagnostics rather than warnings for ordinary misses.

Direct logging bodies and stack capture obey compile policy. should_log also checks
the actual sink gates, so argument preparation can be skipped when all destinations
reject a record. Existing streaming macros gate before constructing/inserting and
preserve an enclosing if/else. Guarded formatted macros/write_lazy keep initialization,
argument preparation, and submission inside a nonthrowing diagnostic boundary.
Direct function arguments still follow ordinary C++ evaluation rules; streaming
construction/caller insertion can throw. Stream destruction catches its own failures
and suppresses a partial record during argument unwinding. Removed the stream move
constructor's unsupported unconditional noexcept claim.

Added regression sources for compile-filtered/direct severities, filtered argument
side effects, independent sinks, first-include/mask consistency, lazy failures,
stream emission failure, partial streams, and macro if/else behavior. Their compile
and executable profile matrix remains unexecuted. The contract is in
[logging.md](../runtime/logging.md).

**Boundary retained:** The queue still blocks and runtime defaults are not yet
explicitly configured. U5b/U5c must settle those contracts before broad U6 logging.
No compiler/configuration probes, builds, or tests were run for this unit.

### U5b — explicit runtime presets and stable file configuration

LogConfig now supplies developer/support/release logger/file/console levels before
publication. Memory starts at INFO in developer/support and WARN in release;
statistics/TRACE require explicit filtering changes. Existing setters remain
independent and their current levels are restored after close/reopen.

Startup settings select directory, rotation bytes, backup count, and rotation on
open, retaining the prior logs/name.log, 10 MiB, five-backup defaults. Validate
levels/rotation/name before file or registry effects, and resolve the directory once
to an absolute normalized path. Reopening uses that path even after a working-directory
change. Existing handler-first constructors and singleton initialization remain
available with settings as the optional second argument. The immutable startup
settings getter is borrowed; all source consumers must rebuild for the Log layout
change. The legacy 16-bit log_id is not a globally unique session/correlation ID.

Added source cases for published presets, memory defaults, append/rotation settings,
reopen levels and stable paths, and invalid settings with zero file-open side effects.
Completed the explicit minimal/off-profile expectations in the existing destructor
acceptance source. Static review only; no builds, compiler probes, or tests were run.

**Discovery boundary:** File/console threshold off does not remove the file sink or
its startup effects. Console-only/custom sinks remain a separate ownership/completion
unit. Pattern overrides remain generation-local, while levels preserve their existing
restoration contract. Queue ownership, saturation, and backend failure containment
still gate dependent U6 integration.

### U5c1 — isolate and retain Cheryl's async pool

TPInit now constructs a private spdlog thread_pool with an 8192-item,
single-worker default instead of calling init_thread_pool and replacing the host's
global spdlog pool. Each Log retains that pool until its destructor's close/registry
work finishes. This removes the compatibility singleton's static-destruction order
as the pool lifetime relied on by live facades.

The bundled pool's constructor starts threads in a loop without joining previously
started workers if a later thread/vector allocation fails. Unwinding those joinable
thread objects can terminate before the logger's initialization guard can report
the failure. Selecting one worker avoids that partial-construction path and
parallel backend reordering. It does not claim measured throughput or impose order
on producers before submission. Multi-worker support requires exception-safe pool
construction and independent acceptance before configuration exposes it.

Native async loggers retain the pool weakly; accepted queue entries retain their
logger/sinks. Do not add a strong pool reference to a sink/queued logger: that can
cycle the queue owner and release its final ownership on a worker that would then
join itself. External logger owners extend sink lifetime but must stop producing
before facade teardown. A close timeout is not a bound on native I/O/user callbacks
or final pool thread joins.
Final facade destruction belongs outside the async backend; callbacks must not close,
destroy, or wait for their own logger/pool. Missing pool state rejects startup before
any file is opened rather than publishing an unusable logger.

Added a host-pool source case constructing TPInit directly, so it detects global
replacement even when the normal singleton was initialized by earlier cases.
Existing close/reopen/retained-owner cases remain relevant; isolated process/static
teardown and fault-injection acceptance are still unexecuted. Static checks only;
no compiler/configuration probes, builds, or tests were run.

**Boundary:** Queue overflow remains blocking. Saturation and backend callback/sink
failure policy must be settled before U6 adds ordinary logging to engine boundaries.

### Remaining U5 work — queue behavior and backend containment

The unattended session stopped at this design boundary. U5a/U5b/U5c1 are committed
source units, with static checks recorded; U5 and its executable acceptance are
not complete. Broad U6 integration has not started. The owner subsequently chose
to support both blocking and discarding, selected for each Log by its configuration.

**Queue decision:** Expose Block and DiscardNew as immutable initial settings.
Block remains the compatibility default for all existing logger presets until
their subsystem/error ownership audit justifies a different choice. DiscardNew
rejects the newly submitted item when full and counts the loss, preserving items
already accepted from any category. The existing shared spdlog pool supports both
policies per logger; a new queue implementation is not required. Required
terminal/cleanup failures retain U4's independent emergency report, even when
ordinary logs are filtered or lost. Discarding applies to ordinary ERROR records
too; required failure reporting is identified by ownership, not a severity label.
Avoiding the capacity wait is not a promise of allocation-free or lock-free
formatting, acquisition, or queue access. Normal-session saturation frequency has
not been measured; bursts, slow sinks, and blocked callbacks still need acceptance.

The source audit exposed the following implementation constraints:

- [file_helper.cpp](../../extern/spdlog/src/details/file_helper.cpp) invokes close
  handlers directly from close and destruction. A throwing before_close can prevent
  the physical close; a throwing after_close can prevent Cheryl's completion callback.
  These handlers need a nonthrowing boundary before destruction can be relied on.
- [logger.h](../../extern/spdlog/include/spdlog/logger.h) reports and rethrows unknown
  exceptions, including from async sink work. The bundled
  [worker loop](../../extern/spdlog/src/details/thread_pool.cpp) has no enclosing
  catch, so a non-standard sink/formatter/rotation-handler exception can escape
  its thread. A caller-side logging guard cannot contain that backend failure.
- Sink should_log/set_level are nonvirtual in
  [sink.h](../../extern/spdlog/include/spdlog/sinks/sink.h). A forwarding sink that
  always admits TRACE would invalidate U5a's actual-destination probe unless the
  owned graph/probe explicitly consults the delegate's current gate. Preserve
  independent destination levels and close/reopen restoration.
- A failed rotation can leave a closed file, for which file_helper::write returns
  without reporting. Catching an exception alone is insufficient: subsequent
  diagnostic loss needs an observable degraded state and a defined recovery boundary.
- [Async flush](../../extern/spdlog/src/async_logger.cpp) uses the same overflow policy
  as records. Changing that policy can discard flush requests too. Submission,
  completion, and physical durability must be specified separately.
- Backend callbacks can recursively submit through any category's shared queue or
  wait for their own sink completion. Blocking submission can deadlock the worker;
  lifecycle reentry can deadlock independently of saturation. Emergency reporting
  must not reenter the failed ordinary logger.

Continue in the following coherent units; each unit includes its contract/source
updates and its own commit. Execution remains subject to the existing authorization
restriction, and unexecuted cases do not establish acceptance.

1. **U5c2: expose per-Log policy with its submission and completion contracts.**
   Add validated Block/DiscardNew initial configuration, map to the existing spdlog
   policies, and retain the selection after close/reopen. Preserve Block defaults.
   Make shared queued/discarded item statistics observable without exposing a reset
   that would erase another category's loss. Ordinary flush uses that same policy
   and only submits a request. Required failures use independent reporting. Preserve
   strong close for accepted/retained owners; do not claim successful I/O or fsync durability
   from ownership completion. Determine the scope and lifetime of counters: the
   current pool is shared across categories, so its discard count is not a per-logger
   count. **Discovery boundary:** A requirement for guaranteed flush acknowledgement,
   per-category delivery guarantees, or independent queues requires a separate
   completion/queue design before changing the submission path.
2. **U5c3: contain owned backend and callback failures.** Wrap close handlers so each
   failure reports independently and completion can still run. Preserve the original
   startup failure when construction fails. Contain all exceptions from owned sink
   log/flush operations after delegate locks unwind, with bounded emergency reporting
   and visible failure counters. Define degraded-generation behavior and explicit
   close/reopen recovery, keeping the retained-file completion boundary. Preserve
   actual delegate filtering before argument preparation and level restoration.
   **Discovery boundary:** If a guard cannot preserve public sink ownership and
   filtering contracts, settle the owned graph/interface change before adding logs.
   Adding a strong pool reference to queued loggers/sinks is not a valid repair.
3. **U5c4: enforce reentrancy boundaries and complete queue loss reporting.** Detect
   backend recursive emission across the shared categories; suppress/count it through
   a nonrecursive reporting path. Reject forbidden close/wait/initialization reentry
   before acquiring lifecycle locks or recursively entering singleton initialization.
   Complete bounded loss/degradation summaries outside locks and final shutdown
   accounting from the shared statistics. Per-Log configuration can land separately
   using the existing backend, but new subsystem calls and changed category defaults
   must wait for these boundaries. **Discovery boundary:** A native-owner bypass
   or callback cycle that cannot be guarded without changing the public contract must
   be resolved before claiming deadlock containment or beginning U6.
4. **U5c5: establish acceptance before dependent integration.** Prepare deterministic
   cases for a held backend/full queue, discard counts and surviving record order,
   flush loss/completion, standard and non-standard sink/formatter/handler failures,
   callback reentry, failed startup retry, degraded close/reopen, retained owners,
   and final/static teardown. Use isolated processes for fatal-path/fault-injection
   cases. Run the compile-profile/disabled-argument matrix and relevant concurrency
   checks only when authorized. **Discovery boundary:** Failures revise the affected
   contract/unit before subsystem integration; throughput measurements cannot replace
   correctness acceptance.
5. **U6 first slice after those gates:** Use existing categories with operation/phase
   and outcome fields at runtime/native ownership boundaries. Audit primary-error
   ownership before each call site so future consumers do not receive duplicate
   diagnostics. Add shader reflection records from existing query data, then inspect
   normal-session noise before worker/event/resource integration. Stable correlation
   IDs and aggregate metrics are explicit prerequisites where needed, rather than
   reasons to repurpose the wrapping 16-bit log_id or log inside subsystem locks.

Console-only/custom destinations and multi-worker configuration remain separate
discovery units. Neither is required to add safe diagnostics to the currently
supported file-backed logger. The original U7–U15 order remains in place; independent
work should be selected only after checking its prerequisites and shared-tree state.

### U5c2 — per-Log saturation policy

Before source changes, this coherent unit was scoped as follows:

1. Validate a LogOverflowPolicy setting before file/registry effects; default to
   Block independently of compile/runtime severity profiles. Wire it into each
   native async logger, including generations created by reopen. Keep the shared
   8192-item/single-worker pool and already-accepted owner lifetime unchanged.
2. Expose copied shared queue statistics through Log and its compatibility facade.
   Include queued and discarded items, which cover flush requests as well as
   records; document independent sampling, lifetime, and cross-category scope.
3. Add acceptance sources that hold one backend operation, fill the shared queue,
   and submit through a second Log. Show DiscardNew returns and preserves accepted
   records while Block waits until capacity exists. Cover lost flush requests,
   invalid settings before effects, and policy retention after reopen. Release
   blocked workers/producers before any assertion can unwind into logger cleanup.
4. Update the contract and record static validation; compile and execution remain
   unauthorized. Commit this configuration/observation unit independently.

These source tasks are implemented. LogConfig appends the validated overflow field
after its existing members, preserving partial aggregate initialization order.
All presets keep Block. DiscardNew maps to the bundled backend's existing behavior,
without replacing accepted entries or allocating a second pool. The shared statistics
remain available to an owned closed Log; its facade keeps normal lazy initialization.
Configuration/template layouts have changed; source consumers must rebuild.

The saturation sources exercise two generations of DiscardNew, including lost flush
requests and ordered accepted output, and concurrent use of Block/DiscardNew through
one pool. Capacity submissions under observation run on joined producers; an incorrect
blocking mapping releases the held backend before producer/owner cleanup during a
fatal assertion. The blocking case uses a bounded scheduling observation. These
sources have not been compiled or run and do not replace the remaining isolated
process/fault-injection or concurrency acceptance. Static source/diff and local
documentation-link checks passed; no compiler/configuration probes, builds, or tests
were run.

**Discovery boundaries:** This unit does not establish backend exception/reentry
containment, flush acknowledgement, or durability. If sharing the pool prevents a
safe held-backend acceptance fixture, isolate its ownership without altering the
production pool contract. Per-category default changes remain part of the U6 error
ownership audit; saturation assumptions alone do not establish delivery requirements.

### U5–U8 continuation — implementation plan

The owner requested continuation from U5c3 through U8 on
`engine-foundations-and-consumer-facilities`. The initial working tree is clean.
Build, compiler/configuration probes, test execution, and remote push remain
unauthorized under AGENTS.md. Follow these units and commit each independently:

1. **U5c3:** Preserve the retained native file ownership boundary while placing
   exception guards around the owned backend destinations. Keep actual delegate
   filtering visible to lazy argument probes. Contain close callbacks individually,
   preserve startup exceptions, expose cumulative failure/degradation observations,
   and require completed close/reopen to recover failed destinations. Add focused
   acceptance sources and review destruction/rotation paths before proceeding.
2. **U5c4:** Guard shared-category backend/callback emission and lifecycle reentry
   before singleton or lifecycle locks. Expose suppression/loss observations and
   bounded summaries outside locks, including final pool accounting. Resolve any
   native-owner bypass discovered before claiming the boundary complete.
3. **U5c5:** Prepare isolated fault/reentry/teardown acceptance and the compile
   profile matrix. Executable acceptance is a prerequisite for U6; request explicit
   authorization once this work is concrete. Until acceptance passes, U6 stays
   pending while independent U7/U8 work proceeds.
4. **U6, after acceptance:** Audit error owners and add runtime/native diagnostics
   first, then shader records and worker/dispatch/event/asset/input/display and
   aggregate metrics in separate coherent units. Preserve emergency reporting and
   avoid ordinary writes inside callbacks, final deleters, or subsystem locks.
5. **U7:** Audit the real library's public/private dependencies and supported
   headers; define a stable build-tree consumer target and explicit packaging scope.
   Add an independent consumer and header/link acceptance sources. Make bootstrap
   ownership explicit rather than relying on static-library extraction. No broad
   CMake cleanup or unsupported platform claims.
6. **U8:** Audit provider/preparation/upload/cache publication and document the
   one-provider domain, ownership, partial publication, retry, and retirement at
   declarations. Apply U0's neutral-probe requirements: immutable replacement
   images and one domain; no automatic residency or speculative mutable resources.
   Record any correctness prerequisite before consumers depend on it.

Each unit includes source/diff and local documentation-link checks, plus an honest
record of unexecuted acceptance. Required discoveries revise the remaining plan;
unrelated findings are recorded for their appropriate later boundary.

### U5c3 — owned destination and close-callback containment

Implemented forwarding guards around the owned file/console destinations without
changing the retained native file completion signal. Actual destination gates and
degradation participate in argument-admission probes. All delegate log/flush
exceptions are contained after delegate locks unwind; a failed destination stops
receiving queued operations until completed close/reopen creates a new generation.
The healthy destination continues. Cumulative observations survive reopening.

Close callbacks are individually guarded, counted, and reported from bounded
slots after operations/destruction. Repeated same-slot failures before reporting
are coalesced. Physical close and both callback opportunities remain reachable;
opening exceptions retain their original type/value during failed construction.
Sources cover these contracts and standard/non-standard formatter failures.
Source/diff checks passed; no compilation or test execution was performed.

**Required U5c4 discovery:** Facade-only reentry guards cannot protect callers
holding a native spdlog logger: recursive submission can still fill/block the
shared queue. Resolve the published native submission path before U6. Arbitrary
replacement of the owned sink vector also bypasses both containment and actual
filtering. U5c4 must make this ownership restriction explicit and update acceptance
fixtures to hold owned formatters rather than inject unsupported destinations.

### U5c4 — guarded native submission and reentry/loss accounting

The native-owner discovery is resolved by publishing an spdlog-compatible guarded
frontend backed by a private async logger. Both retain the same guarded file and
console destinations; queued work still borrows the pool. Native submission and
cloning cannot bypass the shared-category callback check. The owned sink graph is
now fixed; unsupported graph changes reject submission. Acceptance fixtures hold
the owned file formatter instead of adding destinations. Native async_logger casts,
error-handler replacement, raw delegate emission, and singleton-base casts are not
part of the supported guarded interface.

Backend/file callbacks share a thread-local scope. Ordinary recursive writes are
suppressed/counted before facade initialization or argument preparation. Native and
facade flush/close/reopen/acquisition/initialization and guarded destination setters
reject before lifecycle/delegate locks. Final owned/native/file release inside a
callback is an explicitly fatal ownership violation; destruction cannot throw a
rejection or keep destroyed storage alive. Cross-thread user wait cycles remain
caller-owned and cannot be inferred from thread-local context.

Cumulative reentry/failure summaries and shared queue-loss ranges use bounded stdio
outside lifecycle locks; completed close and final pool release perform accounting.
flush retains its logger then releases the lifecycle lock before queue submission.
Sources cover native/facade recursion, disabled preparation, unopened categories,
startup reentry, fixed graphs, and guarded clones. Diff/source checks passed;
executable acceptance, isolated fatal cases, and compile profiles remain U5c5.

### U5c5 — isolated acceptance sources; execution gate pending

Prepared a standalone acceptance target and Python driver for previously built
binaries. Each fault/rotation/saturation/reentry/retention/static/fatal scenario
runs in its own process with a 30-second timeout. The driver also runs logging.*
regressions, checks surviving record order, verifies record/flush loss summaries,
and inspects the final static-teardown record. The disabled profile still exercises
native queue/backend behavior independently of compiled-out facade calls.
The normal/sandbox five-row compile-profile matrix is documented in
[logging-acceptance.md](../development/logging-acceptance.md).

**Required discovery repaired:** Registry initialization inside a singleton Log's
constructor can order registry destruction before the facade storage. Facade get
and initialize now establish registry lifetime before entering singleton storage;
supported static teardown closes logs before registry destruction. Explicit base
casts and external owners surviving the facade teardown remain unsupported.

Diff, Python syntax/static source, and local documentation-link checks are the
only permitted checks performed so far. U5 executable acceptance is **pending**;
U6 must not acquire a dependency on unaccepted logging containment. Continue U7
and U8 independently while requesting explicit execution authorization.

### Authorized focused execution and required compilation prerequisite

The owner explicitly authorized focused builds/tests and the normal/sandbox logging
profile matrix. This supersedes the earlier execution restriction for these checks
and necessary consumer/integration validation; remote push remains unauthorized.
The first sandbox developer-profile build on GCC 16.2.1/CMake 4.4.3 exposed a U3
legacy error message passing filesystem::path directly to C++23 std::format.
C++23 has no standard path formatter. Format its string representation instead;
this is a required compilation prerequisite, not legacy-format redesign.

Focused execution found two acceptance-fixture lifetime assumptions: a callback
looked up its own native registry entry after concurrent close had dropped it,
and a configuration case raised its async destination threshold before its earlier
INFO record completed. The fixtures now use a separately retained/open category
and completed close before changing the threshold. No production delivery/filter
guarantee is strengthened to accommodate either assumption. The profile runner
uses a focused cheryl-logging-tests target linked to the actual engine, including
the first-include policy source, rather than rebuilding unrelated test sources.

### U7 — build-tree consumer and explicit bootstrap

Selected build-tree composition, documented in
[consuming-engine.md](../development/consuming-engine.md). Cheryl::Engine carries
C++23, public headers, logging policy, and public/private dependency requirements.
Tests now link the actual archive instead of recompiling engine sources. GoogleTest
and demo targets are optional; Gainput/X11 remain normal-build requirements. Generic
entry points remain distinct from explicit native/umbrella headers. An installed
find_package distribution and unsupported platform combinations remain later work.

The archive excludes src/main.cpp. Cheryl::SignalHandlers explicitly owns the
legacy process bootstrap; only the demo opts in. Backward::Interface supplies stack
capture/resolver requirements without automatically linking a signal-handler object.

**Acceptance:** Independent Release consumer configure/build and execution passed
in sandbox and normal Linux/X11 configurations on GCC 16.2.1/CMake 4.4.3. The
consumer links only Cheryl::Engine across multiple translation units and compiles
ten sandbox/eleven normal first-include header probes, including the native input
entry point in normal mode. Generic translation units reject GL/GLFW header leakage.
Configuration used CMAKE_POLICY_VERSION_MINIMUM=3.5 for pinned legacy dependencies;
no vendor policy upgrade was made. Diff and local documentation-link checks passed.

### U8 — selected consumer resource and publication contracts

Audited preparation/upload/cache/native retirement and documented the contracts at
ResourceProvider, Image, Loader, TextureMgr, and AssetCacheContext declarations.
[consumer-resource-contract.md](../resources/consumer-resource-contract.md) maps
U0's neutral UI probe to one active provider/cache domain, owned CPU pixels, immutable
replacement handles, retained frames, and the existing ASCII atlas. No mutable
texture, multiple-domain, atomic batch reload, or automatic eviction API is needed
for this scope. Their accounting/admission/ownership prerequisites are recorded
before later consumers can require them.

Prepared data owns no native handles; owner/context-affine upload consumes its value
and can publish completed entries before failure. The metadata snapshot commits
only after upload and final allocation succeed; retry skips existing keys. Logical
retention preserves generations while the native domain is live, and permits safe
destruction/CPU inspection after shutdown rather than binding to a closed domain.
Pending requests cancel before execution; executing upload has no rollback contract.

**Acceptance:** 25 current sandbox developer-profile cases passed across
asset_preparation, resource_upload, asset_cache, material_cache, and
execution_shutdown. These cover owned preparation, partial publication/retry,
provider guards, retained generations, and cancellation/shutdown. Public header
compilation also passed as part of the independent normal/sandbox U7 consumers.
Diff and local documentation-link checks passed. These results establish the
selected existing contract, not native GPU budgets or unimplemented reload features.

### U5 executable gate and U6 implementation boundaries

All ten normal/sandbox rows passed: developer, support, release, release with
0x07, and explicit off. Each row runs logging.* and seven isolated process cases
with timeouts, record-order/file assertions, saturation loss checks, retained-owner
completion, fatal ownership rejection, and static teardown. The developer row
contains 29 passing regressions; stripped profiles retain their conditional checks
and explicit saturation cases. U5's executable gate is satisfied. A supplementary
Debug ASan/UBSan run is in progress; it is recorded separately from this matrix.

Use existing compile-time string categories and host-configurable lazy/explicit
Logger ownership. U6 adds process-local monotonic numeric domain IDs (never raw
addresses), operation/phase/outcome fields, and queryable cumulative snapshots.
Do not initialize extra per-subsystem log files or change host logger configuration.

Follow these coherent U6 slices:

1. Runtime/session and native lifetime records, terminal phase retention, shutdown
   summaries, and timing/poll/frame counters. Capture mutable phase metadata on
   each owning thread; report after locks/join, preserving original exceptions.
2. Worker/group and dispatcher/event IDs and snapshots. Known EngineContext
   construction holds its execution mutex across worker creation: worker constructors,
   group creation, shutdown, and final capture destructors must not log there.
   Report their snapshots from the context after unlock or an explicit unlocked
   observer. Futures/error handlers retain callback-error ownership.
3. Asset preparation/upload batch records and partial cache-publication counts;
   shader reflection records from existing query results; quiet expected memory
   misses with unlocked statistics/preallocation summaries.
4. Platform/input capability and focus/device/resize observations outside native
   callbacks. GLFW error callbacks use bounded emergency output and preserve the
   existing host callback. Native GL debug observation is optional/capability-gated;
   settle borrowed-context callback lifetime before installation. Prefer context
   message-log polling when it preserves host callback ownership and eliminates a
   dangling renderer user pointer on failed context recovery.

Acceptance includes focused existing runtime, worker, dispatch/event, resource,
memory, and recorded OpenGL cases plus diagnostic-specific quiet-session, partial
publication, identity/counter, and callback-owner checks. Do not claim native driver
or race-detector coverage from recorded calls alone. Independent U7/U8 are complete.
