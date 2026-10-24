# WORLD-U8 — checked allocator repair; benchmark BLOCKED

Starting EXACT clean reviewed feat/tilemap HEAD
`bb3a0421d4d6fdc7d1a609ada441a04282288b6c`. Committed U7 receipt ready/true hashes to
`f2476b81aac182ba570e86f16d59f4341a69631bc646798ad36800d60e6c46c1`, matching the
assignment manifest. Selection/review hashes and all10 nonempty U7 evidence logs
were verified. The review preserves U7's real170 CPU passes/routing but identifies
R1 comparison, R2 allocating return and R3 throwing conversion defects. This is one
standalone invocation, not an old queue or automatic continuation.

Implemented the ONE related repair first. Final accumulated178/178 passes retain
all170 U7 names, plus8 meaningful law/release/node cases. The requested locality
benchmark is NOT complete: no matched standard-vs-Engine trace implementation,
optimized Engine configuration, cold/warm measurement samples, per-slab occupancy /
upstream count comparison, p50/p95/max or cache evidence. No timing was attempted.
[WORLD-U8 receipt](continuation/WORLD-U8.json) is BLOCKED/checks_passed=false despite
passing repair checks. [Compact](decisions.md) and [detailed](agent-notes.md) views
match corrected WINT-019 and new WINT-022/023. Coordinator source review is pending.

## Proposed caller versus executed repair caller

```cpp
// Proposed benchmark notation only: NOT implemented or executed.
const auto trace = fixed_trace(512, fixed_seed); // hard live limit8192
run_trace<std::list<Record, std::allocator<Record>>>(trace.list, exact_list_sequence);
run_trace<std::list<Record, PoolAlloc<Record>>>(trace.list, exact_list_sequence);
run_trace<std::map<Key, Record, Compare, std::allocator<Pair>>>(trace.map, exact_ordered_pairs);
run_trace<std::map<Key, Record, Compare, PoolAlloc<Pair>>>(trace.map, exact_ordered_pairs);
```

Future finite trace would include ordered/shuffled insertion, complete scans, map
hit/miss queries, erase-half/refill, two churn rounds, sparse occupancy and cleanup;
7 fresh-process optimized samples and separately labelled within-process warm reuse.
Nothing in this proposal authorizes another invocation. No tuned policy/backend,
segmented list, general resource framework or automatic next job was added.

Executed real callers use unchanged std::list and std::map through actual retained
ObjectPoolAllocator/rebound class-node hooks. Literal list sequence inserts0..7,
erases evens, refills8/10/12/14 and independently checks1/3/5/7/8/10/12/14. Surviving
references stay valid across unrelated erasure/refill/swap; no erased iterator is
used. Copy/move/swap and construction/destruction balance. Actual extracted map
node survives source destruction, reinserts with a compatible allocator and later
survives another container until final typed-slot return. alignas(256) nontrivial
throwing list/map values demonstrate alignment, cleanup and later successful use.
These are conformance cases, NOT the fair full benchmark workload or locality proof.

## Corrected contracts

- R1: cross-type comparison follows shared-family rebinding law, not is_same_v.
  All pool comparisons are equal; this never permits returning a U pointer as T.
  Replace U7's wrong inequality assertion and independently check both directions,
  a==b versus a==A(b), b==a versus b==B(a), symmetry and round trips. Same-type
  contexts with different handle addresses still share actual typed state.
- R3: converting construction only copies a shared retained context-directory
  handle, noexcept. Destination Pool<T> is not initialized; actual first-use tests
  check get_existing()==nullptr before/after conversions. First allocate/context
  acquisition performs setup where failure is permitted. Valid allocation registers
  its return type BEFORE publishing storage; return does a find-only retained
  lookup. Same-type copies/moves remain usable and generic U subobjects retain
  explicit untracked construction, while whole T tracking is unchanged.
- R2: checkout prepares empty section/pool/timestamp node handles, bank vector
  capacity and timestamp buckets before storage publication. Compatible node
  transfers/recycling satisfy valid returns without fresh metadata. Keys in spare
  nodes are cleared, retaining no backing owners. Typed units cover all requested
  slots; byte units cover active sections, with extra low-level middle-fragment
  admission before mutation. Exact/greedy backing, contiguous n-object ranges,
  merging, partial typed returns and culling remain. Admission may retain increased
  EMPTY metadata capacity on failure, while published partitions stay unchanged.
- State metadata uses explicit new_delete_resource via PMR containers/banks;
  scoped borrowed resource must outlive State. No global PMR/new override or
  recursive Engine allocation. This is existing producer metadata, not a new
  storage backend. Source/layout changes require recompiling Engine/consumers;
  installed/static/other-library/platform/unload ABI is unverified.
