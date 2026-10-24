# Active procedural-generation contracts and evidence

Continues immutable [PROCGEN-001–030 records](../../features/procedural-generation/agent-notes.md)
from U7 `946c2db7d9a14311a398664444fc4d1fe0417a90`. U8 begins at exact clean
WORLD-U1 promotion `e9289fcb992feddf85deb942a560562a54971f78`; both integration
merge parents, receipts and nine source-provenance archives are preserved.

## PROCGEN-U8 — Proposed caller before dependent APIs

```cpp
const std::array<Index,2> extent{17,11};
const StreamKey key{42,0x67656f6772617068,7};
auto a = generate_elevation(extent,key,{3,-80,120},budget).value();
auto b = generate_elevation(extent,key,{6,-240,360},budget).value();
auto coast0 = measure_sea(a,0,extent[0]*extent[1]).value();
auto coast40 = measure_sea(a,40,extent[0]*extent[1]).value();
```

Proposal (engineering choice, not personal preference evidence): height ticks above
an arbitrary zero datum, signed32 integers; one horizontal cell is a sampling unit
with no prescribed physical aspect ratio. Inclusive requested height interval,
positive integer feature spacing, bilinear weights with one final floor rounding.
Finite origin-aligned lattice may bracket the last cells using outside anchors;
no periodicity, pinned edge elevation or chunk continuity is implied. Water means
height<=sea level; changing that analysis never mutates/regenerates heights.
Separate family-local errors avoid changes to shared WORLD maze/random/cave
producers. No extra octave/normalization is needed for this useful first proof.
Implementation and native acceptance are pending at this point.

## PROCGEN-031 — Implemented integer field contract

Owner: projects/tests/procedural-generation/elevation.h/.cpp. Exactly two positive
Domain axes; x=axis0 fastest, index=x+width*y, positive y advances rows. Vertical
Height is int32, one abstract height tick above an arbitrary zero reference.
Neither meters nor a vertical-to-horizontal aspect ratio is selected. Config has
exact integer spacing in[1,4096] and inclusive signed32 low<=high. Full signed32
range and flat low==high are valid, with no implicit float conversion/normalization
inside the utility. Bad spacing/range rejects. Unsupported algorithm/version
rejects explicitly. Existing Domain dimension/extent/product failures map to
family-specific errors before allocation.

For each axis, lattice count=ceil((extent-1)/spacing)+1. Anchors start at0 with
pitch spacing; the last may lie just outside the finite domain. Check last-anchor
coordinate arithmetic even during planning. Unit axes have one anchor; exact
end-anchor samples use zero fractional weight and clamp only the unused second
lookup to that same anchor. No periodic boundary, sea-height edge pinning, shortened
last segment or seam/chunk/cropping consistency across different domains is implied.
A finite window's row width affects stream order; Domain is part of replay.

Generate unsigned32 offsets uniformly in[0,high-low] with existing unbiased
integer sampling. This requires a u64 bound up to2^32, not overflowing32-bit
high-low arithmetic. Order is x-fast lattice rows. Flat range still consumes one
bound1 draw per anchor. Interpolate offsets using four products whose weights
partition spacing^2. Sum, divide ONCE by spacing^2, then add low in i64 and cast
to Height. This is floor toward negative infinity: a -2.5 interpolated height is
-3, not -2. No intermediate horizontal rounding. Floor error is less than one
tick below the exact rational value; requested anchor interval is never normalized
to the observed interval. Achieved extrema/range may be strictly smaller than
requested, especially when anchors lie outside the output domain.

All offsets<=2^32-1; spacing^2<=2^24. The sum<=range*spacing^2<2^56, preflighted
with checked u64 multiplication. Every partial sum is nonnegative and bounded by
that total. Adding low afterward lies within signed32 low/high. Coordinate and
row-major products remain below checked Domain.cells; operation admission caps
cells before buffers/traversal. No signed overflow, float interpolation, std::hash
or implementation-dependent random distribution. This is a stylized value field,
not physical geology. Optional octave/smoothing controls are deferred because
pitch/range plus independent classification already supply a useful bounded proof.

## PROCGEN-032 — Sea semantics and lifetime

classify_sea(height,sea_level) returns Surface::water for height<=sea_level,
Surface::land otherwise. Sea threshold has the same signed height ticks/datum.
measure_sea borrows an existing Elevation only during its call, allocates nothing,
counts every cell under an exact work cap and returns owned threshold/count/work
values. Equal heights are water. Flat domains are all water at their own height;
one tick below is all land when representable. INT32_MAX sea makes every height
water; INT32_MIN sea includes only minimum-height equality. No percentage or
connected-ocean/river/lake guarantee follows. A changed threshold does not regenerate
heights or change their descriptor; sea threshold is analysis metadata.

For the SAME immutable field, raising threshold cannot decrease water count. This
precise classification monotonicity is tested; no per-seed visual complexity,
perceptual quality or real terrain claim is made. height(index) checks flat index
bounds; callers can use existing Domain coordinate/index round trips. Result owns
its full vector and copied replay. Deep copies are independent; extents/temporary
replay can die after generation. Read-only spans expire on owner move/assignment/
destruction; moved-from values support destruction/assignment only. Refusal does
not mutate a previous result or return a partial field/summary. Allocation and
programming exceptions propagate; only explicit local expected-failure transport
is caught. No global registry, hidden cache or state is retained.

## PROCGEN-033 — Streams, resources and integration boundary

ElevationAlgorithm::lattice_bilinear version1, exact Domain, requested config,
caller seed/stage/stream and budgets are retained. Effective config equals requested.
Child stream is IntegerStream::for_stage({derive_stream_state(random),
0x656c65766174696f,0}), with existing documented LE64 numeric-word derivation and
SplitMix64. Unrelated stage consumption cannot perturb it. No cryptographic or
collision-free stream claim. Existing8-bit-byte/64-bit Index/size_t arithmetic
contract applies; exact integer ordering is specified, broader toolchain execution
is not inferred. Successful content is independent of sufficient admission budgets;
there is no saved recipe or long-term old-version/migration guarantee.

Peak requested payload4096+4*C+4*L: C signed32 output heights, L unsigned32 scratch
lattice values, fixed metadata/return allowance. L<=C for spacing>=1. Checked
products/sums/container capacities and last-anchor coordinates precede allocation;
metadata sizes have static assertions. This excludes allocator/runtime overhead
and existing caller input/results; it is not exact RSS. Hard caps65536 cells,
2MiB requested payload,10M work and spacing4096. Planning may describe huge valid
Domains but never allocates their buffers. Operations clamp caller budgets to hard
caps and refuse before allocation when cell/storage/minimum work cannot fit.

Initialization charges C+L, lattice write/setup charges L, every raw draw including
rejections charges one via accepted sample_below, each output charges six (setup,
four weighted terms, final height/measurement update). Thus actual work is
7*C+2*L+raw_draws, minimum7*C+3*L. Rejection draws have no automatic finite worst-
case bound; runtime Work exhaustion refuses without publishing partial buffers.
Generation is O(C+L+draws), O(C+L) payload. Sea analysis uses exactly C work, no
scratch/output allocation and exact early work admission. Units count deterministic
operations, not CPU instructions/time or a memory-pressure/RSS guarantee.

Local ElevationFailure uses existing FailureKind but its own ElevationReason and
expected alias. This narrow private error boundary avoids changing maze.h/.cpp,
random.cpp or cave.h/.cpp and their actual WORLD producer linkage/semantics.
The closed positive-bound sampler can return only work refusal here; all other
allocation/programming exceptions propagate. WORLD builds exactly maze.cpp,
random.cpp,cave.cpp in projects/engine/tests/world-consumer/CMakeLists.txt:9,
not a glob of new utilities. That file, producer bytes, old test sources and archive
records are untouched. Only standalone procedural CMake adds new targets. No
unchanged Engine/map suite is rerun solely due to promotion.

Provenance and taste: [archived PROCGEN-002](../../features/procedural-generation/agent-notes.md)
records HyperMaze3fddec642ce06737c9600f5036198a0826d56f55 Graph::Generate topology
and eb3f81c219e779b0200c1e82e5bc7239386528e8 include/abstracts/singleton.h
CherylE::ConcreteSingleton::construct typed caller evidence (medium confidence,
legacy ownership/import counterevidence). It does not settle elevation API,
interpolation or units. All new spelling/error/storage/algorithm choices are
engineering judgments, not personal preference claims. New active records preserve
historical hashes; future exact-base retention, drainage/closed basins, geography
changes and settlement suitability need later bounded consumers, not a World API.

## PROCGEN-034 — Executed checks, preservation and handoff

Starting source is e9289fcb992feddf85deb942a560562a54971f78. The promoted history
and archived U1–U7 docs are unchanged. Before/after SHA256 checks cover nine archived
records against their manifests and five shared producer/header files;14/14 match.
Saved preservation-before.json/preservation-after.json are lightweight source
checks, not new native Engine acceptance. All66 retained case assertion counts
match the preceding U7 logs exactly; old71 CTest names and test sources remain.
Root development index only gains links to this active namespace.

Reused clone-local builds/procedural-generation (no cache/output copy or cleanup):
GNU16.2.1/Linux, Ninja, Debug/C++23, strict warnings-as-errors, UBSan ON/no recovery/
frame pointers. Compiler/cache/flags inspected. Every format/configure/build/native
check uses /mnt/projects/cheryl-features/scripts/build --, one job, existing
adaptive/no-swap/admission-floor policy. Original build49218/test97700 sessions
were retained through normal queue waits and completed. No admission/resource/
allowance/auth refusal, retry, native failure or lost/truncated-file diagnostic.
No new dependencies, system change, Engine/map suite, external service, subagent,
protected mutation, branch integration or publication command was performed.

