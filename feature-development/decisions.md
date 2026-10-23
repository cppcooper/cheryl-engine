# PROCGEN-U1 decisions

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
clone-local logs. Executed Debug/UBSan acceptance passes all 11 CTest entries:
10 cases with 1,470 assertions plus the 2D/4D consumer; zero failures/skips.
Later caves/dungeons, geography, settlements and tectonic edits
inherit semantic ownership/replay questions, not this maze representation.
Presentation and public extraction await separate review.
