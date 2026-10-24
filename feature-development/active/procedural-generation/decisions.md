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
