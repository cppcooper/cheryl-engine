# Active procedural-generation decisions

Continues [immutable PROCGEN-001–030](../../features/procedural-generation/decisions.md)
after reviewed WORLD-U1 promotion e9289fcb992feddf85deb942a560562a54971f78.
Detailed contracts/evidence: [agent-notes.md](agent-notes.md); handoff: [next.md](next.md).
U8 is outside the completed finite queue. No archive hashes/history are rewritten.

## PROCGEN-031 — Integer elevation and separate scale/relief

**User requirement plus engineering choice; high confidence in definition.**
Generate an owned finite2D signed32 height field with integer lattice spacing and
inclusive height interval. Units are ticks above arbitrary zero, independent of
horizontal cells. Use one final floor-rounded bilinear interpolation; report
actual min/max/range without normalizing. Outside bracketing anchors, unit axes
and nonperiodic boundaries are explicit. Spacing controls sample pitch, not a
per-seed visual complexity score. No octave is needed for this first useful proof.

## PROCGEN-032 — Sea level is independent analysis

**User requirement; high confidence.** Water means height<=sea level, including
exact equality. A scalar semantic query and bounded allocation-free summary
return land/water counts; generation replay stays unchanged when sea level moves.
There is no guaranteed land percentage, connected ocean, lake filling, drainage,
physical aspect ratio or material/asset choice. Higher thresholds cannot reduce
water count for the same owned field; visual complexity/physical realism is not
measured. This boundary is small and reversible.

## PROCGEN-033 — Isolated replay, errors and bounded resources

**Engineering choice; high confidence locally, portability unexecuted.** New
lattice_bilinear v1 retains Domain/config/seed-stage-stream/budgets. Derive a stable
child stream and charge all raw rejections. Peak4096+4*C+4*L, work7*C+2*L+draws;
hard65536 cells/spacing4096/2MiB/10M work. Family-local failures preserve unchanged
WORLD shared producers and old errors. Reject before huge allocation; runtime
exhaustion publishes nothing. Allocation/programming exceptions propagate. Sufficient
budgets change admission only. No persisted recipe/public extraction is selected.

## PROCGEN-034 — Verification and preserved integration evidence

**User requirement; executed GNU/Linux Debug/UBSan evidence.** All80 accumulated
CTest entries pass:74 cases/27681 assertions plus six consumers. Prior71 entries/
27265 assertions remain; eight elevation cases add416. New consumer passes3/3,
including water20->77 after sea0->40 on unchanged heights. Nine archive and five
shared-producer hashes match. Only private targets change; no unchanged WORLD/
map suite is rerun. Receipt reports execution, not broader source/platform acceptance;
visual/leak/fun/durability gaps remain with their owning records.

## PROCGEN-035 — Strict descent with explicit outlets

**User requirement and engineering tie choice; high confidence.** Analyze supplied
signed32 heights on finite cardinal2D cells. Explicit outlets stop even when lower
neighbors exist; other cells select greatest strictly positive original-height
drop, ties by smallest row-major neighbor index. Outside is absent. Empty outlets
are valid, duplicates reject. Flats/local minima remain sinks. Acyclic links do
not prove outlet drainage; sea level and U8 replay stay independent.

## PROCGEN-036 — Owned catchments and contributing cells

**User requirement plus private engineering choice; high confidence.** Terminal
cell index identifies its catchment; outlet/sink kinds remain distinct. Every
cell contributes one unit along its selected chain. Return owned receivers,
terminal labels and counts, with exact copied source heights/outlet membership
as provenance. FIFO topological accumulation and reverse label propagation avoid
quadratic production walks. Counts are combinatorial, without rainfall/discharge
or depression filling. Retained source costs4 bytes/cell and supports later checks.

## PROCGEN-037 — Bounded analysis and refusal

**Engineering choice; high locally, broader execution unverified.** steepest_cardinal
v1 retains Domain/options/caller budgets. Reuse private budget/Domain/Work types;
family-local errors leave shared WORLD producers untouched. Peak12288+48*C bytes,
actual work1024+13*C-2*O+2*links; hard65536 cells/4MiB/2M work. Validate outlets
before cell buffers, charge failed neighbor probes and runtime exhaustion. Refusal
publishes nothing; allocation/programming exceptions propagate. Payload is not RSS.

## PROCGEN-038 — Executed proof and finite handoff

