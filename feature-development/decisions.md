# Procedural generation decisions

Private implemented checkpoint. The caller was proposed before implementation;
its final source and executed checks now agree with these entries. Detailed
contracts/evidence live in [agent-notes.md](agent-notes.md); readiness in
[next.md](next.md).

## PROCGEN-001 — Semantic private CPU slice

**User requirement; high confidence.** Implement one standalone C++23 utility and
text consumer under `projects/tests/procedural-generation`. Return owned maze
topology, without Engine/SDK exports, assets or rendering. The same implementation
handles 2D and true 4D. This keeps native/asset prerequisites out of U1 and leaves
the public boundary for review. No general graph or World framework is proposed.

## PROCGEN-002 — Reference influence and caller

**Inferred taste; medium confidence.** The nominated HyperMaze `Graph::Generate`
supports a topology-first operation with explicit caller setup. Intentional v3
`ConcreteSingleton::construct` supports typed setup arguments, but neither source
settles this API. Implement `ProcGen::generate(extents, key, budgets)` returning
an owned value. That ownership is an engineering choice required by safe rejection;
the private spelling remains reversible.
General endpoint/port/loopback semantics remain a separate later unit.

## PROCGEN-003 — Checked rectangular domain

**Engineering choice; high confidence, implemented.** Support 1–16 axes, axis 0
fastest, with unsigned 64-bit extents/indexes and two passage bits per axis in a
32-bit cell mask. Unit axes and one cell are valid. Fixed domain metadata avoids
allocation during validation. This is compact for the assigned orthogonal subset;
it deliberately does not express HyperMaze's variable-degree room/link model.

## PROCGEN-004 — Integer replay and independent stages

**Engineering choice; high confidence, implemented.** Specify SplitMix64 arithmetic,
numeric seed/stage/stream derivation, and rejection sampling instead of library
distribution replay. Replay v1 fixes every ordering and arithmetic choice. Budgets
control admission only. Independent stream values prevent consumption coupling;
they are neither cryptographic nor collision-free. Encoding, arithmetic and pinned
vectors are recorded. Portability covers logical results with 8-bit bytes, exact
u32/u64 integers and at least 64-bit container sizes; only Linux has executed.

## PROCGEN-005 — Bounded traversal and failure

**User requirements plus engineering choice; high confidence, implemented.** Use
iterative randomized depth-first traversal, fixed-size candidate storage, and a
cell-sized traversal stack. Preflight cells, edges, storage and minimum work;
charge traversal and every random draw at runtime. Refusal returns no partial
maze. Expected validation/budget errors remain distinct from allocation and
programming exceptions. Hard caps are 1,000,000 cells, 999,999 edges, 16 MiB
accounted storage and 100,000,000 work units. Storage includes scratch, with an
explicit overhead exclusion; it is not an RSS promise. No uniform-tree or
difficulty claims.

## PROCGEN-006 — Focused acceptance and later work

**User requirement; high confidence.** Use a stdlib-only runner with independent
topology validation, fixed replay vectors and bounded refusal cases. Run only this
standalone configuration through the shared one-job wrapper and retain complete
clone-local logs. Executed U1 Debug/UBSan acceptance passed all 11 CTest entries:
10 cases with 1,470 assertions plus the 2D/4D consumer; zero failures/skips.
Later caves/dungeons, geography, settlements and tectonic edits
inherit semantic ownership/replay questions, not this maze representation.
Presentation and public extraction await separate review.

## PROCGEN-007 — Borrowed maze traversal view

**Engineering choice; high confidence, implemented.** Borrow checked domain, const
passage masks and optional positive integer costs. All rectangular cells are
active. Costs use positive-axis edge slots; synthetic masks exercise disconnected
and cyclic cases. This separates search from generation without adding callbacks,
portals or a general graph service. Exact private spelling is reversible.

## PROCGEN-008 — Deterministic A* and resources

