# Procedural-generation next action

PROCGEN-U7 is implemented and verified on feat/procedural-generation from clean
U6 `fb0b0ffe758325af6424f8173a9aa852f207e81b` and its ready receipt/logs. Private
2D dungeon output owns half-open room rectangles/local IDs, logical chain links,
binary occupancy, versioned replay, requested/achieved metrics and charged usage.
Room size/count/spacing/margin and finite placement attempts have explicit effects.
Existing cave/maze/clearance/tunnel replay and test sources remain unchanged.

Preserved GNU16.2.1/Linux Debug/C++23/UBSan build passes. Accumulated nonempty
CTest ran ONCE: **71 passed,0 failed,0 skipped**,66 cases/27265 assertions plus
five consumers. All61 prior entries/26849 assertions remain; nine new dungeon
cases add416 assertions. New consumer2/2; total consumer demonstrations16/16.
No compiler warning or UBSan diagnostic. Full diagnostics/immediate exits:
`builds/procedural-generation/logs/u7/` (format-1/build-1/ctest-1 and exits.tsv).
Normal queue wait completed in original session27375; no resource refusal/retry.
PROCGEN-027–030 match between compact decisions and detailed agent notes.

Ready receipt is committed with this checkpoint:
`feature-development/queue/three-commits-20261024/PROCGEN-U7.json`.
It reports worker execution, not automatic coordinator source acceptance.
This is the third/final pinned milestone in this lane: U5 clearance4834939,
U6 tunnelsfb0b0ff and this U7. Next concrete action: coordinator/controller review
of these coherent source checkpoints, receipts and saved logs after this run
fully drains. Publisher handles assigned-branch transport. No fourth task,
sibling work, background agent, recurring loop or integration is started here.

Assumptions/confidence: high in explicit tested rectangles/margin/spacing, replay,
occupancy connectivity, ownership and refusal semantics; medium in private API
and first placement/corridor choices. No unresolved blockers. Underfilled placement
is explicit attempt_limit success with actual achieved count, possibly zero;
minimum-room nonfit is a cheap known contradiction, general packing feasibility
is not solved. Budget exhaustion publishes nothing and proves no infeasibility.
Same31x21/key42 demonstrations: small2..4 rooms achieve8/8,84 room+54 corridor
floor; large5..7 rooms achieve2/4 after256 attempts,98+8 floor. Both nonempty floors
are connected; the latter does not imply four rooms are mathematically impossible.

Margin>=1 preserves outer walls. Room spacing applies before corridors, which
may cross rooms/each other and add physical adjacency/cycles beyond the logical
chain. Independent occupancy BFS, not graph connectivity alone, verifies floors.
IDs are local to one result; no cross-seed/persistent entity identity is promised.
Hard64 rooms/8192 attempts/65536 cells/4MiB/100M work; peak4096+10*C+80*R is not
RSS. Every raw rejection draw, pair test, carve and flood operation is bounded.
Attempts affect content, sufficient budgets admission only; child stages are
fixed numeric derivations, with no global RNG or mutable registry.

Other compilers/OSes, ASan, large-domain performance, physical/perceptual quality,
public Engine/SDK and storage integration remain unverified. Retention needs exact
algorithm/version/Domain/config/seed-stage-stream and exact base identity before
edits; no saved recipe, save policy, hash/witness-file format or adapter is selected.
Changed geometry invalidates old derived clearance. Later layout/topology, maze
controls, quests/stories, settlements/geography/tectonics stay backlog. No keys,
locks, encounters, masks, stairs/3D, renderer/assets or broad AI/physics is added.