**User requirement; executed GNU/Linux Debug/UBSan.** All90 entries pass once:
83 cases/34503 assertions plus seven consumers. Prior80 names/counts remain;
nine cases add6822 assertions, new consumer2/2. Exhaustive2187 tiny analyses and
eight generated fields agree with independent chain tracing. Seventeen preservation
hashes match. Ready queue receipt reports worker execution; coordinator acceptance
and later-task selection remain separate. No physical/platform/public integration
claim or unchanged Engine/map rerun follows.

## PROCGEN-039 — Explicit minimax escape levels

**User requirement and engineering construction; high confidence locally.** Return
the minimum, over cardinal paths to explicit outlets, of the greatest original
height on that path. Outside is absent; missing/duplicate/bad outlets reject.
Effective heights never lower original values; outlets keep their own heights.
Priority flood v1 is deterministic, without RNG, sea inference, erosion or lake
simulation. Filled area is a cell count; volume is summed cell-height ticks.

## PROCGEN-040 — Plateau routing and exact provenance

**User requirement plus private engineering choice; high confidence.** Frontier
key is (effective height,index); discover once from a settled parent. Unique settle
ranks strictly decrease along parents to actual declared outlets, including flats.
Owned records retain original/effective/depth/parent/rank/outlet plus Domain/options/
budgets. This supplies exact input evidence without a borrowed source or saved
format. U9 remains strict original-height descent; its sink counts do not describe
this separate forest. Paths minimize escape level, without shortest-hop promises.

## PROCGEN-041 — Bounded charged frontier

**Engineering choice; high locally, medium for API tradeoff.** Fixed binary heap
has Q<=C slots, no stale/duplicate entries, C pushes/pops. Peak12288+40*C+8*Q bytes;
minimum work1024+12*C+Q plus charged replacements/comparisons/swaps. Cap65536 cells,
4MiB/8M work and65536 queue slots/pushes. Validate outlets before buffers, preflight
depth/volume arithmetic, enforce runtime work/frontier limits and publish nothing
on refusal. Sufficient budgets change admission/usage only; payload is not RSS.

## PROCGEN-042 — Independent acceptance and finite stop

**User requirement; executed GNU/Linux Debug/UBSan.** Accumulated99/100 plus a
corrected1/1 limits recheck establish100 distinct passes:92 cases/43544 assertions,
eight consumers. Test-only work expectation1079->1080 accounts for stable insertion
order; production unchanged. Threshold BFS/chain oracles cover2187 small analyses,
eight generated fields, extrema and refusals. All prior90 names/counts and20
preservation hashes match. Receipt reports execution; controller alone selects
subsequent work, with broader platform/physical/public acceptance still open.

## PROCGEN-043 — Accumulate the actual accepted forest

**User requirement and engineering boundary; high confidence locally.** Accept a
finite ranked effective-height forest plus exact original heights/explicit outlets.
Validate shape, matching heights/depths, cardinal parents, unique ranks and roots.
Recompute own-inclusive units from those parents; external counts/opaque IDs are
not accepted. Full owned graph witness identifies the actual input. This validates
a supplied forest, without certifying arbitrary literals as U10 minimax output.
The real caller supplies accepted Spill; U9 raw counts remain separate.

## PROCGEN-044 — Continuous threshold networks with fill crossings

**User requirement plus engineering representation; high confidence.** Positive
inclusive unit threshold selects a downstream-closed mask; above total cells yields
empty success. Compress one-in/one-out runs between selected source/junction/outlet
nodes. Stable IDs are result-local, nodes sorted by cell, edges by upstream node.
Inclusive cardinal paths copy every actual parent step, keeping positive-fill flags
and per-edge counts. Isolated outlets remain nodes. These are semantic catchment
proxies, without physical discharge, width, navigation or lake simulation.

## PROCGEN-045 — Charged linear extraction and refusal

**Engineering choice; high locally, API tradeoff medium.** threshold_runs v1 owns
routing/units/mask/network. Peak12288+66*C+16*N+40*E+8*P bytes; known work minimum
1024+14*C-O, exact adds2*L+3*N+2*E+2*P. Caps65536 cells/12MiB/2M work plus explicit
node/edge/path budgets. Preflight base buffers, then exact network counts before
output allocation. Rank validation proves acyclicity; runs partition selected links.
Refusal publishes nothing and exceptions propagate. Payload is not RSS.

## PROCGEN-046 — Verified finite-round checkpoint

**User requirement; executed GNU/Linux Debug/UBSan.** All110 accumulated entries
pass once:101 cases/44105 assertions plus nine consumers (28 demonstrations).
All100 prior names/counts and23 preservation witnesses match. New fork/fill literals,
independent tracing, generated threshold profiles, malformed graphs and exact resource
boundaries pass. The worker's third milestone ends this finite lane; receipt records
execution, not automatic source/platform acceptance. No further unit, public extraction
or sibling integration is authorized by this completion.