Complete logs and immediate exits under builds/procedural-generation/logs/u8/:

| Executed command | Evidence | Outcome |
| --- | --- | --- |
| clang-format -i on six owned C++ files | format-1.log | exit0 |
| cmake --build builds/procedural-generation --parallel 1 | build-1.log | exit0; existing standalone cache regenerated; compile/link pass |
| ctest --test-dir builds/procedural-generation --no-tests=error --output-on-failure --verbose -R '^procgen[.]' --parallel 1 | ctest-1.log | exit0;80 passed/0 failed/0 skipped |
| Immediate status | exits.tsv | all three commands0 |
| Independent Python integer/Fraction and inverse-mix vectors | reference-vectors.log | source/oracle evidence; not native acceptance |

Accumulated suite ran ONCE:80 entries,74 cases/27681 assertions plus six consumers.
Old71 entries/66 cases/27265 assertions and five consumers stay intact. Eight new
cases add416 assertions: literal23,fields106,sea27,flat167,replay8,invalid42,
limits29,ownership14. Signed-rational/per-cell checks also execute but are not
inflated into harness assertion counts. New consumer3/3; original consumers16/16,
total19 demonstrations. No compiler warning, failure, skip or UBSan diagnostic.
No broader native repeat is justified by this isolated producer addition.

Complete independently derived4x3 field, spacing2/low-7/high5/key42:
anchors[4,-2,-3,4,2,4] at a3x2 lattice; output rows
[4,1,-2,-3], [4,2,0,0], [4,3,2,3]. X3's -2.5 floors to-3. Observed range7
is smaller than requested12. The last x-anchor4 lies outside the finite output;
row order/coordinate round trips are checked. The oracle reconstructs the pinned
stream then uses SIGNED numerators/rational floor rather than production unsigned
offsets. Sixteen seed/spacing17x11 fixtures and extreme/unit-axis/flat cases also
match it. Full int32 interval with spacing4096 exercises the largest allowed
weight arithmetic without huge buffers. Bound1 flat generation still consumes
anchor draws. No per-seed uniqueness or perceptual monotonicity is asserted.

Literal sea thresholds[-4,-3,-2,-1,0,1,2,3,4] yield water counts
[0,1,2,2,4,5,7,9,12]. Exact equality and INT32 endpoints are checked. Analysis
with work12 passes and11 refuses before allocation, preserving field/previous
summary. Tiny/flat domains, checked indexes, copied outputs after later calls,
local input/descriptor lifetime, moves and propagated bad_alloc all pass.

Resource fixture C12/L6:4168 accounted bytes, minimum/actual102 work. Exact cell/
byte/work admission passes; each one-short fails before allocation using scoped
test-only allocation denial. A derived key seed0x0d201cfa683c7b1a makes the first
raw draw0 reject for bound3, next draw accepts height0:1x1 uses2 draws and11 work;
minimum10 admits but refuses without a partial result. Invalid spacing/range,
empty/zero/non2D, unsupported algorithm/version, index, product/byte/last-anchor
overflow and hard-cap-plus-one are checked with metadata only/no huge allocation
or deliberate memory pressure. Practical hard limits remain intentionally small.

Actual consumer: Domain17x11/key{42,0x67656f6772617068,7}, height ticks/datum0,
x-fast rows. Scalar classification and count summaries are semantic only:

| Spacing/requested interval | Observed min/max/range | Anchors | Sea | Land/water | Generation work/draws | Sea work |
| --- | --- | --- | --- | --- | --- | --- |
|3/-80..120 |-76/118/194 |35 |0 |167/20 |1414/35 |187 |
|same owned field |same |35 |40 |110/77 |same |187 |
|6/-240..360 |-186/352/538 |12 |0 |166/21 |1345/12 |187 |

Sea change reuses exactly the first output; no height/regeneration/descriptive
mutation. These measured consequences prove useful controls without promising
land percentage or hydrology. No river routing, lake filling, erosion/tectonics,
settlement simulation, file/save recipe, materials/assets or universal World API.

Committed PROCGEN-U8.json in this active namespace records ready/checks=true only
after native acceptance, actual nonempty relative logs, contracts/limits and next
action. It reports worker execution, not automatic coordinator source acceptance
or an unexecuted platform. One coherent local checkpoint stops this bounded turn;
coordinator reviews source/evidence and selects any later task, publisher handles
assigned feat/procedural-generation transport. No worker2 later work is merged.

Staged review covers all12 task files: production, independent checker/tests,
consumer/private CMake, matching active decisions/notes/handoff/receipt and root
index links. No unrelated or previously unfinished changes are included. Staged
whitespace checks pass; the complete diff is saved locally as staged-review.diff.
Only these records changed after native acceptance; source and test bytes match
the executed build. No additional native rerun is needed for this record update.
Two source-only receipt-validator text assertions failed (exit1): older expected
CTest summary wording, then whitespace padding before test numbers. Inspecting
the complete original log resolved both; final validation checks the actual
summary and all80 passed-entry lines. Diagnostics/exits are saved locally as
source-validation-*; these are validator fixes, with no native failure or rerun.

Confidence high for the specified tested integer/replay/ownership/resource/sea
contracts, medium for private API/unit/pitch choices and future broader reuse.
No unresolved blocker. Other compilers/OSes, ASan, large-domain performance, public
Engine/SDK geography use and physical realism are unverified. Existing WORLD
visual/input/assets/teardown/leaks/fun, ABI/installed/static/Release and persistence
power-loss/multiwriter/durability gaps remain unchanged in their archived records.
A later bounded drainage utility needs explicit slopes/ties/outlets/closed-basin
policy and exact input identity; current sea threshold does not implement it.

## PROCGEN-U9 — Proposed caller before dependent APIs

Exact clean prerequisite cd446ab3c1ffed71e4e6d68d1a8452d050283a7a is inspected:
U8 ready receipt, source, next.md, build-1.log/ctest-1.log/exits.tsv establish80
passes/27681 assertions. New unit belongs only to feature-round-20261024-0500.
The immutable earlier queue and all provenance archives stay unchanged.

```cpp
auto domain = Domain::checked(std::array<Index,2>{3,2}).value();
const std::array<Height,6> heights{9,6,3,8,5,0};
const std::array<Index,1> outlets{5};
auto literal = analyze_drainage({domain,heights,outlets},budget).value();
auto field = generate_elevation(extent,key,{3,-80,120},budget).value();
const std::array<Index,2> chosen{0,field.descriptor().domain.cells()-1};
auto generated = analyze_drainage(
    {field.descriptor().domain,field.heights(),chosen},budget).value();
```

Proposed engineering choices: terminal cell index is the catchment identity;
equal greatest drops choose smallest row-major neighbor index. Explicit outlets
terminate even if downhill neighbors exist; duplicates reject, no implicit outside
or sea outlet. Retain exact owned source heights plus outlet membership as input
provenance. Kahn ordering computes upstream unit contributions, reverse ordering
propagates terminal labels in linear bounded work. No per-cell repeated path walk
in production. Implementation/checks pending; spelling is not user taste evidence.

## PROCGEN-035 — Implemented routing and terminal contract

Owner: projects/tests/procedural-generation/drainage.h/.cpp; private standalone
CPU C++23 only. DrainageInput copies checked Domain metadata and borrows heights
and explicit flat outlet indexes for the synchronous call. Callers supply a real
positive2D checked Domain and exactly C signed32 heights, stable during the call.
Domain::checked owns empty/zero/extent/product validation; analysis refuses non2D,
shape mismatch, unsupported algorithm/version and invalid outlets. No retained
span/input borrow, mutation, callback, RNG, sea threshold or hidden outside cell.
Axis0=x fastest, index=x+width*y, positive y advances rows; heights retain original
abstract ticks and datum. This has no physical distance/aspect-ratio convention.

Explicit outlet indexes may be interior or perimeter, unordered, including every
cell; empty set is valid. Duplicates and out-of-range indexes reject before cell
buffers. Membership is set semantics, so accepted input order cannot alter output.
An outlet has no receiver even if a lower neighbor exists. Every other cell inspects
x-,x+,y-,y+ cardinal neighbors, excluding outside. Choose greatest positive
original-height drop, tie by smallest flat neighbor index regardless of inspection
order. int64 subtraction covers [-4294967295,+4294967295] for full int32 extremes.
No equal-height link, diagonal, wrap, forced boundary escape, implicit sea/ocean
outlet, path fabrication or depression fill. No-lower-neighbor cells are sinks.

Strict descent in a finite domain proves no cycle, hence every chain reaches an
explicit outlet or sink. It does not prove all cells reach declared outlets:
plateaus can have one sink per nonoutlet cell. Outlet and sink counts, and their
contributing totals, remain separate. TerminalKind::none means this cell flows;
outlet/sink means this cell terminates. no_receiver is UINT64_MAX, impossible as
an admitted cell index. terminal is the actual terminal CELL index, also the
catchment identity; it is not a dense label. Terminal cells label themselves.

## PROCGEN-036 — Accumulation, identity and ownership

