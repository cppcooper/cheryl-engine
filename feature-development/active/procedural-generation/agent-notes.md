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
