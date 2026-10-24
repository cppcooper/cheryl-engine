# Procedural-generation next action

PROCGEN-U6 is implemented and verified on `feat/procedural-generation`, based on
clean U5 `483493998776586a1d0d75f1683d8dec57cbcf55` and its ready receipt/logs.
Private `connect_cave` returns complete owned replacement plus exact source bytes,
Domain/version, carving/connection measurements and usage. It preserves all floors
and solid perimeter, deterministically joins nonempty interior components, and
leaves empty/one-component inputs unchanged. Existing cave/maze replay and U5
clearance are untouched. Recompute clearance explicitly for changed occupancy.

Preserved GNU16.2.1/Linux Debug/C++23/UBSan build passes. Accumulated nonempty
CTest ran ONCE after final code: **61 passed,0 failed,0 skipped**,57 cases with
26849 assertions plus four consumers. All52 prior entries/22276 assertions remain;
eight tunnel cases add4573 assertions. New consumer passes2/2, retaining earlier
maze8/8,cave2/2,clearance2/2. No UBSan diagnostics. Complete logs/immediate exits:
`builds/procedural-generation/logs/u6/` (build-1/build-2/ctest-1 plus formatting).
PROCGEN-023–026 match between decisions.md and agent-notes.md.

Ready receipt is committed with this checkpoint:
`feature-development/queue/three-commits-20261024/PROCGEN-U6.json`.
This reports worker execution, not automatic coordinator source acceptance.
Next action belongs to the finite controller: after this exact run drains,
validate clean descendant checkpoint/receipt/logs and select preassigned U7
(first finite dungeon rooms/corridors). This worker starts no next unit, sibling
work, background agent or publication command. Publisher handles assigned-branch
transport; no integration is performed here.

Assumptions/confidence: high in explicit tested floor/perimeter/connectivity/
least-immediate-carve/ownership/resource contracts; medium in reversible private
API/cap choices. No unresolved blockers. Each greedy search minimizes its immediate
new solid-cell cost, not a global tunnel network. Ties are cost/index then first
strict parent in canonical neighbor order. Route edges and carved cells remain
separate. A boundary floor is a known contradiction; version unsupported, malformed
input invalid, work exhaustion a resource refusal, never impossibility proof.
All-solid/unit-axis solid inputs succeed without fabricated floor, connected=false.

Accounted peak4096+139*C is not RSS; hard65536 cells/16MiB/100M work. Heap <=5*C
pushes per search, at most original-components-minus-one searches, runtime work
bounds repeats/comparisons/flood/reconstruction. Exact work admission can vary
with stdlib heap comparison counts; sufficient-budget logical replay is stable.
Other platforms/compiler, ASan, large-domain performance and public integration
remain unverified. No width/circular-actor/physical-geology guarantee.

Retained layers need exact original Domain/source occupancy plus transform family/
version and any upstream cave replay. Source changes invalidate old clearance;
new replacement is not the same base merely because seed/dimensions match.
No persisted recipe, save policy, hash/witness-file format or sibling dependency
is selected. U7 remains queued; advanced mazes, geography, settlements, tectonics,
ports/masks and broader AI/physics remain future bounded scopes.