Each cell's contributing starts at1 for its own unit. Count incoming receiver
edges; enqueue zero-indegree cells in ascending flat order. FIFO pop transfers
the complete contributing count once to its receiver and decrements indegree;
enqueue that receiver only when it becomes zero. Each cell pushes/pops once,
each edge updates once; no stale entries, sort, recursion or repeated chain walk.
Stored queue order is reused backward to copy the already-complete destination
terminal label. Checked sums cannot exceed C; invariant failure throws logic_error,
not a generic expected refusal. Terminal totals must conserve C. Measurement fields
are cells/links/outlets/sinks, outlet_contributing/sink_contributing, largest_catchment
and maximum_contributing. A catchment size is its terminal's contributing count;
the latter maximum also includes nonterminals. Counts are cells, not water flow.

Drainage owns C DrainageCell records and an exact C-element source height copy.
Descriptor owns Domain, requested budgets and algorithm/options. Original outlet
membership is retained exactly as cells.kind==outlet; source order/duplicates are
not an input identity because duplicates are invalid and order is immaterial.
Together these form a complete bounded witness: no digest collision or dangling
generated Elevation borrow. Caller may retain U8 recipe separately, but a seed
is not this analysis's input identity. No persisted witness/recipe/save format or
long-term migration policy is selected. A later changed height/outlet set requires
fresh analysis; current results are exact only for their retained witness.

Copies deep-copy both buffers. Const views expire on owner destruction/move/
assignment; moved-from values support destruction/assignment. A checked cell()
returns an owned record or invalid index. Temporary input arrays or generated
Elevation can die after return. Later calls/refusals cannot change prior outputs.
Only private Stop transport for defined invalid/unsupported/budget failures is
caught; bad_alloc and programming exceptions propagate with no partial publication.

## PROCGEN-037 — Checked admission and actual operation accounting

Algorithm steepest_cardinal v1 is deterministic integer analysis, no random seed.
Same exact Domain/heights/outlet set/options yields same logical cells/metrics;
sufficient budgets affect admission only. Unknown algorithm/version reject.
Existing64-bit Index/size_t and8-bit-byte contracts apply; logical integer ordering
is specified, broad compiler/platform execution is not inferred. Existing private
CaveBudgets is reused only for cells/payload/work, with no cave/generation policy.
Family-local DrainageFailure/DrainageReason avoids any shared maze/cave/Engine edit.

Hard caps65536 cells,4MiB accounted payload,2M work. Planning checks non2D,
outlet_count<=C, vector capacities and all byte/work products/sums before allocation;
large checked Domain metadata never triggers giant buffers. Admission clamps caller
budgets to hard caps, checks cells/storage/minimum work, then exact height count.
Within admitted C, an8192-byte fixed bitset validates all outlet bounds/duplicates
before allocating cell buffers. Thus malformed lists do not allocate large output.
If multiple errors exist, algorithm/version/planning/budget failures can precede
span-content validation; callers must supply valid spans, not fake huge spans.

Peak requested payload12288+48*C:32*C owned records (three u64s and kind/padding),
4*C owned original heights,4*C indegree scratch,8*C bounded queue reused as order;
8192 fixed outlet-validation bitset plus4096 metadata/return allowance. Static
assertions check element/metadata sizes. Maximum-domain payload3158016 bytes fits
the4MiB cap. Existing input/results, allocator/runtime/exception overhead and stack
runtime are excluded; this is not exact RSS. No pressure/OOM experiment is needed.

Operation units, not instructions/time: bitset initialization1024; outlet checks
O and marking O; four buffer initialization/copy passes4*C; receiver setup C;
four neighbor attempts per NONoutlet4*(C-O), including outside/uphill/flat probes;
accepted incoming-edge record E; source scan C; queue writes C; pops C; edge
accumulation E; reverse terminal/measurement pass C. Total1024+13*C-2*O+2*E.
Preflight lower bound1024+13*C-2*O, finite upper adds2*(C-O). Actual charges occur
before each operation/phase; an admitted lower bound can still exhaust at runtime.
No partial field/measurements are returned. C65k bounds contributing values and
all indexes; checked sums guard both overflow and internal conservation invariants.
Complexity O(C+O) work/storage plus fixed bitset; no unbounded queue duplicates.

Engineering alternatives: sorting by height would need bounded comparison accounting
and extra ordering rules; topological order is simpler and still exact on strict
descent. A hash-only input ID is smaller but cannot prove exact identity. The4*C
witness is useful to later depression/channel consumers and remains small here.
Choice confidence high for the tested contracts, medium for private type spelling,
fixed bitset/payload tradeoff and future reuse. No new personal taste inference:
archived PROCGEN-002 retains exact HyperMaze/v3 symbol/revision evidence and legacy
counterevidence. Agent-authored U8 types supply compatibility, not user preference.

## PROCGEN-038 — Executed checks, source preservation and handoff

Source starts at required clean cd446ab3c1ffed71e4e6d68d1a8452d050283a7a. Inspected
U8 ready receipt/source/records and complete saved native logs before editing.
No prerequisite blocker. Workspace/autonomy/finite-queue/round plan/feature brief/
workstream/generation-storage and clone guides are read. This is only assigned U9;
older completed queue/receipts and source-provenance archives remain unchanged.
Ongoing records stay active here; the required new queue receipt uses its exact
batch path. No sibling fetch/merge/publication, protected mutation, process/cache
change, install/dependency change, model substitution, agent or recurring work.

All native commands use /mnt/projects/cheryl-features/scripts/build --, one job,
preserved builds/procedural-generation GNU16.2.1/Linux Ninja Debug C++23/UBSan ON,
strict warnings-as-errors/no UBSan recovery. Original build16204/test80472 sessions
were retained to completion; normal shared-lock waiting is not a blocker. No
resource75/allowance/auth refusal, failure, correction or unchanged retry occurred.
Complete command stdout/stderr, commands.txt and immediate exits.tsv are in
builds/procedural-generation/logs/u9/:

| Executed check | Log | Exit/result |
| --- | --- | --- |
| Wrapped clang-format -i, six new owned C++ files | format-1.log |0 |
| Wrapped cmake --build builds/procedural-generation --parallel 1 | build-1.log |0, standalone cache auto-regenerated, compile/link pass |
| Wrapped ctest --test-dir builds/procedural-generation --no-tests=error --output-on-failure --verbose -R '^procgen[.]' --parallel 1 | ctest-1.log |0,90 passed/0 failed/0 skipped |
| Lightweight source/count/hash verification (not native acceptance) | source-evidence.log/source-exits.tsv |0;17 preserved witnesses, all prior80 names/74 case counts match |

Accumulated suite ran ONCE:90 entries,83 cases/34503 assertions plus seven
consumers (21 demonstrations). All prior80 entries/27681 assertions and19 old
demonstrations pass unchanged. Nine new cases add6822 assertions: literal36,
terminals26,ties15,extremes16,exhaustive6562,generated40,invalid47,limits64,
ownership16. Independent per-cell oracle checks execute additionally; not inflated
into harness counts. New semantic consumer passes2/2. No compiler warnings or
UBSan diagnostics. No unchanged Engine/map suite or broader platform claim.

Literal3x2 heights[9,6,3;8,5,0], outlet5: receivers[1,2,5,4,5,none], all terminals5,
contributing[1,2,3,1,2,6], five links, one outlet/no sink, work1110. Literal fork
has contributing[1,1,1,2,6,2,1,9,1] to outlet7. Bowl has four sink terminals
plus outlet0, center accumulation5, outlet units1/sink units8. Plateau2x2 with
one outlet has three sinks, no links; it is acyclic without being outlet-drained.
Tests also cover forced outlets on a descending slope, empty/all-outlet sets,
singleton, unit axes, no diagonal/row wrap, equal-drop index1 over index3, full
signed extrema/drop4294967295 and deterministic accepted outlet order/budgets.

Independent checker enumerates all cells as geometric cardinal candidates using
coordinate differences, choosing lowest neighbor height/index; no Domain::neighbor
or production selection/queue/accumulation helper reused. It then traces each
source chain, bounds steps, adds unit counts and checks EVERY output cell,
terminal type, exact witness, measurements and terminal conservation. Exhaustive
3x2 grids over heights{-1,0,1} cover729 fields times three outlet policies=2187
analyses. Eight U8 fields (seeds0,1,42,991; pitch3/6;17x11) match independent
tracing without changing original Elevation/replay. Larger65536-cell flat domain
is a modest bounded hard-limit check, not quadratic oracle or memory pressure.

Admission fixture C6/O1:12576 bytes, lower1100/upper1110 work, actual1110.
Exact cells/payload/work passes. One-short cells/storage and lower-work1099 refuse
before allocation (scoped test-only allocation denial); budgets1100..1109 each
admit then exhaust without publishing partial results. All-outlets has equal
lower/upper work. Hard cap+1, metadata-only capacity/byte overflow, non2D, missing/
extra heights, bad/duplicate outlets, unsupported version/algorithm and invalid
cell indexes reject. Domain checks empty/zero/product overflow. No huge rejected
buffer is allocated. Owned input/copy/lifetime/replay and prior result preservation
pass; allocation denial confirms bad_alloc propagates through successful admission.

Actual generated consumer uses U8 lattice_bilinear v1 key{42,0x67656f6772617068,7},
pitch3/requested-80..120/domain17x11 and EXPLICIT outlets{0,186}. It reports187
cells/175 links, two outlets, ten sinks, outlet contributing14/sink contributing173,
largest catchment43, actual work3801. Outlet0 receives13 units; outlet186 receives1;
sample cell3 reaches sink6; cell185 reaches sink168. Complete oracle checks all187
cells while text displays six samples. No sea-level classification affects routes.