- Allocator deallocate is noexcept for valid allocated ranges. Invalid allocator
  deallocation violates its precondition; checked invalid/double/out-of-bounds
  SERVICE returns still reject through PoolState/Manager/context APIs. No valid
  catch-and-leak/terminate workaround was added. Default/Object facade lifetime
  constraints remain; growing a retained pool still needs a live byte manager.
  Context directory/control costs and retained metadata are additional, not free.

## Actual commands and results

Every configure/build/native/format/symbol/loader check used
`/mnt/projects/cheryl-features/scripts/build --`, one job, preserved
build/tilemap-u1-debug and existing shared lock/admission controls. No resource,
auth, quota or memory refusal occurred. No downloads/installs/pin/global-config /
sibling changes. Full argv/cwd/environment/merged stdout+stderr/immediate exits /
elapsed times are in build/world-u8-logs/commands.jsonl and per-label logs/exits.
ASan/UBSan instrumentation and halt_on_error remain; only command-local
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and UBSAN_OPTIONS=halt_on_error=1. Leaks
remain UNVERIFIED. Tests are CPU-only; no display/X11 retry.

| Log under build/world-u8-logs | Actual result |
| --- | --- |
| preflight.json | Exact clean HEAD/manifest predecessor receipt/selection/review hash and10 nonempty prior evidence files verified |
| repair-build.log | Exit1: PMR vector tried uses_allocator construction of node_handle; full diagnostic retained |
| repair-build-node-holder.log | Exit0 after narrow node holder correction; Engine/memory/header probes built |
| release-build.log | Exit0; new actual metadata resource/refusal cases compiled |
| repair-tests.log / repair-results.xml | Preliminary45/45,0 failed/skipped,11.49s; separate from final campaign |
| repair-final-build.log | Exit1: strict world consumer rejected int-to-size_t sign conversion; full diagnostic retained |
| repair-build-signed-count.log | Exit0 with explicit size_t arithmetic, Werror unchanged; all affected CPU callers built |
| final-build.log | Exit0; final formatted memory/node cases compiled/linked |
| discovery.log / registration-correspondence.json |178 unique names, all170 U7 names preserved,8 new exact names listed |
| accumulated-tests.log / results.xml |178/178 ONCE,0 failed/skipped,116.49s;48 memory/resource/node cases +130 prior shared/map/editor/world cases |
| world-processes.json +16 copied child logs |16 successful actual child exits, including fresh literal/semantic/generated full-snapshot reloads |
| ownership.log + symbol/link/loader logs | Exit0; normal single clone-local shared Engine in memory/unit/world callers,17 actual prior export/import pairs and imported Engine diagnostics, no copied implementation archive |
| format-check.log | Exit0; all changed C++ headers/sources pass formatter |

Actual commands (all through wrapper/run.py, exact absolute JUnit path in JSONL):

```text
cmake --build build/tilemap-u1-debug --target tests-memory-containers tests-engine tests-world-consumer tests-terrain-consumer map_storage_consumer --parallel 1
ctest --test-dir build/tilemap-u1-debug --output-on-failure --no-tests=error -V -j 1 -R '^tests-memory-containers\.' --output-junit ABS/build/world-u8-logs/repair-results.xml
ctest --test-dir build/tilemap-u1-debug --output-on-failure --no-tests=error -V -j 1 -R '^(tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.|tests-terrain-consumer\.|map_storage\.consumer\.|world\.|tests-memory-containers\.)' --output-junit ABS/build/world-u8-logs/results.xml
python3 build/world-u8-logs/ownership.py
```

223 test executions cover178 unique names (45 preliminary +178 accumulated), not
one223-case invocation. No native test failed or skipped; initial COMPILE failures
remain recorded. Two unrelated unchanged dangling-else warnings remain; no warning
policy weakened. All170 U7 names are retained while one wrong assertion is corrected.
Existing original memory/block/reservation cases remain, plus affected real editor /
history/generated/frame consumers. Three allocator public-header probes rebuilt.
Graphics configuration/output is preserved, NOT rebuilt/run/accepted by U8. No
unchanged standalone storage/procedural or broad platform campaign was repeated.

## Measured versus unmeasured

The scoped resource observes ACTUAL block-producer collection/bucket/bank-vector
allocation requests, delegating successful requests to new_delete_resource. No
global interception. A private ActualPool only supplies its owned State; all
retrieve/return/construction/unwind algorithms are SAME inherited Engine producer
code, raw backing still from actual ObjectMemoryManager. Void test retains a real
Engine managed block. This is not denial of all process allocations: tracking,
shared-control objects, exception runtime and raw backing outside this resource
are excluded. Source review complements this precise oracle.

