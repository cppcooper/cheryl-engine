# Next unit: CPU map values and visible ranges

The next implementation unit is **U1 only**: implement a finite CPU map with
checked queries/edits and the pure visible-cell range helper. Follow
[TM-001](agent-notes.md#tm-001), [TM-003](agent-notes.md#tm-003),
[TM-004](agent-notes.md#tm-004) and [TM-006](agent-notes.md#tm-006), with logical ID
semantics from [TM-002](agent-notes.md#tm-002). All behavior here is pending;
this design assignment does not activate implementation, builds or dependency
retrieval.

At entry, read the implementation/testing guides and applicable instructions,
then inspect branch status/diffs again. Preserve any work introduced after this
handoff. The inspected source baseline was
`3542f883391d4225329dab8d5d61a5987e69c8f0`. The coordinator resolved the read-only
`.git` runner conflict that blocked the earlier design checkpoint. Implementation
remains a separately assigned unit. Do not access protected workspaces or merge
sibling features.

Proposed file owners, all under this clone:

- `projects/engine/include/cheryl/maps/tilemap.h`: CPU value types (`TileId`,
  `LayerIndex`, signed `Point`, `Extent`, `CellSize`, `LocalRect`, `CellRange`),
  `Map`, and `visible_cells`. Exact spelling is provisional; preserve semantics.
- `projects/engine/src/maps/tilemap.cpp`: checked dense storage and range math.
- `projects/engine/tests/unit/src/tilemap.cpp`: focused behavioral cases, using
  no renderer, resource manager, asset file or native graphics API.

Map construction takes nonzero `{columns,rows}` and layer count, checks storage
products/limits before allocation, and initializes all cells to ID zero. `get`
returns a value; `set` changes one checked cell without allocating. Reject invalid
points/layers and preserve all cells. The map owns neither placement nor a
palette. `visible_cells` intersects validated double-precision map-local bounds
before integer conversion and returns half-open cell ranges. Keep CPU headers
independent of assets/rendering/runtime headers. No binding table, submission
adapter, demo controls, auto-tiling, persistence, resizing or chunking belongs to U1.

Required checks for the unit, once its execution is authorized:

1. Construct a 3×2 map with two layers; all 12 cells are empty. Paint/read/erase
   each extreme corner on each layer, including a nonzero arbitrary logical ID.
   Assert exactly the addressed cell changes. The map deliberately does not
   validate renderer palette membership.
2. Reject negative coordinates, column==width, row==height and invalid layer.
   Establish valid setup first and verify the map remains unchanged. Reject zero
   dimensions/layers and overflowing or unaddressable products before allocation;
   choose a deterministic product-overflow input instead of exhausting memory.
3. Edit A and a deep copy while B remains unchanged; destruction of A must not
   affect querying/editing B. No static active-map state is permitted.
4. Check every TM-004 range example, all exterior/touching edges and corners,
   whole-map containment, zero width/height, fractional edges, extreme finite
   off-map rectangles, and invalid/infinite/NaN rectangles, zero extents,
   nonpositive cell sizes or overflowing footprint products. Include a small
   exhaustive comparison against positive-area cell/rect
   intersections to detect off-by-one errors independently of the floor/ceil code.
5. Inspect public CPU includes and actual test registration. The current
   `tests/unit/CMakeLists.txt` glob feeds new unit files into `tests-engine`;
   `tests/CMakeLists.txt` and `cmake/CherylTests.cmake` own the runner. Do not assume
   a filename or aggregate alone means the new cases execute.

For a later authorized native run, inspect/reuse compatible clone-local build
configuration first, acquire the workspace's shared feature-build lock and bound
parallelism as the workspace policy requires. Select only the new `tilemap.*`
cases in the actual `tests-engine` runner. All dependency submodules, including
assets, were uninitialized during this design phase; inspect prerequisites and
the authorization of the later assignment before any retrieval or configure.
Do not borrow protected caches or turn this handoff into background execution.

Exit when the CPU contract and focused cases are implemented and the permitted
checks' actual outcomes are recorded. If native execution is unavailable, report
that limit explicitly; static review cannot establish passing behavior. Update
both decision views for material changes, commit only the coherent assigned
unit, and hand it back for selection of the next bounded task. U1 alone does not
establish layered rendering, visual edit feedback, artwork availability or the
first consumer's acceptance.