**User requirement plus engineering choice; high confidence, implemented.** Support
only consistent built-in zero and Manhattan heuristics. Accept the goal on a
current pop; handle improved open costs and stale entries. Order by (f,g,cell).
Bound validation, expansions, probes, total pushes, live queue, heap work and
reconstruction; check arithmetic and publish complete independently owned paths.
Invalid input, overflow, budget refusal and fully searched no-path stay distinct.

## PROCGEN-009 — Traversal measurements

**User requirement plus engineering choice; high confidence, implemented.** Report
active cells/edges, reachability, components, cycle rank, degrees, dead ends and
chosen route length/cost. Only a connected tree with an explicit simple route
gets off-route branches/cells and maximum false depth. Strict traversal recognizes
a whole-domain path with the selected endpoints, or the singleton convention.
Search success is not a difficulty or ambiguity guarantee.

## PROCGEN-010 — U2 acceptance and U3 boundary

**User requirement; high confidence.** Preserve U1 regressions and validate routes
with independent BFS/Dijkstra and literal metric fixtures. Wrapper-only Debug/UBSan
execution passes 23 entries, zero failures/skips: 22 cases contain 2680 assertions,
including all 1470 U1 assertions. Both U2 consumer demonstrations pass. This
records U2 acceptance; U3 is now authorized and implemented separately below.
Preserve proven incompatibility versus exhausted-resource distinctions.

## PROCGEN-011 — Exact traversal profile

**User endpoint requirement; engineering mapping, medium confidence.** Normalize
finite binary64 [0,1] to 65536 ticks, nearest with half upward. For C>=2 reserve
`L=2+floor(ticks*(C-2)/65536)` route cells; singleton L=1. Every cell stays active,
with exactly C-L off-route. Increasing ticks decreases that exposure while
lengthening the route and changing auto goal. This is a coverage prototype,
not perceptual difficulty or monotonic branch/depth control. Tiny domains quantize.

## PROCGEN-012 — Constructive strict endpoint and policies

**Engineering choice; high confidence for rectangular domains.** Seed axis
permutation/reflections, reserve a serpentine prefix, then attach each remaining
cell to an earlier visited neighbor. At L=C this is one spanning path with the
selected endpoints, not merely a successful search. Auto endpoints are supported;
fixed accepts the constructed pair in either orientation. Proven endpoint/color
contradictions, unavailable pairs, invalid options and resource exhaustion remain
distinct. Arbitrary fixed endpoints/masks/loops and extra growth controls are deferred.

## PROCGEN-013 — Separate replay, ownership and accounting

**User requirements plus engineering choice; high confidence.** Keep DFS v1 intact;
new `serpentine_tree` v1 retains exact request/key/domain/budgets, effective ticks,
selected endpoints, owned topology/route and measured achievements. Reuse U1
integer streams. Preflight all buffers, including U2 analysis after order scratch
is released; charge scans and every draw, including rejection. Cap at 1M cells,
999999 edges, 64 MiB accounted storage and 100M work. No partial publication or
catch-all exception conversion; budgets affect admission only.

## PROCGEN-014 — Independent evidence and review boundary

**User requirement; implemented checks, execution recorded in agent notes.** Keep
all U1/U2 tests, add pairwise-coordinate BFS comparisons, strict degree/coverage
checks and literal replay/metric fixtures. Exercise true 4D, unit axes, rounding,
endpoint refusals, overflow/budget and ownership boundaries. The text caller uses
0/0.5/1 on both domains. Final wrapper Debug/UBSan execution passes 33 entries,
zero failures/skips: 32 cases/8635 assertions plus the eight-demo consumer.
The saved logs retain a corrected literal-count failure. Stop for coordinator
review; broader portability, ASan and public extraction remain unverified.

## PROCGEN-015 — Finite cellular cave occupancy

**User technique requirement; engineering controls, medium confidence.** Add a
private 2D CPU cave value with x-fast rows, floor0/solid1 and forced solid perimeter.
Integer initial solid fill is a probability numerator over10000, not exact density.
Use 0–64 simultaneous Moore-8 steps and explicit birth/survival count masks. Report
initial and achieved counts. These small reversible controls supply a stylized
occupancy prototype; physical geology, materials and presentation are outside it.