Nine typed slots return in literal fragmented order4,0,8,2,6,1,7,3,5, merging fully;
whole bytes, reuse, constructor unwind and final retained pool-owner release make
ZERO fresh requests while metadata refusal is active. All12 tested admission
failure positions refuse, preserving every old registry/section/free record, and
the earlier promised release still completes without a new request. Later valid
construction succeeds. Scoped resource bytes return to0 after State ownership ends.
The nine-slot case observes5344 peak requested metadata bytes, NOT RSS or per-
container/slab overhead. State/control/tracker/directory/backing/malloc overhead is
additional. U7's real workload still observes7 allocator call/return pairs and96
construction/destruction pairs per vector service; these are NOT upstream heap
counts or timing. The erased-slot locality benchmark remains unmeasured.

Default greedy pool behavior remains1.29*request+64*sizeof(actual rebound U), with
alignment rounding and shared byte reuse; no assumption of exactly64 slots and no
new geometry/tuning column. Metadata reservations can increase memory materially,
retain high-water banks after culling and change allocation-failure points. Actual
cold/warm slab fullness, upstream ownership/bytes, per-container retained storage,
standard-backend instrumentation and fair optimized timing are the exact unfinished
WORLD-U8 acceptance. No mixed Debug timing substitutes for that gate.

Logs are finite/fixed-count (currently about2 MiB, within an8 MiB handoff budget),
including complete failed diagnostics/raw selected evidence; no timing samples are
missing because none ran. tested-source-hashes.json matches final C++/CMake/cache
inputs. preservation.json verifies all29 older receipt/archive paths byte-identical,
including U7's immutable receipt; public storage/tilemap/schema/strict-v1 source is
unchanged. New public State/allocator layout is intentionally source-incompatible;
normal affected CPU Engine/callers rebuild together. Graphical/input/atlas upload /
teardown/visual/fun, leaks, packaging/other platforms and stronger file durability
remain open. This partial repair is not automatic coordinator source acceptance.

## Next action — idle with a blocked benchmark receipt

Coordinator reviews this focused R1/R2/R3 repair and its178-case/native producer
proof, especially metadata reservation invariants/context lifetime and cost. Only
a separately selected bounded assignment may implement the matched trace and fair
optimized slab/reuse measurement. Publisher alone backs up feat/tilemap. Worker2
stops after this checkpoint; no retry loop, queue extension, next controller,
sibling integration or automatic additional resume.

---

Historical U7/U6 handoffs follow unchanged except for their current namespace
entry points; they do not certify the unexecuted U8 benchmark.

# WORLD-U7 Engine allocator containers and retained editor workspace

Standalone implementation on EXACT clean reviewed feat/tilemap parent
`0654533c01f29058262ffdb7e4a26db5205515c0`. The committed U6 receipt was ready/true;
all12 nonempty saved logs and127 passing XML entries were inspected before edits.
The explicit user choice selects Engine memory/object services. Exact hooks and
private workspace policy are reversible engineering, not inferred API taste.
Implementation and selected CPU acceptance pass; ready for coordinator review.
Matching WINT-018/019/020/021 live in [compact](decisions.md) and
[detailed](agent-notes.md) views; [receipt](continuation/WORLD-U7.json) is standalone,
not an addition to either immutable completed queue. No sibling input is selected.

## Actual callers and ownership

```cpp
#include <cheryl/core/resources/allocators.h> // first header; normal Cheryl::Engine
using A = CE::Memory::Allocators::DefaultAllocator<int>;
std::vector<int, A> values;
values.reserve(32);
values.push_back(7);
values.clear(); // CE storage retained, lifetimes ended
```

Actual private editor input caller remains:

```cpp
DemoTilemapEditor editor;
editor.consume(std::array{press(Key::B), paint(first_pointer), paint(second_pointer)}, sizes);
const auto warm = editor.workspace(); // scalar observations + borrowed backing address
editor.consume(std::array{press(Key::T), erase(first_pointer)}, sizes);
// Same bounded CE-backed pending buffer; owned document/history/frame rules unchanged.
```

The SAME tilemap-editor.cpp compiles into real demo, unit and world consumers.
Records use their own admitted captured pointers; keyboard Press, wheel and middle
sample flush boundaries/order remain unchanged. Member pending TerrainEdits use
DefaultAllocator and lazy reserve of the configured request cap (default32, hard
1024), with actual capacity checked before adoption. Zero permits no edit/no
allocation;1025 config refuses. Clear at entry AND on all exits/unwinds prevents
stale samples after rejection or invalid sizes. Per-run request/affected/work and
history/generated overlay limits still apply. Completed earlier runs survive a
later tick error; the existing actual demo update diagnostics/drop/recovery path
is exercised by retained CPU cases, not by a new graphical run.

