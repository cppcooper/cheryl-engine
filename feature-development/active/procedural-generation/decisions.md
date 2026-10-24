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