## CLIMATE-001 — Explicit regional physical adapter (U12)

**User boundary; implemented private choice.** Require a valid metric/height,
row/north, restricted geographic and integer-calendar frame; omitted physical
context rejects. Own exact heights/surfaces/revisions and the generated U8 replay
when supplied. A <=256km local approximation excludes polar maps/dateline wrapping,
while the point sun helper handles poles. This avoids invented geography at the
cost of explicit caller setup. Confidence high in checked boundaries, medium in
chosen regional scale; no personal API preference is inferred.

## CLIMATE-002 — Derived targets, separate from weather (U12)

**User boundary plus engineering model; implemented.** Circular-orbit/tilt sun
geometry derives instantaneous slope illumination and daily horizontal seasonal
targets. Explicit absorbed/cooling and lapse parameters distinguish reference-level,
elevation-corrected and instantaneous response targets. Caller water fractions
produce a retained thermal timescale; roughness stays input context. These are
bounded game targets, with no evolved weather or measured normals. Confidence high
in tested equations, medium in usefulness, unverified for physical calibration/fun.

## PROCGEN-047 — Owned and charged climate candidates

**Engineering choice; high locally.** regional_solar_baseline model/numerics1
owns complete source/context/derived cells. Caps128x128/8MiB/2M work, actual
peak8192+184*C and work64+32*C, no cell scratch. Validate source values before
allocation and charge rejected cells. Expected invalid/unsupported/budget refusals
publish nothing; allocation/programming exceptions propagate. Sufficient budgets
affect admission only. Floating replay needs the same build/platform/order;
integer stream keys imply no broader floating guarantee.

## PROCGEN-048 — Independent U12 checkpoint

**User requirement; executed GNU/Linux Debug/UBSan.** One accumulated122/122 pass
and a source-review-driven12/12 climate recheck establish112 cases/44568 current
assertions and ten consumers/33 demonstrations. All110 prior names/counts and26
source/archive/receipt witnesses remain. Independent solar scalars/integral,
ridge/coast, units/calendar/ownership and refusal checks pass. The receipt is
execution evidence. Controller alone may select U13; transport, dynamics,
physical/visual/platform/public acceptance remain separate.

## CLIMATE-003 — Conservative known-face transport (U13)

**User progression; implemented private choice.** Transport vapour/cloud water
with shared-face first-order donor retention, using explicit east/north winds,
closed or periodic boundaries and the retained U12 metric area. Reference air
temperature instead uses an intensive advective difference update that preserves
constants during convergence. This gives auditable mass transport before dynamic
pressure, trading feature sharpness for bounded nonnegative updates. Confidence
high in executed discrete contracts, medium in game usefulness; detailed evidence follows.

## CLIMATE-005 — Water and elevation cause rainout (U13)

**User mechanism; implemented engineering model.** A clamped linear column-capacity
proxy diagnoses local temperature from transported reference temperature and one
lapse correction. Bounded condensation/cloud evaporation/rain transfers move actual
available water. Finite evaporation depletes an explicit reservoir; replenishment
is reported external supply and rain is export. Ridge/gap experiments test causal
removal. No RH/dew-point, latent-heat, real ocean or hydrodynamic claim. Confidence
high in accounting, unverified in physical/coarse rain-shadow calibration.

## CLIMATE-006 — Fixed ticks and complete bounded advances (U13)

**User limits plus engineering schedule.** Accept1..10 integer ticks and choose
the smallest1..64 substeps satisfying actual outgoing water AND incoming intensive
fractions per cell. Account unsuccessful trials, every copy/pass/face and full
candidate/report payload. Refusal preserves prior owners; no wind clipping or
skipped time. Own exact baseline/model/state and latest forcing interval; floating
replay is build/platform/order scoped. Confidence high in bounded publication,
medium in numerical policy; no evolving pressure solver is selected here.

## PROCGEN-049 — Executed conservative transport checkpoint

**User acceptance; executed GNU/Linux Debug/UBSan.** All135 accumulated entries
pass once:124 cases/45176 assertions plus eleven consumers/38 demos. New608
assertions include literal transport/transfer oracles, terrain/gap/mirror rainout,
exact resource limits, runtime refusal, grouping and ownership. All122 previous
names/counts and33 preservation witnesses match. Confidence high in checked
mechanisms, unverified for physical calibration, visuals, ASan/portability/public
integration. Receipt reports execution; only controller may select U14.