Primary std traits now honor CE rebind, converting construction, allocator copy
selection, equality/propagation and generic U construction. Positive addressable
counts are checked BEFORE checkout, <=PTRDIFF_MAX/sizeof(T); zero/excess throws
existing CE bad_request. Default exact and Object greedy byte policies remain.
Byte storage still requires users/deallocation before manager facade teardown.
The source-inspected app reset after simulation join meets that boundary; graphical
cleanup has not been executed. Whole byte-section returns and typed pool-slot
returns retain their separate semantics. No new global PMR/new/delete interception.

Pool SAME-T equality follows shared BlockManagement/Constructor domains, not
context address; different-T domains compare unequal. Explicit capture of a live
local Pool facade supports retained-context tests. Copies/moves keep source
allocators usable. T construction remains tracked through retained context;
different U node-value subobjects construct/destroy directly, explicitly UNTRACKED,
without inventing Pool<char/double> or retaining arbitrary tracker registries.
Growing a retained typed pool still needs its underlying byte-manager facade live;
retained construction/final return use their captured contexts. Raw allocation
never constructs objects. Existing tracked Factory/Reservation
semantics remain. User synchronization of contents remains independent.

Compatibility: no std::allocator base/upcast, no obsolete Pool Base alias, no
program-defined allocator_traits specializations/instance-free overloads. Eight
active production traits calls (Factory7, asset manager1) already pass instances;
none needed migration. New external calls use allocator/member/instanceful primary
traits; tracked batch operations explicitly use Constructor<T>. Templates require
recompilation; no installed/static/other-standard-library ABI promise follows.

## Executed verification

All native/configure/build/format/link probes ran through
`/mnt/projects/cheryl-features/scripts/build --`, one job, preserved clone-local
Debug configurations and existing lock/admission controls. No resource refusal,
install/download/pin/config/sibling change or display retry. Full exact argv,
cwd, sanitizer environment, immediate exits and elapsed time are saved in
`build/world-u7-logs/commands.jsonl`; run.py captures merged complete stdout/stderr.
ASan/UBSan remain instrumented with halt_on_error; invocation-local
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and UBSAN_OPTIONS=halt_on_error=1 only.
Leak checking stays UNVERIFIED.

| Check / log under build/world-u7-logs | Actual result |
| --- | --- |
| baseline-probes.log | Exit1: probe script's library glob used wrong Engine filename, before compilation; preserved |
| baseline-probes-fixed.log, baseline-run.log, baseline-string-compile.log | Probe driver0; Default/Object vector CE rebind0/domain0; expected string compile1: final allocator holder derivation error |
| memory-build.log | Exit1: missing CherylConsumers.cmake include; corrected locally, preserved |
| memory-build-fixed.log | Exit0; actual shared Engine, focused resource runner and three allocator header-first probes built |
| memory-tests.log / memory-results.xml |40/40,0 failed/skipped,8.49s;32 existing resource/block cases +8 new allocator cases |
| shared-build.log | Exit1: mistyped target tests-map-storage-consumer; no compilation; corrected to map_storage_consumer |
| shared-build-target-fixed.log | Exit0; tests-engine, tests-world-consumer, tests-terrain-consumer, map_storage_consumer built/linked |
| discovery.log / registration-correspondence.json |130 nonempty unique names, ALL127 U6 names retained plus3 scratch cases |
| shared-tests.log / results.xml |130/130 once,0 failed/skipped,104.05s; accumulated relevant selection only |
| world-processes.json +16 copied child logs |16 successful child exits0, including fresh full semantic/literal snapshot reloads |
| demo-build.log | Exit0; actual demo rebuilt/linked in preserved graphics output, no execution |
| corrected-probes.log | Exit0; same Default/Object vector rebind1/domain1; long string compile0 |
| ownership.log + detailed symbol/link/loader logs | Exit0;17 existing exports/imports, SAME three private app sources in unit/world/demo, original private cave producer, normal single shared Engine; memory consumer imports actual Engine next_domain_id and loads local libcheryl-engine.so.4 |
| format-check.log | Exit0; all changed C++ sources/headers pass formatter |

Actual selected commands (each prefixed by scripts/build -- through run.py):

```text
cmake --build build/tilemap-u1-debug --target tests-memory-containers --parallel 1
ctest --test-dir build/tilemap-u1-debug --output-on-failure --no-tests=error -V -j 1 -R '^tests-memory-containers\.' --output-junit ABS/build/world-u7-logs/memory-results.xml
cmake --build build/tilemap-u1-debug --target tests-engine tests-world-consumer tests-terrain-consumer map_storage_consumer --parallel 1
ctest --test-dir build/tilemap-u1-debug --output-on-failure --no-tests=error -V -j 1 -R '^(tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.|tests-terrain-consumer\.|map_storage\.consumer\.|world\.)' --output-junit ABS/build/world-u7-logs/results.xml
cmake --build build/tilemap-u4-display-debug --target demo --parallel 1
python3 build/world-u7-logs/corrected-probe.py
python3 build/world-u7-logs/check-ownership.py
```