Seventeen before/after SHA256 witnesses match: nine immutable archived records,
five shared WORLD producer/header files, U8 elevation.h/.cpp and ready receipt.
Only standalone CMake/new private sources and active/queue records change. WORLD's
explicit maze/random/cave source list remains unchanged; no new integration or
source archive rewrite. Full logs retain exact commands/exits for coordinator review.

Complete staged review covers12 task files: six new private C++ files, standalone
CMake, three active records, root index links and required queue receipt. Whitespace
checks pass; full staged diff is saved as staged-review.diff. No unfinished or
unrelated work is included. Code/test bytes are unchanged since successful native
acceptance; subsequent edits only complete records and receipt, requiring no rerun.

Ready receipt reports only actual worker execution. Stop idle after one coherent
reviewed local checkpoint; publisher handles assigned-branch transport. Controller
alone may select the next exact task after clean successful drain and committed
passing receipt. Later depression/spill work must inspect this source/receipt/logs
first; U9 strict descent cannot route filled flats. River extraction must use the
actual selected graph and its contributing counts. No next unit starts here.

High confidence in tested descent/identity/ownership/resource semantics; medium
in private API future reuse. No unresolved blocker. Other compilers/OSes, ASan,
performance/public Engine geography, physical rainfall/discharge, depression
filling/spill/plateau routing, erosion, rendering and persistence formats remain
unverified/unimplemented. Existing WORLD visual/input/assets/leaks/fun, ABI/
packaging and file-durability gaps remain with their archived owners.

## PROCGEN-U10 — Proposed caller before dependent APIs

Starts at clean dfeeeffd51249a4994e34c745bd143aed3a8f525, not the old U8 head.
Committed U9 receipt has exact batch/unit, ready/true and real nonempty logs;
source, limits, records and saved90-test/34503-assertion evidence are inspected.
This one assigned milestone leaves all older archives/receipts unchanged.

```cpp
auto bowls = analyze_spill({domain,literal_heights,outlets},budget).value();
auto terrain = analyze_spill(
    {field.descriptor().domain,field.heights(),chosen_outlets},budget).value();
// Read cells' original/effective/depth; follow parent with decreasing rank.
```

Proposed engineering choices: minimax cardinal escape level to explicit nonempty
outlets; stable priority flood (effective height,cell index), discovered once.
Parents point to already-settled cells; decreasing ranks route equal-height fills.
Owned records retain original heights and outlet membership (parent absent).
Explicit bounded frontier capacity/push/work/payload budgets; no lake grouping,
physical simulation or U9 strict-descent substitution. Names are compatibility/
engineering choices, not personal taste. Implementation/acceptance pending here.

## PROCGEN-039 — Implemented minimax height contract

Owner: projects/tests/procedural-generation/spill.h/.cpp in the private standalone
C++23 CPU target. Reuse only U9 DrainageInput's checked Domain/heights/outlets view;
no call to its strict-descent analyzer. Exactly two positive axes, C heights and
at least one distinct in-domain outlet are required. Unit axes/singleton valid.
Domain owns zero/empty/extent/product checks. Outside is absent, with no wrap,
diagonal, implicit perimeter/sea outlet or missing-outlet fallback. Accepted outlet
order is irrelevant; duplicates reject before large buffers. No input mutation.

For cell v, effective(v)=min over cardinal paths to any declared outlet of the
maximum ORIGINAL height on that path, including v and the outlet. Thus effective
cannot lower original; declared outlets have their original heights and no parent.
All cells are traversable (no masks). Rectangular cardinal connectivity guarantees
some finite path when an outlet exists. Signed32 heights use original abstract
ticks/datum, x=axis0 fastest, index=x+width*y, positive y advances rows. Effective
levels are original sampled levels, without epsilon gradients or float rounding.

depth=u64(i64(effective)-original), max4294967295 for full signed32 endpoints.
Measurements: cells/outlets, positive-depth filled_cells, maximum_depth, checked
sum of depths volume, min/max effective. Area means number of unit cells; volume
means cell-height ticks with abstract unit cell area, not physical cubic units or
rainfall/discharge. No connected-lake IDs/groups or physical lake/erosion/weather
model is reported. Original source and U8 interpolation/replay stay unchanged.

## PROCGEN-040 — Single-discovery proof, forest and lifetime

Priority_flood version1 seeds explicit outlets in ascending flat index order at
their original heights. Fixed binary min heap key=(effective height,cell index).
Pop minimum; assign increasing unique settle rank. Inspect x-,x+,y-,y+ neighbors.
The first undiscovered neighbor receives max(its original,current effective),
parent=current and current's actual outlet identity; mark discovered BEFORE push.
Queued keys never change. Every cell queues/pops exactly once, with no stale
entries, relax/reopen loop or recursion. Rank0..C-1 is globally unique; every
parent was already settled, so parent rank is strictly smaller and effective
height nonincreasing toward the root. This explicitly routes filled plateaus.

Why first discovery gives the minimum escape level: any path at a lower level
would have reached that neighbor through a lower-key frontier predecessor first.
The neighbor's own original height is a fixed lower bound, so max(original,popped
minimum) cannot improve via a later predecessor. Equivalently the frontier crosses
every lower-threshold reachable set before a higher key. Seeds are exact outlet
levels, not fabricated zero/sea. Stable heap ties and neighbor order choose one
reproducible witness path among equal escape options; neither shortest hop count
nor smallest-root-ID across all possible equal-level paths is promised.

Spill owns one C-record vector: original/effective signed32 plus u64 depth,parent,
rank,outlet (40 bytes/record). Parent absent exactly identifies original explicit
outlet membership. Descriptor copies Domain/options/caller budgets, giving exact
input meaning/witness without a probabilistic hash or retained input span. Caller
can retain U8 recipe separately; a seed alone is not this analysis's identity.
No persisted recipe/witness, migration or indefinite old-version policy is chosen.
Logical content is deterministic for the same exact input/options; sufficient
admission budgets may change initialization usage, not forest/heights/metrics.

Borrowed inputs remain stable during the synchronous call only. Copies deep-copy;
const views expire on owner destruction/move/assignment. Moved-from supports only
destruction/assignment; cell() returns an independent record or invalid index.
Later calls/refusals and dying input arrays cannot invalidate previous output.
Only local Stop transport is caught. Allocation/programming exceptions propagate;
no partial records/metrics or replacement are published on failure. U9 still
reports strict original-height sinks; calling it on equal filled levels cannot
substitute for this rank forest. Future contributing counts must use the actual
chosen forest, rather than importing old raw-descent accumulation.

## PROCGEN-041 — Payload, frontier and operation limits

SpillBudgets carries cells,storage_bytes,work,queue_entries,queue_pushes. Hard
ceilings65536 cells/4MiB payload/8M work/65536 queue entries and pushes, clamped
against caller limits. Q=min(clamped queue_entries,C). Planning requires nonempty
outlet_count<=C, Q>=outlet_count, checked vector capacities, index/byte/work sums
and C*(2^32-1) maximum volume. Admission checks cells/storage/minimum work and
known C pushes before input-height count/content. Then an8192-byte fixed bitset
validates every outlet bound/duplicate before allocating cell buffers. Competing
errors can therefore report planning/budget before span-content validation; callers
must supply valid spans. Huge metadata-only checks never allocate giant buffers.

Peak requested payload12288+40*C+8*Q: owned records, fixed-capacity heap slots,
8192 bitset plus4096 metadata/return allowance. Static assertions verify record/
metadata sizes. At C=Q=65536 this is3158016 bytes. Existing caller storage/results,
allocator/runtime/exception/stack-runtime overhead are excluded: not exact RSS.
Q>=outlets is necessary but not sufficient; a larger pending frontier can refuse
at runtime. Every push checks capacity and push limit before its slot write.
An unavailable slot refuses the whole local result, not mathematical feasibility.

Work is deterministic counted operations, not machine instructions/time:
bitset initialization1024, outlet validation O, buffer initialization C+Q,
original/effective copy C, seed scan C, settle/measure C, neighbor attempts4*C
(including outside/seen), first-discovery record C-O, pushes2*C (admission + slot
write), pops C. Mandatory total1024+12*C+Q. Extra work counts root replacements,
every key comparison including failed tests, and swaps. Sift-up costs<=2*ceil(log2Q)
per push; sift-down<=3*ceil(log2Q) plus replacement per pop. Upper bound is minimum
+C+5*C*ceil(log2Q), checked before allocation. Overall O(C log(Q+1)+O) work and
O(C+Q) payload plus fixed validation. No standard opaque queue hides comparisons
or duplicates; counters record pushes/pops/peak/work/heap_extra_work. Successful
work=minimum+heap_extra_work. Smaller sufficient Q can reduce zero-initialization
work/storage without changing content. Runtime budget exhaustion publishes nothing.
Constant-size metadata preflight is outside Work: existing Domain checks use at
most16 axes; the work-bound calculation uses at most64 integer halving steps.

Depth differences fit signed64; checked unsigned64 sums and maximum-volume planning
guard aggregate arithmetic. No-lowering/settle/coverage invariants throw logic_error
for programming failures. Distinct SpillFailure reasons distinguish invalid shape/
outlets/overflow, unsupported algorithm/version and cells/storage/work/queue refusal.
No universal framework or shared WORLD error/source change. Engineering choice
confidence high locally, medium for fixed bitset/heap/private spelling; inherited
64-bit Index/size_t/8-bit-byte contract, broader execution remains unverified.
PROCGEN-002 retains intentional-v3/HyperMaze exact provenance/counterevidence; those
sources do not settle spill algorithms/APIs. U8/U9 compatibility is not user taste.

