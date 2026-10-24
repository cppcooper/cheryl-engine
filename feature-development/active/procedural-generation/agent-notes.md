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