ABS is the clone's absolute path; exact executable commands are retained in JSONL.
The new focused runner compiles unchanged resource memory/reservation/block cases
without enabling broad acceptance; existing aggregate/acceptance registration is
preserved. Native tests actually cover empty/full vectors, growth/clear/reuse,
long strings, nested explicit CE strings in maps, map insertion/erase/copy/move /
assignment/swap, class pools/rebound nodes/value subobjects, constructor failures,
context equality/retention, aligned storage, zero/overflow refusal before storage
change and valid recovery. Direct unwrapped CE containers complement delegated
instrumentation so its own rebind cannot hide the original defect. Actual Manager
registry/free-range inspections establish storage domain/return, not equal output
alone. Two unrelated unchanged dangling-else warnings occur in recompiled asset /
shader cases; no warning policy was weakened. There are NO test failures, skips or
post-pass campaign repeats. Total170 unique test entries:40 memory +130 shared.

## Real measurements and limits

`measurements.json` distinguishes real call counters from logical observations.
For EACH Default/Object/Pool measured class-vector workload:7 allocator calls /
7 returns,96 successful construction/destruction pairs,352 summed requested bytes,
22 value copies and30 moves. Failed construction is excluded from successful
count and returns its allocation. Pool map records20 class-node allocations and
19 successful different-U value-subobject constructions, including failure cleanup.
These are real delegated service calls in a bounded test, not production-global
counters or performance comparisons.

The actual editor retains capacity32, sizeof(TerrainEdit)=24, payload768 bytes;
after warmup four consume/flush calls and separate undo/redo retain the SAME backing
and capacity. Hard1024 slots is24576 payload bytes on this ABI. Every key/label /
visual compares independently; sibling maps/modes, other cells, histories,
generated exact-witness/final patch and old frames stay isolated. Refused collected
samples clear before later recovery; native bind/draw/upload counters remain zero.
There is no editor-wide allocation/constructor counter or measured copy-byte total.
Capacity/address plus the reserve-only-when-unallocated source establish THIS
buffer's reuse; they do not prove allocation-free editor ticks. Copy/move counts
are real; bytes inferred from them would be modeled, not observed memory traffic.

Payload/request totals EXCLUDE manager registry/section/free/stale/tracker nodes,
locks, shared-owner/control blocks, greedy spare bytes/alignment/fragmentation,
allocator objects and test counter/observer allocations. Returned backing can
remain cached in Engine. U6 recipe/base/witness/patch/history/candidate charges
still apply separately and may exceed a full snapshot; no compactness or saving
claim. Allocation/programming exceptions propagate; byte facades must outlive
scratch users, and serialized writer/preparation/cleanup remain required.

Concrete unconverted paths: full document/key-string snapshots and history copies,
TerrainDocument two-owner reconstruction, Map/TerrainMap batch candidates/results,
generated base/witness/patch/codec copies, private Cave replay scratch, event/frame
packet vectors. Future weather field/step scratch and magic effect/query/preparation
buffers require separately assigned Engine-service/lifetime/accounting review;
no sibling source or measurement was imported here.

`source-inputs.json` hashes final tested C++/CMake inputs and both preserved caches.
`preservation.json` verifies372 of375 prior production/header/archive paths stay
unchanged; ONLY the three selected allocator headers differ. All16 older receipts
and complete source-provenance archives remain byte-identical. Public storage /
tilemap headers, strict-v1 bytes and standalone procedural/storage suites remain
unchanged and were not repeated. Real input/display/atlas upload/graphical teardown /
visual correctness/fun, leak detection, installed SDK/static/Release/other platforms
and stronger filesystem durability remain open. No unchanged X11 retry occurred.

## Next action — idle

Coordinator reviews the drained standalone WORLD-U7 checkpoint, receipt, exact
source and saved native evidence. Publisher alone backs up feat/tilemap; coordinator
alone selects any integration or next data-path/mining assignment. Worker2 remains
idle. No queue, sibling merge, public extraction or next task starts automatically.

---

Historical predecessor handoff below is retained as U6 evidence, not the current
assignment or authority to start more work.

# WORLD-U6 owned generated base plus semantic edits