## PROCGEN-042 — Executed checks, correction and handoff

Predecessor dfeeeffd51249a4994e34c745bd143aed3a8f525 is clean and its committed
exact batch/U9 ready receipt, source/limits/records and90-test saved logs are
inspected before dependent edits. Workspace/clone/operating/workstream/feature/
generation-storage instructions apply. This is only the selected U10 task;
old queue plans/receipts/provenance archives and WORLD sources remain preserved.

Reused builds/procedural-generation GNU16.2.1/Linux Ninja Debug C++23/UBSan ON,
strict warnings-as-errors/no recovery. Every configure/build/native check goes
through /mnt/projects/cheryl-features/scripts/build -- with one job, adaptive
limits/no swap/host floor. Original build95710 and correction92917 sessions are
retained through completion. No resource75/allowance/auth refusal, queue relaunch,
subagent/service, dependency/pin/install, sibling merge or publication occurred.

Complete stdout/stderr, actual commands and immediate exits are saved under
builds/procedural-generation/logs/u10/:

| Executed command/check | Evidence | Outcome |
| --- | --- | --- |
| Wrapped clang-format -i on six new C++ files | format-1.log |0 |
| Wrapped cmake --build builds/procedural-generation --parallel 1 | build-1.log |0; standalone auto-configure/build/link |
| Wrapped ctest --test-dir builds/procedural-generation --no-tests=error --output-on-failure --verbose -R '^procgen[.]' --parallel 1 | ctest-1.log |8;99 pass/1 fail/0 skip |
| Wrapped cmake --build builds/procedural-generation --target procgen-spill-tests --parallel 1 | build-2.log |0; test-only correction |
| Wrapped CTest same options, selection '^procgen[.]spill_limits$' | ctest-2.log |0;1 pass/0 fail/0 skip,55 assertions |
| Lightweight preservation/count script, not native acceptance | source-evidence.log/source-exits.tsv/counts.json |0;20 hashes match; all prior90 names/83 case counts match |

Resolved failure: ctest-1.log retains FAIL spill_limits line266, expected1079.
The declared x-/x+/y-/y+ order inserts2 then1 from root3. Inserting1 performs a
comparison AND swap, not the single failed comparison assumed in the literal
expectation. Six heap extras are two insert comparisons/two swaps and two root
replacements; mandatory1074 +6 =1080. Only expected test counter/comment changed;
production and forest golden fixtures did not. Corrected limits recheck passes.
All other99 successful entries were not repeated. No compiler/UBSan diagnostic.

Final distinct coverage100 entries:92 cases/43544 assertions plus eight consumers
(24 demonstrations). All prior90/34503 assertions and21 demos pass unchanged.
Nine new cases add9041: literal40,saddles27,plateau34,extremes29,exhaustive8749,
generated40,invalid51,limits55,ownership16. Threshold/chain oracle checks additionally
execute without inflating harness assertion counts. New semantic consumer3/3.
This is99/100 accumulated plus corrected1/1, not a claimed100/100 initial run.
The failed attempt's five partial assertions are not counted as passing coverage.

Independent oracle: sort unique ORIGINAL levels, perform plain cardinal BFS from
only explicit outlets at or below each threshold, and record the first threshold reaching
each cell. No production priority flood/heap/neighbor helper is reused. Check every
effective value/depth, exact original witness, ranks/permutation, cardinal parents,
nonincreasing levels, decreasing ranks, actual terminal outlet and maximum original
height along every returned chain. Literal bowl/nested/saddles, plateaus, negative/
flat/extreme/unit domains,729 ternary3x2 fields times three outlet sets=2187
analyses and eight17x11 generated U8 fields match threshold connectivity exactly.

Literal7x3 middle row[0,6,-2,3,-4,8,1], outer rows all9, outlets7/13: effective
middle[0,6,6,6,6,8,1], depths8/3/10 at9/10/11, filled3/max10/volume21,peak14,
work1469. Cell11 crosses the lower saddle6 to outlet7, while cell12 uses outlet13.
Equal saddles[0,5,-3,5,0] outlets0/4: center effective5/depth8 chooses outlet0,
volume8,peak2,work1098. Flat2x2 outlet3 has parents[1,3,3,none], ranks[2,1,3,0];
U9 simultaneously reports three strict sinks. Full signed extrema yield depth
4294967295 and two-cell volume8589934590 without narrow overflow. All-outlet,
singleton and unit-axis results retain exact heights. All original inputs survive.

Resource fixture C4/Q2:12464 accounted bytes, minimum1074, actual1080, peak2 and
four pushes/pops. Exact cells/bytes/work/slots/pushes pass; one-short storage/cells/
minimum-work/pushes refuse before buffers using scoped allocation denial. Q0 early
refuses; Q1 runtime refuses. Work1074..1079 each exhausts with no partial value or
prior-result mutation. Unit C1/Q1 has no heap extras and exact minimum work passes.
Hard65536-cell flat case passes within finite work bound, not a pressure/OOM test.
Cap+1 and metadata-only capacity/maximum-volume overflow refuse before allocation;
invalid shapes/outlets/versions/indexes, copy/input lifetime and propagated bad_alloc
also pass. No huge rejected-input allocation, generic exception catch or relaxation.

Actual U8 key{42,0x67656f6772617068,7},17x11/pitch3/requested-80..120/outlets0/186:
85 filled cells/max depth135/volume1964, effective-53..118,187 pushes/pops,peak46,
work6439. Deepest cell6 follows a concrete21-cell parent path to outlet186 across
levels59/51/42/35/12/-22/-26 with strictly decreasing ranks. Text reports original/
effective/depth/parent/rank values and paths; full oracle checks every cell. No
simulation, material/asset choice, rendering or claim about physical lake fidelity.

Twenty before/after hashes preserve nine archives, five WORLD producer/header
files, U8 elevation source/receipt and U9 drainage source/receipt. Only new private
spill files, standalone CMake and active/queue records change. No unchanged shared
Engine/map suite is rerun. Ready U10 receipt reports final worker execution with
both failure and correction evidence, not automatic coordinator source acceptance.

Staged review covers all12 task files: six new private C++ sources/headers,
standalone CMake, three active records, root index link and required queue receipt.
Whitespace checks pass; complete diff is saved as staged-review.diff. No unrelated
or unfinished work is included. Since final native acceptance, only records and
receipt changed; production matches the accumulated run, limits test matches its
successful focused recheck. No additional native rerun is justified.

Stop idle after the coherent checkpoint. Publisher handles assigned-branch backup;
controller alone selects the next exact task after a clean passing drained run.
Later river extraction must inspect this real source/receipt/logs and compute
contributions for the selected raw or spill forest; these graphs are distinct.
No next task/sibling integration/background worker starts here. Confidence high
in specified tested contracts, medium in private API/future reuse. No blocker.
Other toolchains/OSes, ASan, performance/public Engine geography, physical hydrology,
lake groups, river extraction and saved formats remain unverified/unimplemented.
Existing WORLD visual/input/assets/leaks/fun, ABI/packaging and file durability gaps
remain with their archived owners.

## PROCGEN-U11 — Proposed caller before dependent APIs

Starts from clean8725d75b3d3839bef7b154ad3aa3e11419bfc439, with committed U10
ready/true batch receipt, real source/meaning and saved99/100+corrected1/1 evidence
inspected. This is the third/last assigned procedural milestone in this finite
round. Earlier immutable plans/archives/receipts and WORLD producers stay unchanged.

```cpp
auto terrain = generate_elevation(extents,key,config,elevation_budget).value();
auto route = analyze_spill({domain,terrain.heights(),outlets},spill_budget).value();
auto rivers = extract_channels(
    {domain,terrain.heights(),route.cells(),outlets},threshold,channel_budget).value();
```

Proposed engineering contract: supplied ranked effective-height forest, exact
original heights/outlets checked against graph records. No external accumulation
or opaque ID is accepted. Recompute units for ACTUAL parents, threshold inclusively,
compress selected one-in/one-out runs between source/junction/outlet nodes. Keep
filled cells in paths with explicit positive-depth flags/counts. Own exact graph
witness/results. This validates the forest contract; it does not rerun/prove U10
minimax optimality for arbitrary supplied literals. Real caller supplies accepted
Spill. API spelling is engineering/compatibility, not personal taste evidence.
Implementation and native acceptance pending at proposal time.

## PROCGEN-043 — Actual graph, source meaning and accumulation

Owner: projects/tests/procedural-generation/channels.h/.cpp, private standalone
CPU C++23. ChannelInput copies checked positive2D Domain metadata and borrows C
original signed32 heights, C SpillCell-shaped routing records and nonempty explicit outlets
for the synchronous call. Source meaning is ranked_effective_forest=1, not a claim
that arbitrary records came from a particular solver. Original values must match
the supplied height array exactly; sizes/opaque IDs alone are never provenance.
Each effective>=original, depth equals the full signed64 difference, and outlet
label names a declared in-domain outlet. Parent absent must match explicit outlet
membership exactly, with outlet=self/depth0 at roots. Duplicate/bad outlets reject.

Every nonroot parent must be in-domain and cardinal under x=axis0 fastest,
index=x+width*y (positive y advances rows), with nonincreasing effective height,
same outlet label and strictly smaller rank. Ranks must be a permutation0..C-1.
This independently validates acyclicity/actual outlet termination without trusting
the label or blindly replaying producer summaries. Outside/diagonal/wrap/sink roots
are unsupported; missing outlets reject. Unknown source meaning/algorithm/version
reject explicitly. Finite rectangular domains retain original height ticks/datum.

