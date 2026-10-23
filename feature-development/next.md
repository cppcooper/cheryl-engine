# PROCGEN-U2 current handoff

Status: U2 implemented and focused native checks passed, based on accepted U1
`323914810d6a9e4cd6f870baaaa5916c50747c8a`. Ready for coordinator review after
the commit containing this record. No further unit is activated.

Private owners: `projects/tests/procedural-generation/traversal.h`,
`traversal.cpp`, `traversal-internal.h`, `metrics.cpp`, and `traversal-tests.cpp`.
The existing text consumer now generates a U1 maze, finds an entrance-goal route
and reports topology/traversal metrics. PROCGEN-007–010 in both decision views
record exact contracts, assumptions, source provenance and evidence.

Implemented: borrowed const orthogonal masks, optional checked positive integer
costs, deterministic A* with built-in zero/Manhattan heuristics, checked priorities,
bounded duplicate queue/work/reconstruction, independently owned complete paths,
and explicit found/no-path/invalid/overflow/budget outcomes. Metrics cover all
active cells, components/reachability, cycles, degree distribution/dead ends and
chosen route length/cost. Tree-only off-route metrics are absent for cycles or
disconnected graphs. Strict recognition checks a whole-domain simple path with
selected endpoints, including the singleton convention. No difficulty score.

Executed: preserved Debug/GNU16.2.1/UBSan standalone configuration, one shared-wrapper
job. CTest `^procgen[.]` with `--no-tests=error`: **23 passed, 0 failed, 0 skipped**.
22 unit cases contain 2680 assertions: all 1470 U1 assertions plus 1210 U2.
The consumer separately passed 2/2 demonstrations. Complete diagnostics remain
in `builds/procedural-generation/logs/u2/`; U1 logs and incremental cache remain.
No resource refusal occurred. Retain live sessions on normal queue waits; genuine
admission refusal means checkpoint idle without unchanged retry.

Consumer 4D output remains 36 cells/35 edges/ten fourth-axis edges. Its route has
8 edges/cost8, but 27 off-route cells and false depth26; strict recognition is
false. A literal winding 4D full-coverage path is recognized as strict. Independent
BFS and small Dijkstra verify routes/costs; tests cover improved goal candidates,
stale queue entries, cycles/disconnection, exact budget/integer boundaries,
invalid inputs, ownership and previous-result preservation.

Assumptions/confidence: high for the finite orthogonal positive-integer contracts;
medium for these exact private types' eventual reuse. Only known consistent
heuristics are accepted; portal links/arbitrary callbacks are unsupported.
Storage budgets count requested result/scratch/queue buffers plus fixed metadata,
not exact RSS, allocator/runtime overhead, borrowed input or old owned results.
Search caps: 1M cells/16M edges/64 MiB accounted storage/100M work, 1M settled
expansions, 1M live queue entries and 16M total pushes. U1 caps/replay are unchanged.

Unresolved blockers: none. Material gaps: other compilers/OSes, ASan, large-domain
performance, Engine/SDK exposure and player difficulty/playtesting are unverified.
No global configuration, new dependency/pin, protected/sibling mutation or export.
Checkpoint transport remains with the coordinator publisher on the assigned ref.

Next concrete action: coordinator reviews this U2 checkpoint, its caller,
PROCGEN-007–010 and saved logs. Proposed U3: checked strict labyrinth construction,
a useful traversability mapping and selected controls. Separate coverage, route
length, fixed endpoints and ambiguity; report requested/achieved constraints.
Full coverage/endpoints/masks can conflict: distinguish proven contradiction from
bounded construction failure. U3 is not implemented or automatically started.
General graph/portal/loopback mazes, presentation, caves, geography, settlements,
tectonic modification, broad AI and physics remain later separate assignments.