Standalone continuation on exact clean reviewed parent
`9cfa376a6366012fff2e1399af2c5d87acfa471c`, feat/tilemap. The committed U5 receipt
was required as old batch feature-round-20261024-0500 / WORLD-U5, ready/true; all9
nonempty evidence paths and119 successful XML entries were inspected before work.
This is NOT a fourth batch unit or a new queue. Older plans/receipts/provenance
archives and worker1 climate remain untouched. Current matching decisions are
WINT-015/016/017 in [compact](decisions.md) and [detailed](agent-notes.md) records.
Implementation and selected CPU acceptance are verified; ready for coordinator
review. The standalone [receipt](continuation/WORLD-U6.json) reports executed behavior,
not automatic coordinator or graphical/platform acceptance.

## Actual private caller

```cpp
DemoTilemapEditor editor(existing_directory);
const std::array mode{press(Key::B)};
editor.consume(mode, {{400,200},{800,600}});
editor.install_generated(recipe, WorldProof::GeneratedReplace::DiscardCurrent);
editor.set_view({0,-3072},0.125);
const std::array brush{press(Key::Digit6),paint(13,67),paint(13,67)};
editor.consume(brush, {{400,200},{800,600}}); // repeated31x19 west cell(2,2)
const auto retained = editor.generated_state(0); // deep owned value
editor.regenerate_selected();                  // retain current final semantic edits
const std::array save{press(Key::F8)};
editor.consume(save, {{400,200},{800,600}});
```

This is the SAME private DemoTilemapEditor/generated-document/world-document code
compiled in actual demo, CPU unit and world process callers. press/paint build
actual admitted ordered records. Existing F8/F9, F6/F7, B/V, map/layer/brush, pan /
zoom controls are unchanged; no new keyboard generation UI. DemoTilemaps exposes a
narrow forwarding installation entry for deliberate setup; no startup generation,
automatic replacement or new native loop. Generation requires explicit B mode and
caller policy. Existing logical-pointer/independent framebuffer scales/Y conversion /
computed half-open picks, active document extent/culling and authored pass order
remain compatible. No new camera/input/backend/public World API.

## Owned contracts and refusal boundaries

- CaveRecipe owns family, actual Domain and CaveReplay algorithm/version1,
  seed/stage/stream, requested options/budgets and versioned CaveAdapter. Effective
  options are stored from the actual result. Budget values above hard ceilings
  refuse rather than normalize; accepted budget-only changes may update provenance
  without changing content/history/views. Every other seed/config/domain/adapter
  input must remain exact under PreserveEdits, even if output coincides.
- Adapter version1 uses X-right/Y-down/x-fast rows; occupancy0 floor ->terrain:one /
  label1, occupancy1 solid ->terrain:two/label2 by explicit owned policy. Map ID,
  exactly two ordered arbitrary valid layer names and all ordered spawns are owned.
  Optional initial_detail is absent/all null or exactly C explicit semantic keys /
  nulls, bounded as BASE input; changed input requires discard. This supports base
  markers and tests real late second-owner refusal. No labels derive from colors.
  Numeric meanings101/203/307/409, labels1..4, derived IDs7..10, atlas targets0..3
  and storage keys remain separate. Shared private meaning tables preserve U5 policy.
- GeneratedDocument retains full semantic base Snapshot and actual Patch witness.
  Use real capture/diff/apply; no hash/seed/opaque identity or forged Witness.
  Export complete FINAL content after Engine last-write-wins input batches, then
  diff to unique authored-layer/row-major addresses. Null edits erase; metadata /
  extent/order/spawn changes refuse as unsupported cell-only transitions.
- DiscardCurrent stages full recipe/base/empty patch, both nonmovable terrain
  owners and reset selected semantic history before adoption. PreserveEdits requires
  an attachment and unchanged attached limits. Replay validates retained base /
  witness/patch, replays the real producer, compares complete semantic base values
  AND canonical bytes before applying the final patch, then validates both owners.
  Same final document is a validated no-op preserving owner/history/borrowed views.
  A different base/seed/config/adapter needs explicit discard; no implicit rebase.
- Attached edits, undo and redo check current complete content, stage full target,
  overlay and history BEFORE nonallocating swaps. An overlay cap failure or a later
  history refusal preserves content, both stacks and attachment. No-ops still
  validate but preserve redo/owner; accepted new edits alone clear redo. Input
  admission/Press/event-pointer and completed-run boundaries stay unchanged; there
  is no whole-tick rollback. Independent west/east and literal documents/history
  remain isolated. Prepared frames/owned observations survive later replacements.
- SAVE writes only existing complete strict-v1 meaning Snapshot, preserving live
  attachment/history. Failed SAVE retains authored runtime edits and predecessor
  under the existing file profile. Successful ordinary semantic LOAD explicitly
  detaches only selected generated state and resets its history; literal LOAD
  leaves it intact. Failed LOAD retains everything. Fresh process LOAD derives
  saved labels/visuals with NO recipe/history and regeneration refuses until a new
  explicit installation. No persisted recipe/patch/schema extension.