Invert the validated rank permutation. Initialize every contributing count to1;
in descending rank, transfer each complete unit count to its actual parent. Check
sums<=C and terminal sum=C. Complexity is linear, no repeated production chain
walks or U9 raw-slope accumulation. There is NO external contributing array or
ID-only graph binding in the API, so it cannot silently pair foreign counts with
a same-size forest. Exact original/routing/outlet data are checked and copied.

The view contract validates supplied forest consistency, not minimax optimality or
the historical identity/tie order of an arbitrary upstream solver. Literals may
provide such a valid forest directly. The real consumer uses the actual accepted
U10 Spill, retaining its complete records as witness; it prints upstream U8/U10
algorithm/version/parameters separately. No false relabeling as a priority-flood
recipe. A seed alone is insufficient; full Domain/records/threshold/source meaning/
channel algorithm version define this result's input. No saved format or indefinite
old-algorithm/migration promise. Later changed geometry/routing requires new analysis.

## PROCGEN-044 — Thresholds, nodes, paths and ownership

Threshold is an exact positive u64 integer; zero invalid, no floating rounding.
Select contributing>=threshold. A parent's count is at least its child's plus
one, so selection is downstream closed. Threshold>C guarantees empty mask/network
success; lesser thresholds can also be empty across multiple smaller catchments.
Higher threshold cannot add selected cells for a FIXED forest. This is logical
mask monotonicity, not a seed-to-seed or visual/physical quality claim.

Mask flags:1=selected;2=selected through positive original-to-effective fill,
thus values0/1/3. Unselected fill cells still exist in the complete routing witness.
Count unique selected_filled_cells globally. No dropped/teleported lake crossings:
all selected receiver steps, including equal-height fill plateaus, remain paths.
No connected lake-region grouping, river width, rainfall/discharge or navigation.

Selected incoming degree counts selected upstream cells whose parent is this
cell. Sources have zero incoming selected links, junctions at least two, outlets
are explicit roots.
Roles may combine: a root merging channels is junction|outlet; a singleton selected
outlet is source|outlet. One incoming plus one outgoing selected link is compressed
as a degree-two run interior. Always retain selected outlets as endpoint nodes.
Node IDs are dense vector indexes by ascending source cell index, scoped to this
result/threshold. Edge IDs follow ascending non-outlet upstream node ID; one outgoing
network edge per such node. These IDs are not globally persistent object identities.

ChannelEdge from/to are NODE IDs, offset/count address its range in owned path_cells.
Paths contain BOTH endpoint CELL indexes, at least2 points; every hop is exactly
the source record's parent, in bounds and cardinal. Interior points are nonnodes.
Runs stop at the next node. Runs partition selected links, so total traversal L
is linear; P=L+E because each edge repeats its starting endpoint. Shared junction/
outlet endpoints may appear in several paths. fill_points counts positive-depth
points including both endpoints within THAT edge, and can double-count shared
nodes across edges. Global selected_filled_cells stays unique. Isolated selected
outlets need no edge/path points; node coverage represents them explicitly.

Channels owns full routing witness, contributing values, mask, nodes, edges and
flat paths. Descriptor owns Domain/threshold/meaning/options/requested budgets;
measurements and charged usage are values. Copies deep-copy all vectors. Const
views expire on destruction/move/assignment; moved-from only destruction/assignment.
No input view survives return. Prior outputs cannot alias scratch or later calls.
Invalid/unsupported/budget refusal publishes no partial mask/network/counts or
mutation; only private expected-failure transport is caught. bad_alloc and internal
logic exceptions propagate. Caller retains ownership and any upstream recipe policy.

## PROCGEN-045 — Checked resources and actual work

Here C=domain cells,O=declared outlets,L=selected links,N=network nodes,E=compressed
edges,P=inclusive path points. Selected link count differs from compressed edges.

Practical hard caps C65536,N65536,E65535,P131070,payload12MiB,work2M, clamped against
ChannelBudgets. Planning checks2D/nonempty O<=C, container capacities, index/byte/work
products and sums without allocating huge arrays. Validate source view lengths
before buffers, all outlet indexes/duplicates using8192-byte fixed bitset. Admission
checks cells, base payload and known minimum work first; competing errors may report
resource/metadata before content. Callers must pass real spans, not fabricated ranges.

Peak requested12288+66*C+16*N+40*E+8*P bytes. Base includes40*C owned routing,
8*C owned units,1*C mask,8*C rank order,8*C cell-to-node map and1*C degree scratch;
8192 bitset plus4096 metadata/return allowance. Static assertions check record sizes
and metadata. Max N<=C,E<=C-O,P<=2*(C-O) yields<=9056200 bytes at C65536/O1, within
12MiB. Existing input/old outputs and allocator/runtime/exception/stack overhead
excluded: not exact RSS. After validation/accumulation/selection, determine exact
N/E/P and refuse node/edge/path/storage budgets BEFORE final network allocations.
An impossible base budget refuses before all C-sized buffers. No pressure/OOM test.

Operation units, not instructions/time: bitset init1024, outlet validation O, six
base copy/init passes6*C, validation C and parent validation C-O, accumulation C
and C-O transfers, mask/degree reset C, selected-degree scan C and L records,
node-plan scan C, final array init N+E+P, node-output scan C and N writes, edge-node
scan N, E edge writes, L receiver inspections and P path copies. Exact total
1024+14*C-O+2*L+3*N+2*E+2*P. Known minimum omits network-dependent terms; upper
1024+25*C-9*O follows partition bounds. Constant metadata preflight (<=16 Domain
axes) lies outside Work. All C/L/N/E/P loops, including skipped probes and refused
attempts, charge before work. Runtime exhaustion aborts publication. Counts/indexes
are u64 with checked additions; cardinal membership limits incoming degree to4.
Overall O(C+N+E+P+O), no recursion or quadratic per-cell chain walks in production.

Source/rank/member errors are invalid input; algorithm/source-kind/version are
unsupported; cell/node/edge/path/storage/work are budget refusals. No silent
truncation or automatic fallback. Sufficient budgets affect admission only. Empty
selection still owns/validates its complete source and units, with zero node/edge/
path budgets permitted and exact minimum work. Private helpers/types stay local;
no World/graph framework, shared Engine error/source or U8/U9/U10 mutation.

Engineering choices: recomputing inline prevents mismatched cached accumulation
without inventing persistent IDs. Full record witness costs40*C bytes but makes
later invalidation explicit. Rank inversion is smaller/simpler than another queue
for already ranked forests. Confidence high in tested contracts, medium in private
shape/roles/payload tradeoff. Archived PROCGEN-002 retains exact intentional-v3/
HyperMaze provenance/confidence/counterevidence; those do not settle channel APIs.
Recent agent APIs supply compatibility, not new personal taste evidence.

## PROCGEN-046 — Executed verification and finite-round stop

Required predecessor is clean8725d75b3d3839bef7b154ad3aa3e11419bfc439. Its committed
U10 exact batch/ready receipt, source/meaning, decisions/next and real saved native
logs are inspected. U10's test-only work-counter failure is resolved by its actual
1/1 recheck; no unresolved prerequisite is treated as accepted. All100 retained
procedural names/counts now pass together. This is only final U11 for this lane.

Reused builds/procedural-generation GNU16.2.1/Linux Ninja Debug C++23/UBSan ON,
strict warnings-as-errors/no recovery. Every native command uses shared wrapper,
one job, preserved local outputs and adaptive/no-swap/host-floor policy. Original
build71652 and targeted consumer19505 sessions complete without queue cancellation/
relaunch. Source review adds explicit <algorithm> consumer include, then rebuilds
only that target BEFORE the single accumulated run. No native failures, warnings,
UBSan diagnostics, skips or resource75/allowance/auth refusal. No dependency/system/
pin change, subagent/service, sibling merge/fetch, protected mutation or publication.

Full diagnostics, commands and immediate exits in builds/procedural-generation/logs/u11/:

| Executed operation | Evidence | Outcome |
| --- | --- | --- |
| Wrapped clang-format -i on six new C++ files | format-1.log |0 |
| Wrapped cmake --build builds/procedural-generation --parallel 1 | build-1.log |0; standalone auto-configure/build/link |
| Wrapped cmake --build builds/procedural-generation --target procgen-channel-consumer --parallel 1 | build-2.log |0; direct include only |
| Wrapped ctest --test-dir builds/procedural-generation --no-tests=error --output-on-failure --verbose -R '^procgen[.]' --parallel 1 | ctest-1.log |0;110 pass/0 fail/0 skip |
| Source-only count/hash verification, not native acceptance | source-evidence.log/source-exits.tsv/counts.json |0;23 matches/all100 prior names and92 case counts retained |

Accumulated native suite ran ONCE:110 entries,101 cases/44105 assertions and nine
consumers (28 demonstrations). Prior100/43544 passing-case assertions and24 demos
remain. Nine cases add561 assertions: literal22,lake22,thresholds61,tiny49,
generated120,malformed39,invalid44,limits186,ownership18. Independent trace/path
oracle checks execute additionally without inflating harness counts. New consumer4/4.