## PROCGEN-016 — Cardinal components and empty outcomes

**User requirement plus engineering choice; high confidence.** Offer retain-all
components or keep-largest-floor only. Analyze cardinal-4 connectivity; diagonal
contacts remain separate. Largest ties keep the smallest canonical root index;
filtering solidifies other floors. Return counts before/after removal, whether
connectivity was required and whether a nonempty connected result exists. Empty
and tiny all-boundary domains stay empty, with no invented floor or nonempty
success claim. No corridor solver.

## PROCGEN-017 — Bounded ownership and cave replay

**User requirements plus engineering choice; high confidence.** Separate cave
cellular2D v1 replay preserves maze versions. Reuse checked Domain/integer streams,
not maze topology, traversal scratch or sibling storage. Bound two occupancy
buffers, labels/queue, all steps/probes/draws and return owned output. Reserve
worst-case floor traversal work before allocation; rejection still needs runtime
limits. Cap at1M cells/32MiB accounted storage/100M work. Allocation exceptions
propagate; budgets affect admission only. No persistence policy or format selected.

## PROCGEN-018 — Executed cave evidence and storage boundary

**User requirement; high confidence for tested contracts.** Independent BFS/literal
fixtures verify initialization, synchronous updates, diagonal separation and ties.
Wrapped Debug/GNU/UBSan passes10 cave entries (nine cases/844 assertions plus
consumer2/2), all33 maze entries (32 cases/8635 assertions plus consumer8/8), and
the diagnostic follow-up; zero failures/skips. Generation-storage guidance adds
replay concerns without selecting a format or sibling dependency. Stop for
coordinator review; broader portability/ASan/performance/public integration remain
unverified.

## PROCGEN-019 — Cardinal distance and threshold meaning

**User distance requirement; engineering boundary, high confidence in definition.**
Analyze a borrowed binary 2D occupancy view into owned distances/mask/summary.
Distance counts cardinal steps to a solid cell or the first outside cell; solids0,
perimeter floors1. Minimum is nonnegative signed64; thresholds0/1 include all
floors, high values can select none. Masking is a derived measurement and never
changes input. This cell metric supplies neither Euclidean radius nor physical
collision clearance.

## PROCGEN-020 — Ordered sources and bounded region analysis

**Engineering choice; implemented and verified, high confidence.** Use multi-source BFS,
seeding all solid0 sources before boundary-floor1 sources. Every cell enters once;
then reuse the queue for cardinal usable-region counts. Temporary visited flags
live in the unpublished mask and normalize before return. Account17*C+4096 bytes,
work15*C+6*usable, with checked preflight and runtime refusal. Reuse CaveBudgets
because its cells/bytes/work contract fits; no recursion, RNG or generator changes.

## PROCGEN-021 — Derived identity and retained layers

**User coordination requirement; engineering choice, medium confidence.** Retain
owned domain, exact minimum and analysis version1 with results; callers retain
exact source occupancy identity and provenance. No source lifetime is retained.
A changed occupancy invalidates its derived field even if seed/dimensions match.
Generation-storage guidance supplies future retention concerns, not a recipe,
cache/save policy or sibling import. Existing generator replay stays intact;
physics/public extraction and universal identity services remain outside U5.

## PROCGEN-022 — Focused evidence and finite-batch receipt

**User requirement; executed GNU/Linux Debug/UBSan evidence.** Independent exhaustive
Manhattan minima and cardinal floods verify literals, thresholds, regions and
refusals. All52 accumulated CTest entries pass:49 cases/22276 assertions, including
all43 prior entries and eight new cases/12797 assertions, plus three consumers.
The committed ready receipt names complete native logs; it reports execution,
not coordinator source acceptance. One coherent U5 checkpoint ends this run;
finite controller owns later admission. Other platforms/ASan/performance/public
integration remain unverified.