- Actual DemoTilemaps::update catches owned generation/patch diagnostics and drops
  remaining tick records; a later tick may still SAVE. Earlier completed actions
  remain. Serialize update/preparation, exclude concurrent writers/recycling, and
  clean up platform resources after simulation joins. Changing adoption invalidates
  borrowed views; owned values/packet resources remain independent. Allocation /
  programming exceptions propagate normally; no broad exception conversion.

## Bounds and measured logical retention

Existing editor axes1..64/exactly two layers/8192 total layer cells, storage8 MiB /
256 spawns/128-byte keys, patch hard1024 unique changes/8 MiB witness and current
history32 entries/1 MiB (hard256/8 MiB) stay unchanged. Default generated limits:
128 final changes,1 MiB witness,4 MiB attachment payload,1 Mi adapter work. Hard
limits1024/8 MiB/32 MiB/4 Mi work; recipe payload<=64 KiB. Reject excess/overflow,
never truncate. Zero edited-address cap permits validated unchanged content.

Final work admission reserves64 full-layer-cell scan passes +32 copy passes,
8 units per allowed overlay address and1 per spawn:96*L+8*cap.edits+spawns, L=2*C.
This is conservative cell-pass/copy accounting for the synchronous staging path,
not machine instructions/time; codec/table/selection internals retain their own
fixed limits. The initial narrower32*L reservation was corrected during review;
all8 U6 names were rerun afterward. Real CaveBudgets govern generator replay and
Engine input selection separately retains32 requests/160 affected/1024 work.
The cave descriptor rejects above-hard budgets; actual requirements and work are
reported independently. Exact/one-short cave cells/storage/work, adapter work,
witness/retained bytes and overlay addresses use small5x4 inputs, not stress/OOM.

Measured31x19/four-change logical charges from the actual caller:

| Category | Bytes / units |
| --- | ---: |
| Owned recipe/effective config/limits + owned field sizes |964 bytes |
| Full canonical semantic base representation |12802 bytes |
| Exact canonical witness string |12802 bytes |
| Four final sparse edit fields |94 bytes |
| Attachment total (recipe+base+witness+edits) |26662 bytes |
| Current semantic history canonical charge |51202 bytes |
| Attachment + history charge |77864 bytes |
| Existing full encoded Snapshot |12812 bytes |
| Current two terrain-owner numeric map/label payload,24*C |14136 bytes |
| Final adapter work reservation,96*1178+8*128+3 |114115 units |
| Actual Cave requirements peak payload |14698 bytes |
| Actual Cave work used |14643 units |
| Cave requested admission |4096 cells /1048576 storage bytes /1048576 work |

The full final file is12812 bytes; attachment+history is already larger. No
compactness or saving is claimed. Recipe accounting uses C++ sizeof fixed recipe /
limits/effective config plus strings and spawn/detail object/content sizes, not
capacity. Base/history use canonical encodings; edits charge layer text +two u32
coordinates +presence byte +optional key. These are mixed LOGICAL charges, not a
wire format or measured heap/RSS. Container/control/capacity overhead beyond stated
recipe charge, live TerrainOwner metadata/config, temporary encodings and old/new
owner/history copies are additional. A declared32 full canonical snapshot copy
allowance is409984 bytes here (32*max(12802,12812)); two recipe copies1928 bytes,
two witness strings25604 bytes and edits94 give437610 bytes of reconstruction
reservation, plus any copied history charge51202 and generator/current numeric
payload separately. Categories can overlap; do not add them into an asserted
allocator peak. Both live and candidate attachments/owners/timelines coexist.
Allocation can fail and synchronous copy/replay/I/O may stall simulation.

## Actual verification and evidence

All configure/build/native checks use `/mnt/projects/cheryl-features/scripts/build --`
with one job and preserved clone-local Debug incremental configurations, shared
native lock and adaptive/no-swap/admission policy. No resource bypass, unchanged
refusal retry, new dependency/pin, install, display/X11 attempt or sibling operation.
Command-local ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1 only; instrumentation/global selections unchanged.
Leak detection remains unverified. Exact commands/environment/immediate exits /
times are in `build/world-u6-logs/commands.jsonl`; full logs and `.exit` sidecars
are preserved there, including the original compilation failure.

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug \
  --target tests-engine tests-terrain-consumer map_storage_consumer \
  map_storage_header_probes tests-world-consumer --parallel 1
/mnt/projects/cheryl-features/scripts/build -- python3 build/world-u6-logs/discover.py
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error --verbose --parallel 1 \
  --output-junit /mnt/projects/cheryl-features/projects/tilemap/build/world-u6-logs/results.xml \
  -R '^(tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.|tests-terrain-consumer\.|map_storage\.consumer\.|world\.)'
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u4-display-debug \
  --target demo --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error --verbose --parallel 1 \
  --output-junit /mnt/projects/cheryl-features/projects/tilemap/build/world-u6-logs/final.xml \
  -R '^(tests-engine\.tilemap_consumer\.generated_|world\.layered_documents$)'