Independent oracle traces EVERY original source through actual receiver links,
counts unit contributions per visited cell and verifies roots/conservation. Derive
threshold mask/selected degrees/nodes separately; inspect each output path's exact
receiver links, endpoints/interior nonnodes, offsets, fill counts, stable ordering,
and once-only selected-link coverage plus isolated-node coverage. Every selected
chain stays selected to an actual declared outlet. No production rank accumulation
or compression algorithm is reused. Hand-worked fork and fill paths/units have
literal expectations. Twenty generated extractions (four U8 seeds, five thresholds)
match independent tracing, with original Elevation/Spill preserved.

Fork3x3 outlet7: units[1,1,1,2,6,2,1,9,1], threshold1 has9 selected/8 links,
7 nodes/6 edges/14 points. Nodes[0,1,2,4,6,7,8]; explicit path fixture includes
[0,3,4],[1,4],[2,5,4],[4,7],[6,7],[8,7]. Junction4 and junction|outlet7 retain
all merges. Threshold2 has4 selected/3 links/4 nodes/3 edges/6 points; threshold9
retains one source|outlet node7, threshold10/UINT64_MAX is empty success.

Fill5x1 original[5,0,0,0,0], effective all5/outlet0: actual units[5,4,3,2,1].
Threshold2 mask[1,3,3,3,0], path[3,2,1,0], three selected fill cells/edge fill points,
work1115/payload12722. The U9 raw slope result has four sinks/outlet_units1; this
network correctly has outlet_units5 from actual parents. Vertical/unit/singleton,
multiple isolated outlets, full signed32 depth and threshold equality also pass.

Exact resource fixture C9/O1/threshold2: base12882 bytes, peak13114, work1185,
minimum1149, N4/E3/P6. Exact budgets pass. One-short cells/nodes/edges/paths/peak
storage/work refuse, and base-1/minimum-1 refuse before allocation under test-only
allocation denial. Every work budget1149..1184 exhausts without changing held
results. Empty selection accepts zero network budgets/base storage/minimum work.
65536-cell unit-axis chain compresses to2 nodes/1 edge/65536 points within cap,
without recursion/pressure. Huge metadata/cap+1/overflow reject before allocation.
Malformed cycles/noncardinal/out-of-range/self parents, duplicate/invalid ranks,
uphill effective steps, wrong roots, incompatible same-size original/depth data,
shapes/outlets/threshold/source-kind/algorithm/version reject. Copies/input lifetime,
prior-output refusal preservation and propagated bad_alloc pass.

Actual height->U10->fresh units->channels: U8 key{42,0x67656f6772617068,7},17x11,
pitch3/-80..120, explicit outlets0/186. Actual spill-parent units71+116=187 conserve
all cells, unlike U9 raw-slope sink/outlet measurements. Text reports full statistics
and samples of continuous paths, marking fill crossings; complete oracle covers all:

| Threshold | Selected/links/fill cells | Nodes/edges/path points | Work/payload bytes |
| --- | --- | --- | --- |
|8 |47/45/32 |8/6/51 |3868/25406 |
|24 |29/27/17 |6/4/31 |3782/25134 |
|188 |0/0/0 |0/0/0 |3640/24630 |

Before/after23 hashes preserve nine archived records, five shared WORLD producers,
U8/U9/U10 sources and receipts. Only new private files, standalone CMake and active/
queue records change. No unchanged Engine/map suite. Ready U11 receipt records
worker execution, not automatic source acceptance or an unexecuted platform.

Staged review covers all12 task files: six new private source/header files,
standalone CMake, three active records, root index links and required queue receipt.
Whitespace checks pass; complete diff is saved as staged-review.diff. No unrelated
or unfinished changes are included. Only records/receipt changed after successful
accumulated acceptance; no additional native run is justified.

Stop idle after this third procedural checkpoint; no fourth unit, automatic public
extraction or sibling integration follows. Coordinator reviews/publisher transports.
No unresolved blocker. Other compilers/OSes, ASan, broad performance, public Engine
geography, physical hydrology/width/navigation, lake-region grouping and persisted
formats remain unverified/unimplemented. Existing visual/input/assets/leaks/fun,
ABI/packaging and file durability gaps remain with WORLD/archived owners. Later
retention/integration needs exact graph witnesses and a separately authorized caller;
river/depression changes invalidate derived channels and any dependent geography/
settlement layer rather than silently retaining foreign accumulation.

## PROCGEN-U12 — Proposed caller before dependent APIs

Read-only prerequisite gate: exact clean9517333ca3570de1fc3da6212726e23b5281093f,
committed U11 ready/true old-batch receipt and actual nonempty logs, exit0/110
passes. U9 strictly downhill raw heights, U10 minimax escape/ordered plateau
parents and U11 recomputed actual-forest unit counts are inspected; none supplies
metres, latitude, an ocean, rainfall or available evaporatable water. New batch
procgen-climate-20261024-0615 authorizes only this U12 turn. Historical archives,
WORLD producers and all earlier receipts remain immutable.

The following spelling was proposed in ordinary progress before adding APIs:

```cpp
auto summer = derive_climate(elevation, surfaces, revision, context, noon_tick, limits);
auto winter = derive_climate(elevation, surfaces, revision, context, winter_tick, limits);
```

Reversible engineering selection: an explicitly restricted local east/north frame,
owned source witnesses and separately named instantaneous versus daily solar
quantities. Circular-orbit/tilt approximation and linear absorbed/cooling targets
provide independently checkable seasonal/elevation effects without evolving weather.
A synchronous Domain/heights/surface view also accepts honest literals. New
private code is implemented; native acceptance is pending at this record point.

## CLIMATE-001 — Implemented explicit regional adapter

Private owner: projects/tests/procedural-generation/climate.h/.cpp,
RegionalFrame, ClimateInput, ClimateDescriptor and derive_climate. User requires
physical context; spelling, bounds and equirectangular approximation are our
engineering choices. Domain remains the unchanged checked x-fast finite positive2D
metadata; index=x+width*y. The supplied-view overload accepts literal height ticks,
not a forged Elevation. The generated overload retains the ACTUAL owned Elevation
values and its exact ElevationReplay including stream/config/version. Neither call
mutates U8 or adds latitude/metres to Domain.

Frame is at CELL CENTRES on selected sphere R=6371000m:

```
e = (x-(width-1)/2)*dx_metres
n = row_sign*(y-(height-1)/2)*dy_metres
latitude = centre_latitude + (n/R)*180/pi
longitude = centre_longitude + e/(R*cos(centre_latitude))*180/pi
z_metres = datum_metres + tick_metres*original_height_tick
```

Axis0 always east; row_sign is explicitly +1/increasing north or -1/decreasing.
Require finite dx/dy in[1,10000]m, tick_metres in(0,1000], datum[-12000,12000]m,
latitude[-90,90]/longitude[-180,180]deg and known row sign. Omitted frame/calendar
and zero revision labels reject, rather than silently selecting Earth-like context.
Regional support further restricts |centre_latitude|<=60deg, every sample<=65deg,
whole width*dx and height*dy <=256000m and all longitudes in[-180,180]. No dateline
wrapping, global grid, pole crossing, halo or projection accuracy claim. The
point solar helper has broader polar support than this regional mapping.

Validate all transformed heights in[-12000,12000]m and slope magnitudes<=10.
Slopes are dz/de and dz/dn: centred opposite neighbors divided by2*spacing,
one-sided edges, unit axes0. Original integer extrema are converted to double
before differences; capped scale bounds all intermediates, even for rejected
heights. No unsafe signed32 subtraction, normalization, resampling or clipping.
A constant tick increase shifts z and lapse correction; an opposite datum shift
can preserve the metric geometry without pretending the original source is equal.

ClimateRevision has nonzero caller terrain/surface labels, required to equal
context.expected_revision. Labels alone are not exact source identity or evidence
of history. Result cells own EVERY original height and exact ClimateSurface,
plus Domain/context/revisions/tick/versions/budgets and optional generated replay.
Future continuation must compare complete relevant values, not only same-size
arrays or revision labels. surface_version1 is explicit and other versions reject.
All views are synchronous and never retained. Copies deep-copy the cell buffer;
returned spans expire on owner destruction/move/assignment. Moved-from supports
assignment/destruction only. Caller synchronizes adoption and any borrowed inputs.

Surface1 admits finite/replenished water fractions separately in[0,1], sum<=1,
albedo[0,1], roughness[0,100]m and land/water response time constants[1,1e8]s.
The remainder is land fraction. Finite/replenished is a declared availability
CATEGORY, with no water volume/reservoir supplied or consumed in U12. Fractions
weight the retained response_seconds; albedo independently affects absorption.
Roughness is retained for later flow, with no hidden effect on this baseline.
U8 sea threshold, U10 depth/fill-volume and U11 unit-count/channel-crossing values
are deliberately not used as ocean, available water or evaporation sources.

Confidence high in explicit frame/source boundaries, medium in regional scale and
private API tradeoffs. Intentional-v3 evidence re-inspected locally:
eb3f81c219e779b0200c1e82e5bc7239386528e8/include/abstracts/singleton.h,
CherylE::ConcreteSingleton::construct and SuperSingleton::construct explicitly
describe typed constructor forwarding/checks. Medium confidence for typed setup,
not a preference for this climate API. Counterevidence: singleton ownership and
legacy imports do not settle geography, model parameters or numeric representation.
Recent U8/U9/U10/U11 agent APIs establish compatibility, not personal taste.
Coordination climate-weather.md CLIMATE-001..008/P1 is design evidence and scope;
this private implementation does not certify those future dynamic stages.

## CLIMATE-002 — Implemented sun and derived baseline