/mnt/projects/cheryl-features/scripts/build -- python3 build/world-u6-logs/check-ownership.py
```

- First shared build exit1,219.99s: new fixture omitted the adapter initializer
  under Werror and used Events::Modifiers instead of CE::Input::Modifiers. Corrected
  without flag relaxation; fixed shared build exit0,52.05s. New cave usage requirements
  rebuilt existing unit collection; seven dangling-else warnings in unchanged
  asset/font/runtime/shader/math cases remain in full log. No unrelated cleanup.
- Discovery exit0:127 unique entries, retaining all119 accepted names +7 new unit
  cases and1 new process scenario; exact registration correspondence saved.
- Accumulated run ONCE:127/127,0 failed/0 skipped, exit0,110.94s. All16 world child
  exits0; complete argv/stdout/stderr/immediate exits saved. New layered author and
  fresh verifier each compare1178 cells +every key/label/meaning/visual/metadata
  against independent literals/cardinal oracle; read-only fresh file hash unchanged.
- Source-review extra LOAD isolation assertions: unit rebuild exit0,17.49s; affected
  generated_files recheck1/1 exit0,1.06s. Literal LOAD preserves attached state and
  semantic LOAD detaches west while east remains exact. No production change here.
- Final work-reservation correction: shared rebuild exit0,43.64s; ALL8 U6 names
  pass8/8,0 failed/skipped, exit0,8.84s. Two additional fresh child exits0, for18
  successful world children in total (initial16 +final2). Final demo rebuild/link
  exit0,20.89s (initial demo build exit0,56.30s). This is NOT another full127 run.
- Final ownership exit0:17 actual Engine imports/exports, SAME private sources,
  original cave producer and normal single shared Engine link/loader/symbol proof.
  Wrapped format dry-run/Werror exit0 for11 C++ files.136 total passing test
  executions across127 unique names; no test failure/skip. One corrected build
  failure remains honestly recorded. Final command/source hashes are saved.

The seven new names test seed-only diversity (42 vs43), a third stage/config,
complete31x19 base/keys/labels/visuals/metadata; empty/no-op replay and borrowed-view
identity; both-layer repeated paint/erase/undo/redo; aliases sharing visual IDs;
new/failed redo policy; exact/one-short caps and history refusal after staging;
unsupported recipe/version/adapter, unknown meanings, same-sized altered bases
with old AND new legitimate witnesses; metadata/layer/spawn changes; actual valid
first-owner then late second-owner refusal; copies/input destruction/old frames;
SAVE preservation/LOAD detachment; actual update diagnostic/drop/recovery. Synthetic
bind/draw/upload counters remain zero. Frozen expected rows come from independent
Python integer reference in ignored logs; actual production ALWAYS calls original
CaveReplay. All589 cells per choice match; floor counts258/290/233 are derived
from full literal comparisons, not accepted as producer-only summaries.

Shared producer/consumer probes inspect actual source hashes/compile commands,
17 real Engine imports/exports (including capture/canonical bytes/encode), symbols /
readelf/ldd/link. The same generated-document/world-document/editor code compiles
into unit/world/demo. Private cave archive contains ONLY existing maze/random/cave
sources once per configuration. Normal shared Engine alone owns storage/tilemap;
no alternate implementation archive/static macro workaround. Public headers,
replay/strict-v1 bytes and375 preserved production/archive paths stay unchanged;
15 previous committed receipts remain exact. Logs total about19.5 MB, preserving
complete diagnostics rather than relying on truncated tool output. Standalone unchanged storage/PG
campaigns are not repeated. Final source hashes, XML, registration and process
correspondence are saved with logs. Format check is wrapped and scoped.

## Remaining limits and exact next action

Real display/input/atlas decode/upload/capture, graphical teardown, visual
correctness/fun and leak checking stay unverified. Prior X11 :1 failure is unchanged
and NOT retried. Source-tree GNU/Linux Debug/shared proof does not establish
installed packaging/SDK, static/Release, other platforms/toolchains/ABI, indefinite
old-version replay or production latency/allocation. Controlled existing directory /
stable cwd/single-writer native checked-rename profile only: no fsync/power-loss,
hostile-race/multiwriter/distributed transaction. No persisted recipe, compact
storage, wire migration, geometry/chunks/physics, sibling integration or new queue.

Next action: coordinator reviews the exact drained standalone checkpoint, receipt
and saved evidence. Publisher alone backs up assigned feat/tilemap. Worker2 remains
IDLE; root alone selects any subsequent assignment/materialization/integration.