Identity regional_solar_baseline, model_version1, numerics_version1. SolarCalendar
requires day_seconds[60,1e6], year_days[4,4096] integer solar days, seconds_per_tick
[1,1e6], signed64 epoch_seconds and finite tilt[0,60]deg. Epoch phase0 is northern
spring equinox at Greenwich midnight. Tick is an explicit unsigned64 logical
sample time, not wall time/frame count or an evolving checkpoint. Compute positive
modulo epoch and reduce tick BEFORE multiplying, modulo year_seconds<=4096000000;
the largest intermediate is<4.096e15. Full signed epoch and unsigned tick extremes
are valid; never form epoch+tick*seconds in signed arithmetic.

Model defines prograde rotation, circular orbit, no eccentricity/refraction:

```
season = 2*pi*phase/year_seconds
D = asin(sin(tilt)*sin(season))
H = 2*pi*((phase%day_seconds)/day_seconds-.5)+longitude_radians
sun_east = -cos(D)*sin(H)
sun_north = cos(latitude)*sin(D)-sin(latitude)*cos(D)*cos(H)
sun_up = sin(latitude)*sin(D)+cos(latitude)*cos(D)*cos(H)
```

Sun vector points TOWARD sun in local ENU. |up|<=1e-14 snaps to horizon0. Point
helper supports latitude[-90,90]/longitude[-180,180]. At exact poles cos(latitude)
is explicitly0, so there is no vanishing-cosine division. Daily horizontal mean
holds the SNAPSHOT declination fixed, integrating positive a+b*cos(H): full day a,
night0, otherwise (a*h+b*sin(h))/pi with h=acos(-a/b). Daylight_fraction=h/pi;
polar positive a>1e-14 gives1, polar horizon/night0. This is not a full evolving-day
integration or annual climatological statistic. Zero tilt removes tilt seasons
but retains day/night. Below horizon there is zero direct slope illumination.

Instantaneous surface cosine is max(0,dot((-east_slope,-north_slope,1)/norm,sun)),
clamped at1 and forced0 when up<=0. No cast shadows, horizon terrain occlusion,
atmospheric scattering or integration over slope orientation through a day.
Set gain=solar_w_m2*transmission*(1-albedo), then:

```
absorbed_w_m2 = gain * instantaneous_surface_cosine
seasonal_reference_kelvin = cooling_floor + gain*daily_mean_cosine/cooling
lapse = lapse_kelvin_per_metre*(z_metres-reference_metres)
air_reference_kelvin = seasonal_reference_kelvin-lapse
instant_target_kelvin = cooling_floor+absorbed_w_m2/cooling-lapse
response_seconds = land_fraction*land_time+water_fraction*water_time
```

Source parameters are explicit bounded GAME coefficients: solar[0,2000]W/m2,
transmission[0,1], cooling_floor[250,350]K, cooling[10,100]W/(m2 K), lapse[0,.01]K/m,
reference_metres[-12000,12000]. These bounds keep derived fields/sums finite and
targets positive without silent normalization. The reference temperature is at
the stated reference altitude; air_reference applies the chosen lapse. Instant
response target is not evolved air/surface temperature. Water response is an
owned time constant for a later dynamical unit, not a claimed measured lag here.
No moisture transport, pressure/wind, rainfall, equilibrium, observed climate
normal or forecast validation is implemented. U12 proves baseline/state ownership;
CPU transport/flow proof remains the separately assigned U13/U14 responsibility.

Same model/numerics/source/context/order/budgets admitting success reproduce fields
on the tested build/platform with double/libm arithmetic. Full source values and
versions are retained; no cross-platform floating bit identity follows from
integer stream keys. No climate RNG/seed/global registry exists. U8 integer replay
is unchanged. No file format, storage/recipe policy or public extraction is chosen.
Confidence high in tested equation/ownership semantics, medium in useful game
response, unverified for calibrated physical or subjective outcomes.

## PROCGEN-047 — Climate resource/refusal contract

CaveBudgets is reused only as the existing generic cells/storage/work value triplet;
no cave semantics/dependency is imposed. Hard ceiling128 per axis,16384 cells,
8MiB accounted payload and2M work, below the batch ceiling. Checked requirements
compute8192+sizeof(ClimateCell)*C (184*C on the statically admitted build) and
64+32*C before any cell allocation; checked overflow/vector capacity rejects huge
metadata without buffers. No C-sized scratch, frontier, recursive traversal,
iterative solver, retries or random draws. Maximum accepted128x128 payload3022848
bytes and work524352; these are not process RSS/performance promises.

Work units are bounded operation blocks:64 for scalar/version/context handling,
8 per inspected source cell (including a failing cell), C for output initialization
before allocation,23 per output cell for exact copies/geometry/solar/reference/
measurements. Physical geometry is computed during validation and derivation;
both passes are covered. Metadata/shape/cap arithmetic is constant and reported0
on preflight refusal. Runtime charges guard every loop; accepted work is known
exactly, so one-short budget refuses before allocation instead of necessarily
reaching a mid-derivation exhaustion. Scalar solar helper is a separate constant
operation with no allocation/iteration; its per-cell use is charged in derivation.

8192 fixed allowance covers in-flight metadata copies/locals; static assertions
bound object metadata and record sizes. Model payload includes owned source
copies, derived output and all scratch (none per cell); excludes preexisting
inputs/held results, caller-requested copies, allocator overhead, runtime/exception
machinery and other process memory. No rejected huge-buffer allocation or host
pressure tests. Sufficient budgets affect admission/descriptors only, not content.

Expected ClimateFailure distinguishes invalid_input (shape/revision/nonfinite/
option/arithmetic), unsupported (version/projection/height-slope scope), and
budget_refused (cells/axes/storage/work). It reports charged work before refusal.
All source scalar/array values validate before output allocation. Only internal
Stop is caught: bad_alloc and programming exceptions propagate. Complete local
candidate publication leaves borrowed input and all earlier caller-owned results
unchanged. The Elevation overload adds its exact fixed-size replay before return.
Future weather advance/substep limits are not implemented or silently simulated.

## PROCGEN-048 — Executed U12 evidence and review

GNU16.2.1/Linux Ninja Debug/C++23/UBSan, existing build cache/options and shared
wrapper/one job. Complete stdout/stderr, exact commands and immediate exits under
builds/procedural-generation/logs/u12. format1/build1 pass; pre-suite source review
adds polar-horizon consistency, named identity and nonfinite/zero-solar tests,
format2/build2 pass. Nonempty discovery122 names preserves all110. Accumulated
ctest-1 passes122/122 once (112 cases/44559 then assertions, ten consumers/33 demos).
Final contract review removes valid implicit physical defaults, adds nine omitted-
context assertions, format3/targeted build3 pass; affected ctest-2 passes12/12.
Current distinct acceptance:112 cases/44568 assertions, eleven new cases/463 new
assertions, ten consumers/33 demos.0 failures/0 skips, no compiler/UBSan diagnostics
or native failure. This is a source-review correction, not a hidden failed test.
No repeated unchanged campaign or Engine/map suite. All sessions drained normally;
no resource75/auth/allowance refusal, queue cancellation/relaunch or model change.

New assertion counts: sun36,seasons17,calendar27,frame48,ridge22,surface22,
generated21,invalid179,projection39,limits36,ownership16. Source/copy/geometry/
measurement checker runs in addition to the counted assertions. Exact point sun
oracles use equinox longitude180 local noon, solstice latitude30, hemispheres,
horizon/poles/zero tilt; a20000-midpoint positive-cosine integral independently
checks the analytic daily mean. Calendar tests exercise signed epoch extrema,
u64 tick maximum and periodic boundaries. Literal ridge has1000m centre height,
6K lapse at.006K/m, centre noon solar1000W/m2, zero direct night illumination.
Row orientation, unit axes, coordinate ordering, datum compensation, dark forcing,
surface fractions/times, malformed arrays/revisions/versions and every floating
scalar's nonfinite refusal pass. Allocation denial proves invalid cells and
one-short budgets refuse before allocating and bad_alloc propagates on valid work.
Full128x128 is bounded/accepted without deliberate memory pressure.

Consumer5/5: literal equatorial3x1 coast/ridge noon/night, plus same retained U8
17x11 key{42,0x67656f6772617068,7},pitch3/-80..120, explicit10m/tick/datum0,
2km spacing/centre40N/0E/rows south. Caller selects44 replenished water cells and
one finite pool independently of elevation/sea/fill/channels. Z=-760..1180m.
Summer tick2172 mean absorbed658.5622W/m2/daily cosine.3668, centre seasonal
reference278.3888K/air273.5788K. Winter tick6492 gives302.6980/.1105,
reference265.5404K/air260.7304K. Winter midnight6480 gives0 direct absorbed and
0 sunlit cells, retaining the separate daily seasonal target. Slight midnight/
noon reference difference is continuous orbital phase, not evolved thermal memory.
All three use6048 work/42600 accounted bytes. Water time constant432000s is retained.
Three generated seeds pass source/field reductions and repeatability.

counts.json, source-evidence.json, preservation-before/after.json, commands.txt,
exits.tsv, discovery.json, build-{1,2,3}.log and ctest-{1,2}.log retain full evidence;
26 archive/shared/U8/U9/U10/U11 source/receipt witnesses match. Existing110 names
and101 prior case counts retained. No source-provenance archive, WORLD source,
predecessor receipt or sibling was modified. New ready receipt reports execution,
not coordinator source acceptance or a new platform/physical guarantee.
