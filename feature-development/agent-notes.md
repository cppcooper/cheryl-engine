# PROCGEN-U1 agent record

## Authority and inspected state

Assigned clone only: `projects/procedural-generation`, branch
`feat/procedural-generation`, clean baseline
`3542f883391d4225329dab8d5d61a5987e69c8f0`. Initial index and worktree diffs were
empty. No feature-development records or standalone procgen target existed.
Read workspace AGENTS, autonomy policy, worker assignment, brief, backlog,
procedural workstream (including its new reference section), W017, clone AGENTS,
workflow, implementation, investigation, architecture, testing, Git and applicable
documentation instructions. User delegation authorizes this full bounded unit.

## PROCGEN-001 — Scope

User requirements: semantic owned outputs, deterministic bounded CPU generation,
one implementation for N-dimensional rectangular orthogonal perfect mazes,
small consumer/tests, private boundary, coherent local commit. No settings
dependency; no sibling integration, protected-source changes, root dependency
changes, pin changes, downloads, installs, native Engine build or presentation.
Publication transport belongs to the coordinator publisher.

## PROCGEN-002 — Source evidence and proposed caller

Read-only college-ports HEAD observed:
`3fddec642ce06737c9600f5036198a0826d56f55`. Scoped status and graph diff against
HEAD were empty. Graph files' latest local commit:
`66e4c8072362e9f34014b1e5cdd70074d2dd3a1d` (`OpenAI Codex`, legacy import).
The user's nomination supplies authority; the import attribution does not prove
the original author's preferences.

Paths relative to `/mnt/projects/college-ports/projects/hypermaze`:

- `include/hypermaze/world/Graph.h`: `Graph::Generate`, `GraphSet`, `Node`,
  `NodeLink`, endpoint-local `N1Dir`/`N2Dir`. Topology-first influence: high
  confidence given explicit nomination. Caller/naming inference: medium.
- `src/world/Graph.cpp`: `Graph::Generate(GraphSet&, ushort N, float C)` uses N
  family-expansion rounds, variable branching/dead ends and loopbacks. It is not
  an N-coordinate-axis generator. `NodeLink::SetAlignment` gives opposite local
  directions; `IsLooped` means a self-loop specifically. Later general links
  must distinguish self-loops, parallel incidences and multi-node cycles.
- `src/world/HyperMaze.cpp`: inspected constructor and traversal/EnterNode boundary
  only. `HyperMaze::HyperMaze` creates a graph then enters a node;
  `HyperMaze::Traverse` switches node/link spaces; `EnterNode` consults separate
  terrain storage. No actor/control/UI/art implementation was investigated.

Counterevidence to mechanically adopting the reference: raw owning pointers,
maze-deletion TODO, clock/global RNG, pointer-set iteration, graphics includes,
narrow counters and assertion-only validation conflict with U1. No protected
project build, dependency operation, configuration change or output mutation ran.

Supplementary local intentional v3 evidence:
`eb3f81c219e779b0200c1e82e5bc7239386528e8`,
`include/abstracts/singleton.h`, `CherylE::ConcreteSingleton::construct`,
`SuperSingleton::construct`, `SuperSingletonDerived::construct`. Explicit comments
describe typed constructor forwarding and compile-time checks. The squash message
records deliberate singleton revisions alongside WIP imports. This supports
typed setup (medium confidence), not global ownership, maze API, RNG or a universal
generator. Recent agent APIs are compatibility evidence only.

Proposed caller, recorded before accumulating implementation (now implemented in
`projects/tests/procedural-generation/consumer.cpp`):

```cpp
const ProcGen::Budgets budgets{.cells = 4096, .edges = 4095,
    .storage_bytes = 128 * 1024, .work = 1'000'000};
const ProcGen::StreamKey key{.seed = 42, .stage = 0x6d617a65, .stream = 7};
const std::array<std::uint64_t, 2> plane{6, 4};
const std::array<std::uint64_t, 4> volume{3, 2, 2, 3};
auto maze2 = ProcGen::generate(plane, key, budgets);
auto maze4 = ProcGen::generate(volume, key, budgets);
// Check expected failures; each successful value owns its full topology.
// Inspect axis passages and replay descriptor; no viewer/slice in U1.
```

This remains a private engineering boundary. Value results replace legacy mutable
GraphSet/raw ownership because rejection must preserve caller-owned prior results.

## PROCGEN-003 — Implemented domain contract

1–16 axes, unsigned 64-bit extents/indexes, axis 0 fastest:
`stride[0]=1`, `stride[a]=product(extents[0..a))`,
`index=sum(coordinate[a]*stride[a])`. An axis step changes exactly one coordinate
by one. Fixed-size metadata validates empty/oversized dimension sets, zero extents
and product overflow before allocation. Unit axes and one cell remain supported.
Domain methods check coordinates, indexes, axes and direction values.

Owner: `projects/tests/procedural-generation/maze.h` and `maze.cpp`, `Domain`,
`Maze`, `generate` and `replay`. `Domain::checked` is allocation-free and can
represent mathematically valid domains above generation admission caps. Every
extent is a numeric `uint64_t`; there is no text parser or signed extent API.
Index range is `[0,cells)`. `coordinates` returns a fixed 16-word value with
inactive entries zero. `index` requires exactly D coordinates, each in range.
The validated product proves the sum/neighbor arithmetic stays in range. Empty
dimensions, more than 16 axes, zero extents, unknown directions, invalid indexes
and coordinates reject explicitly. A count of UINT64_MAX is mathematically valid
but refuses generation on checked storage arithmetic before allocation.

`Maze::passages` borrows const masks, with negative axis a in bit `2*a` and
positive in `2*a+1`. `Maze::neighbor` returns checked semantic neighbor indexes;
an absent passage or domain boundary returns an empty optional. All generated
passages are reciprocal; inactive bits are zero. No embedding/sprite/tile identity
is implied. Copies own independent buffers; moves transfer the buffer. A moved-from
Maze supports destruction/assignment only. Borrowed views expire on destruction,
move or assignment of their owner. No mutable output/scratch alias is exposed.
Separate calls share no state; callers synchronize their own assignments/views.

## PROCGEN-004 — Implemented stream/replay contract

Owner: `random.cpp`, `derive_stream_state`, `IntegerStream`, `sample_below`; replay
descriptor in `maze.h`. Every operation below uses unsigned 64-bit arithmetic
modulo 2^64 and logical right shifts. Define:

```text
mix(x):
  x = (x xor (x >> 30)) * 0xbf58476d1ce4e5b9
  x = (x xor (x >> 27)) * 0x94d049bb133111eb
  return x xor (x >> 31)
derive(seed,stage,stream) = mix(seed xor mix(stage xor 0x6a09e667f3bcc909)
                                  xor mix(stream xor 0xbb67ae8584caa73b))
next(): state += 0x9e3779b97f4a7c15; return mix(state)
```

Canonical key encoding is the ordered 24-byte tuple LE64(seed), LE64(stage),
LE64(stream), each least-significant byte first. The API takes these decoded
numeric words directly; no native struct bytes, text hashing or file format is
used. Seed is caller-provided. Stage and stream are caller-selected numeric labels;
`0x6d617a65` in the consumer is simply its maze stage tag, stream 7 its instance.
The derivation is pure and independent of stream consumption, not cryptographic
security, a statistical independence proof or a collision-free tuple mapping.

Sampling bound b must be positive. Compute `threshold=(0-b)%b` with wrapping
u64 subtraction; draw until `r>=threshold`, then return `r%b`. Each residue has
the same number of accepted raw words, avoiding modulo bias. Charge every raw
word, even at bound 1 and on rejection. Raw PRNG quality is not a cryptographic
guarantee. Finite work exhaustion returns no sampled value. The directly passed
stream/work may have advanced on refusal; the maze owns these locally and exposes
none of its failed traversal state.

Replay is `{Domain, StreamKey, Budgets, algorithm=depth_first(1), version=1}`.
V1 fixes derivation, SplitMix64, sampling (including bound-1 consumption), start
cell 0, ascending axes/negative-then-positive candidate order, and DFS choices.
Retain exact extents/key/budgets, not only a hash. Changing sufficient budgets
changes descriptor admission metadata only, never successful logical masks.
Unknown algorithms and versions (including zero) reject before allocation.

Logical replay portability: 8-bit bytes, exact u32/u64 types, at least 64-bit
`size_t`, and the fixed-metadata size assertion in `maze.cpp`. No floating point,
pointer ordering, `std::hash`, implementation-dependent distributions or global
RNG is involved. Native layout, object sizes, allocator overhead, diagnostics and
raw memory bytes are not portable replay artifacts. Cross-platform execution is
not established by this Linux run; 32-bit containers are explicitly unsupported.

Pinned evidence in `tests.cpp::rng`/`replay_case`: six raw state-0 words, four
stage-derived words, four derived-state tuples, a rejection-sampling vector and
the hand-traced 2x2 logical fixture (edges 0–1, 1–3, 3–2; masks `[2,9,2,5]`).
Independent Python integer calculations are retained in local
`builds/procedural-generation/logs/reference-vectors.log`; the rejection key was
derived by algebraic inversion, not an unbounded seed search. Consuming 1000
unrelated stage draws cannot perturb a fresh maze call. No test requires distinct
small mazes from distinct seeds.

## PROCGEN-005 — Implemented resource and failure contract

Iterative depth-first traversal starts at index 0, enumerating axis ascending,
negative then positive, choosing uniformly among unvisited neighbors. Store
reciprocal passage masks and a cell-sized traversal stack. No recursion or hidden
registry. All size/index/byte/work arithmetic is checked before allocation.
Each result stores one u32 passage mask per cell. Scratch stores one u64 stack
entry per cell and at most 32 fixed candidates. Visited state is derivable from
a nonzero mask, with the initially visited root 0 treated specially. No visited
buffer or recursive call chain is hidden. Both vectors are constructed at exact
requested size and never grown. The stack contains distinct discovered cells,
so depth cannot exceed C. Every discovery opens exactly one edge to an unvisited
cell. On a rectangular connected domain this visits all C cells and opens C-1
edges without cycles. This is randomized depth-first generation, not uniform
spanning-tree sampling or a difficulty measure.

`requirements(domain)` checks before allocation:

- Cells C and edges C-1 (C is at least 1).
- Requested buffer payload `4*C + 8*C`, plus 4096 bytes fixed allowance for
  domain/descriptor/candidate/local/return metadata. A compile-time size check
  protects the allowance. The accounted peak is therefore `12*C+4096` bytes,
  including the complete traversal stack even if a small maze would use less.
- Container `max_size`, storage multiplication/addition and traversal-work
  multiplication/addition are checked without allocating any buffer.
- Hard caps: 1,000,000 cells, 999,999 edges, 16,777,216 accounted bytes and
  100,000,000 work units; each effective budget is min(caller limit, hard cap).

This is a requested element-storage accounting model, not exact RSS. It excludes
allocator bookkeeping/over-allotment, library/runtime/instrumentation overhead,
OS page granularity and existing caller-owned results/copies. It also excludes
the independent validator/consumer, whose own fixtures are capped at 4096 cells.
No rejected huge domain is allocated or deliberately used to create memory pressure.

Mandatory work is `2*C + (2*C-1)*(2*D+1) + (C-1)` units: two initialized arrays,
exactly 2*C-1 traversal iterations, one iteration action plus 2*D candidate probes
each, and at least one raw draw for each of C-1 discoveries. Additional rejected
draws cost one each. Setup/validation has fixed bounded O(D) cost, with D<=16;
these units are an operation model, not CPU instruction counts or a time/RSS
guarantee. Complete operation complexity is O(C*D + raw draws). Successful work
is bounded by the effective work cap. Checking the mandatory minimum is necessary
but cannot promise acceptance: rejection sampling has no automatic finite
worst-case draw count. Runtime charges precede every action/probe/draw; exhaustion
discards local buffers and returns no Maze. `Usage` records work and raw draws.

`Result<T>` is `std::expected<T,Failure>` with separate `invalid_input` and
`budget_refused` kinds and precise reasons. Schema/version validation and arithmetic
overflow are invalid input; finite cell/edge/storage/work limits refuse budget.
No catch-all converts exceptions: `bad_alloc`, length and programming exceptions
propagate. Tests deny allocation on small valid input to confirm `bad_alloc`
propagation; no actual OOM is attempted. Existing results are never an output
parameter, so only the caller can adopt a new complete successful value. Keeping
the previous Maze until success preserves it across invalid input, early refusal,
runtime work exhaustion and allocation exceptions.

## PROCGEN-006 — Acceptance and later implications

Implemented: an independent breadth-first/edge-based validator checks connectivity,
reciprocity, boundaries, edge count/acyclicity and coordinate round trips.
Test several unequal 4D seeds with positive fourth-axis edge counts, unit/one-cell
domains, ownership, replay, stream isolation, vectors, invalid inputs, overflow,
cell/edge/storage/work boundaries and preserved prior results on refusal.
`checks.cpp::verify` independently decodes mixed-radix coordinates, encodes with
Horner order, constructs adjacency from masks, unions positive-axis edges to
detect cycles, and runs breadth-first reachability. It checks every reciprocal
edge, every boundary, all inactive bits, domain/passage queries and coordinate
round trips. Test-only code, not production acceptance/scratch storage. Six 4D
seeds on `{2,3,2,4}` each reach 48 cells with 47 edges and at least three true
fourth-axis passages. Coverage also includes 1D, 3D, 6D and axis-15 passages,
one cell and unit extents. The largest generated fixture is 48 cells.

Allocation-denial tests prepare inputs first and prove invalid/overflow/early
budget paths do not allocate. Separate boundary cases exercise exact C=6/E=5/
4168 bytes/72 work admission and each just-too-small budget. The forced-rejection
key `{0xd78884aaeab763e4,0,0}` gives first raw word 0 at a 3-choice root; minimum
work passes preflight but traversal exhausts its budget and publishes no value.
With extra work it completes and validates. Previous results remain unchanged.

### Executed checks (2026-10-23)

Source tested: this U1 checkpoint based on
`3542f883391d4225329dab8d5d61a5987e69c8f0`; exact committed checkpoint is the
commit containing this record. Linux, GNU C++ 16.2.1, C++23, Debug, Ninja,
`PROCGEN_UBSAN=ON`, undefined-behavior sanitizer with no recovery and warnings as
errors. Privately populated googletest was absent; no dependencies were fetched.

All configure/build/native checks used `/mnt/projects/cheryl-features/scripts/build --`,
one job, preserving this clone's incremental build. No exit-75 refusal or cancelled
queue occurred. Complete stdout/stderr logs are clone-local and intentionally
ignored under `builds/procedural-generation/logs/`:

| Operation | Evidence/result |
| --- | --- |
| Configure `cmake -S projects/tests/procedural-generation -B builds/procedural-generation -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPROCGEN_UBSAN=ON` | `configure.log`, exit 0 |
| Initial build | `build-1.log`, compile failure in test harness (ADL selected std::generate; invalid-enum diagnostic); no tests ran in this failed build |
| Corrected incremental build | `build-2.log`, exit 0; qualified ambiguous test calls and valid-width invalid enum; production and test warning policies unchanged |
| Focused CTest | `ctest-1.log`, 11 passed / 0 failed / 0 skipped |
| Separate `builds/procedural-generation/procgen-consumer` | `consumer-1.log`, 2 demonstrations passed / 0 failed / 0 skipped |
| Repository-format application | `format.log`, exit 0; only owned C++ files |
| Final formatted-source build and CTest | `build-3.log`, `ctest-2.log`, both exit 0; 11 passed / 0 failed / 0 skipped; 10 unit cases, 1470 assertions, plus consumer |

CTest command (wrapper prefixed):
`ctest --test-dir builds/procedural-generation --no-tests=error --output-on-failure --verbose -R '^procgen[.]' --parallel 1`.
Final CTest is nonempty, has no UBSan diagnostics and includes the consumer. Its
2D output is `{6,4}`: 24 cells, 23 edges, 24 reached, axis edges `[12,11]`,
306 work/23 draws. Full 4D output is `{3,2,2,3}`: 36 cells, 35 edges, 36 reached,
axis edges `[11,8,6,10]`, 746 work/35 draws. No projection or slice is substituted.

Unverified: other compilers/operating systems, ASan, statistical RNG quality,
large-domain performance and public Engine/SDK consumption. These are limits of
observed evidence, not claims established by this bounded suite. No Engine/GUI or
protected-project build ran. Source scope and staged diff are reviewed before
the local checkpoint commit; coordinator review remains the next boundary.

Later graph mazes need explicit endpoints/ports, variable branching, dead ends,
loops and multigraph policies; those are not forced into unit-axis adjacency.
Caves/dungeons may need masks/regions/room graphs; geography fields/drainage;
settlements sites/roads/parcels; tectonic edits changed regions and explicit
invalidation/remap/revalidation of dependent layers. These remain separate
related semantic outputs. U1 does not design or implement them or a common World.

## U2 authority and initial proposal (PROCGEN-007–010)

U1 is accepted at `323914810d6a9e4cd6f870baaaa5916c50747c8a`; U2 is explicitly
activated. Initial status and diffs are clean. Read updated workspace rules,
autonomy, brief/workstream, assignment, W018/W019 and clone instructions/records.
Reuse the existing Debug/GNU 16.2.1/UBSan configuration and preserve U1 replay.

Proposed success-path caller recorded before dependent APIs; executable code
will check errors/no-path before dereferencing:

```cpp
auto maze = ProcGen::generate(extents, key, generation_budget).value();
auto graph = ProcGen::maze_view(maze);
auto route = ProcGen::find_route(graph, entrance, goal,
    ProcGen::Heuristic::manhattan, search_limits).value();
auto metrics = ProcGen::measure(graph, entrance, goal,
    route.path->cells, analysis_budget).value();
```

All new spelling/types are engineering choices, not inferred personal taste.
PROCGEN-002 retains nominated HyperMaze/v3 provenance and counterevidence. W018
supplies the requested traversal endpoint; W019 makes search a supporting utility.
Re-read official Boost 1.84
[A* pseudocode](https://www.boost.org/doc/libs/1_84_0/libs/graph/doc/astar_search.html)
and [heuristic concept](https://www.boost.org/doc/libs/1_84_0/libs/graph/doc/AStarHeuristic.html)
as algorithm references, without choosing a Boost dependency or copying an API.
U2 chooses only known consistent zero/Manhattan heuristics: one axis step changes
Manhattan distance by one and each supported cost is at least one. No geometric
portal heuristic or arbitrary-callback optimality is promised.

Recorded sequence: implement the borrowed view/positive costs and bounded A*, then
metrics and literal fixtures, then independent BFS/O(V^2) weighted reference,
failure/ownership tests and text caller. Charge validation, initialization, heap
height bounds, expansions, probes and reconstruction. Check buffers and arithmetic
before allocation. Found/no-path are complete search outcomes; expected validation,
overflow and budget errors publish no partial path. Allocation/programming
exceptions propagate. This sequence is implemented; executed evidence follows.

U3 remains proposed: strict labyrinth construction and useful measured traversal
mapping, with coverage/endpoints/ambiguity distinct and requested/achieved outcomes.
Proven incompatibility is separate from bounded construction failure. No generator,
new controls, general graph mazes, caves, settlements, broad AI or physics in U2.

### PROCGEN-007 — Implemented input/cost and ownership boundary

Owners under `projects/tests/procedural-generation`: `traversal.h::MazeView`,
`traversal-internal.h::header/validate/arc`, `traversal.cpp::maze_view`.
`MazeView` borrows a const Domain and spans of const u32 masks/u64 costs. All C
rectangular cells are active, including isolates. No portals, masked active
domain, callbacks, graph registry or generation state. Source data must stay
alive and unchanged during each call; moves/assignment may invalidate its views.

Empty costs mean unit cost. Otherwise exactly C*D entries at `cell*D+axis`
store positive-axis edge costs. Open slots require 1..UINT64_MAX; closed positive
slots require zero. Negative incidences use the same slot at the lower-index
endpoint. Each undirected edge therefore has one cost. Full validation checks
span lengths, inactive bits, reciprocal in-bounds steps, positive/closed-slot
costs and finite edge/work caps before scratch buffers are allocated. Medium
confidence for future game-AI reuse; no general graph adapter is extracted.

Paths own their vectors and deep-copy on copy. Later searches or destruction of
input cannot invalidate them. No call takes mutable prior results or aliases
scratch. Measurements own independent scalar/array/optional values. Adopting a
new successful result remains caller policy; caller mutations need its own
synchronization. U1's generation/replay implementation is unchanged. Failure
reasons/messages are appended without changing previous enum ordering.

### PROCGEN-008 — Implemented A*, determinism, failures and resources

Owners: `traversal.cpp::find_route/estimate/route_storage` and checked helpers in
`traversal-internal.h`. Only zero and unit-Manhattan heuristics are accepted.
An axis step changes Manhattan distance by one and costs at least one, so both
are consistent, with zero at the goal. Closed costs are final. There is no
arbitrary-callback or admissible-but-inconsistent reopening contract. Portal
links cannot enter this boundary and inherit a geometric heuristic.

Search v1 orders the min-heap by `(f,g,cell)` ascending and enumerates axes
ascending/negative before positive. Equal costs keep the first parent. Strict
open-cost improvements enqueue new entries; stale entries are detected on pop
by settled state/current g and consume queue/storage/work limits. No pointer,
hash or unspecified insertion tie is used. Goal acceptance happens on a current
settled pop, after charging settlement, followed by fully charged reconstruction
and reversal before publication. Start=goal gives one index, cost0, one settlement.

Expected outcomes: `SearchResult.path` present means found; absent means the
queue was completely drained. Failure kind/reason distinguishes invalid endpoints
(`invalid_input/endpoint`), topology/cost/heuristic/path errors, numeric overflow
(`invalid_input/overflow`), and finite-budget refusal (cells/edges/storage/work/
expansions/queue_entries/queue_pushes). No failure publishes a partial SearchResult.
Checked sums include g+edge, g+h, Manhattan sums, buffer/work formulas and metric
route costs. UINT64_MAX is a valid path cost, not an unseen sentinel. Any performed
sum overflow refuses even if another representable route might exist; it never
becomes no-path. Only private `TraversalDetail::Stop` control exceptions become
typed errors. Allocation/standard-library/logic exceptions propagate. Invalid and
early-budget paths allocate no owned buffers; the C++ exception runtime may use
its own bounded control storage, excluded from payload accounting. Allocation
denial tests cover operator-new buffers, not all runtime allocation internals.

Effective limits are min(caller, hard): 1M cells, 16M undirected edges, 64 MiB
requested storage, 100M work, 1M settled expansions, 1M live queue entries and
16M total pushes. The input edge cap is distinct from repeated inspected open
incidences, which are counted and work-charged through directional probes. Zero
expansions, live capacity or pushes refuse before buffer allocation.

Search peak model: `4096+C*(sizeof(Record)+sizeof(Index))+Q*sizeof(Entry)`.
It includes all records, full C-index possible output, and ALL Q reserved queue
entries, including duplicates. On the executed ABI Record/Entry are 24 bytes:
`4096+32*C+24*Q`. No vector grows beyond its admitted capacity; no extra C-sized
path buffer appears during reconstruction. Fixed metadata has a compile-time
allowance check. Container max_size and all byte/work arithmetic are checked
before allocation. Requirement helpers plan without allocating buffers or applying
hard caps. Requested-element accounting excludes allocator over-allotment,
runtime/sanitizer overhead, borrowed inputs and prior caller-owned outputs; not RSS.

Work charges: validation C*(1+2*D); C initialized records; heuristic one unit for
zero or D for Manhattan; push from n entries `1+bit_width(n+1)`; pop from n
`1+2*bit_width(n)`; one per settlement, directional probe, reconstructed index
and reversal swap (`path.size()/2`). Heap-height charges conservatively include
standard heap comparison bounds, including stale pops. The early mandatory
lower bound is `C*(1+2*D)+C+7` (validation, records, minimum start h/push/pop/
settlement); actual completion always enforces runtime work. Complexity is bounded
by validation O(C*D), queue O(pushes*log(Q+1)), expansion O(expansions*D) and
reconstruction O(C), under the effective work cap. D<=16 bounds per-unit helpers.
No elapsed-time/RSS guarantee is inferred.

`search_version=1` labels debug/replay evidence, not a file format. Same logical
view/costs/endpoints/heuristic/version and sufficient limits reproduce the path.
Limits govern admission/completion, not successful path content. U1 Maze retains
its original replay descriptor. Integer/domain portability assumptions are U1's;
object sizes/admission may differ by ABI. Only Linux/GNU execution is verified.

### PROCGEN-009 — Implemented measurements and conventions

Owners: `metrics.cpp::measure/metric_storage`, result declarations in `traversal.h`.
The full active graph is flood-traversed, starting with entrance. Report C, E,
entrance reachability including itself, components K, cycle rank E-(C-K), and
degree histogram 0..32. Dead ends are degree-one cells INCLUDING selected
endpoints; a singleton/isolated degree-zero cell is not a dead end.

A nonempty selected route must be simple, in bounds, passage-adjacent and exactly
entrance-to-goal. Recompute its cost; route cell count and edge count (cells-1)
stay distinct. Supplied paths need not be shortest. Empty span means no route
selected, not proof of unreachability, leaving route/tree-only fields absent.

Only connected trees with an explicit selected route get off-route cells/edges,
attachment-branch count (including attachments at endpoints), off-route degree-one
dead ends and maximum false depth. Multi-source flood measures UNWEIGHTED edge
distance from path attachment, ignoring weights/geographic distance. Tree
off-route edge count equals off-route cells. Empty off-route mass yields zeros.
Cycle/disconnected inputs return absent tree-only metrics; a chosen shortest
route does not classify every other cyclic route as a false lead.

Strict recognition is structural and independent of path selection: connected,
cycle-free, exactly the selected two distinct degree-one endpoints, all others
degree two, covering the declared active domain. Singleton C1/E0 accepts start=
goal=0, also when no route is selected. A trivial start=goal path in a larger
graph is valid but never strict. Excluding the arrival edge leaves one onward
choice at internal path nodes; voluntary reversal/player orientation/difficulty
are not guaranteed or measured. No scalar difficulty score is invented.

Metric peak: `4096+C*(sizeof(Cell)+sizeof(Index))`; Cell holds depth/flags and a
full C-entry queue is reused across components/tree analysis. On this ABI this
is `4096+24*C`. Minimum complete no-selected-path work is C*(7+4*D): validation,
two initialized buffers, degree/component scans and enqueue/pop/probes for every
cell. Path validation/tree analysis add runtime work. All enqueue/pop/probe/path
index/seed operations are charged; no recursive traversal. Common caps match
search's cell/edge/storage/work caps. Failure publishes no partial measurements.

### PROCGEN-010 — Executed U2 evidence and next boundary

Test owner: `traversal-tests.cpp`, included in the EXISTING `procgen-tests`
executable. All-case selection includes unchanged U1 case functions and U2.
No dependency, target family, root configuration or export is added. Tested
source is this U2 commit based on accepted U1 `323914810d6a9e4cd6f870baaaa5916c50747c8a`.

References use literal edge lists, or pairwise coordinate/mask extraction for
generated graphs, independently of generation/search neighbor traversal. Unit
distances use BFS; weighted reference uses bounded O(V^2) scan-based Dijkstra,
without heap/heuristic/stale-entry logic. Validate each returned route's endpoints,
unique cells, literal adjacency and summed cost separately. Three seeds each on
2D `{5,3}` and 4D `{3,2,2,3}` compare both heuristic modes to BFS. Every 4D route
has a real fourth-axis step. Equivalent optimal routes need not match exact BFS
paths; square tie `[0,1,3]` is separately pinned.

Literal expected metrics:

| Fixture | Evidence |
| --- | --- |
| Four-cell line endpoints0/3 | C4/E3/K1/rank0, degree1:2/degree2:2, route3 edges/cost3, no false branches, strict true |
| Same line endpoints1/3 | Off-route cells1/branch1/depth1; strict false because selected entrance is internal |
| Singleton | C1/E0/K1, degree0:1/dead ends0, one-cell route/cost0, strict true |
| Winding literal 4D path | C16/E15, degree1:2/degree2:14, all reached, route15 edges/cost15, strict true, fourth-axis edge |
| Branch tree | C6/E5/K1/rank0, degree1:3/degree2:2/degree3:1, route2 edges, off-route cells3/edges3/branches2/dead ends2/depth2; strict false |
| Deep false-lead tree | C8/E7, degree1:4/degree2:2/degree3:2, off-route cells4/branches1/dead ends2/depth3; strict false |
| Looped ladder | C6/E7/K1/rank2, degree2:4/degree3:2; zero dead ends yet two onward options at degree3; tree metrics absent, strict false |
| Disconnected | C6/E3/K3/rank0, reachable3, degree0:1/degree1:4/degree2:1, absent route/tree metrics; strict false even with a route in one component |

Weighted square first discovers goal at cost10, improves it to3, and uses five
pushes for four cells (duplicates are charged). Another weighted cyclic fixture
pops one stale entry: six pushes/pops, five settlements. Tests cover exact
UINT64_MAX cost, g/f/route cost overflow, huge planning arithmetic, exact and
just-too-small cell/edge/storage/work/expansion/queue/push budgets. Exhausted
search is never no-path. Earlier inputs/results survive invalid/budget failure
and propagated bad_alloc. Paths survive source destruction. References cap at
64 cells; no huge rejected buffer or memory-pressure/OOM experiment.

Executed in preserved `builds/procedural-generation`: Linux/GNU16.2.1, C++23,
Debug/Ninja/UBSan ON, warnings as errors. ALL configure/build/native checks used
`/mnt/projects/cheryl-features/scripts/build --`, one job. No resource refusal,
queue cancellation, dependency retrieval or model substitution occurred. Complete
stdout/stderr logs are local/ignored under `builds/procedural-generation/logs/u2/`:

| Command after wrapper prefix | Log | Result |
| --- | --- | --- |
| `clang-format -i` on owned changed C++ files | `format-1.log` | exit0 |
| `cmake --build builds/procedural-generation --parallel 1` | `build-1.log` | existing standalone configuration regenerated; compile/link exit0 |
| `ctest --test-dir builds/procedural-generation --no-tests=error --output-on-failure --verbose -R '^procgen[.]' --parallel 1` | `ctest-1.log` | 23 passed / 0 failed / 0 skipped; 22 cases, 2680 assertions (1470 U1 +1210 U2), plus consumer; zero UBSan diagnostics |
| `builds/procedural-generation/procgen-consumer` | `consumer-1.log` | 2 demonstrations passed / 0 failed / 0 skipped |

Consumer seed42/stage0x6d617a65/stream7 retains U1 topology/replay. 2D `{6,4}`
stays C24/E23; chosen route9 cells/8 edges/cost8, components1/dead ends4,
off-route cells15/branches2/depth8, strict false. Full 4D `{3,2,2,3}` stays
C36/E35/axis-four edges10; route9 cells/8 edges/cost8, reachable36/rank0/dead ends3,
off-route cells27/branches1/depth26, strict false. Search work271/527 respectively.

Confidence is high for this finite orthogonal/integer boundary and measured
invariants, medium for exact private API suitability for future game-AI reuse.
Unverified: other compilers/OSes, ASan, large-domain performance, public Engine/SDK
integration and player difficulty/playtesting. A* proves no generation feasibility
or navigation guarantee. U3 remains proposed: checked strict labyrinth construction,
useful traversability mapping and selected controls with requested/achieved
constraints. Distinguish proven incompatibility from bounded construction failure.
No automatic U3 or general graph/sibling integration.

## U3 authority and proposed caller (PROCGEN-011–014)

U2 is accepted at `e4a735bf793287eb3126edff0e3feab4828b7e81`; initial
status/diffs are clean. U3 is authorized, not a proposal awaiting approval. Read
updated workspace AGENTS, autonomy/memory policy, brief/workstream, W018/W019,
worker assignment and clone instructions/records. Adaptive resource admission
belongs to the shared wrapper/monitor; no worker-side cap or slot changes.

Proposed success-path caller recorded before dependent APIs (errors checked in
the executable):

```cpp
for (double value : {0.0, 0.5, 1.0}) {
    auto maze2 = ProcGen::generate_configured(plane, key,
        {.traversability = value}, budgets); // plane {6,4}
    auto maze4 = ProcGen::generate_configured(volume, key,
        {.traversability = value}, budgets); // volume {3,2,2,3}
    // Each success owns topology, selected endpoints, main route, replay and
    // U2 measurements; independent checker/search compare the full topology.
}
```

Engineering plan: normalize finite binary64 [0,1] requests to 65536 integer
ticks (nearest, half upward); reserve `L=2+floor(ticks*(C-2)/65536)` route cells
for C>=2, otherwise L=1. Seeded axis order/reflection gives a rectangular
serpentine spanning order; reserve its prefix, then attach each remaining cell
to an already visited neighbor. No other growth control is needed in U3.
Measure the completed tree with U2 and charge that analysis in generation's
peak/work model. Exact off-route cells C-L decrease as ticks increase, while
auto goal and route length change. This is not perceptual difficulty or a
branch-count/depth monotonicity promise.

Fixed endpoints will accept the selected prefix pair in either orientation.
Other feasible pairs are unsupported by this construction; proven
distinctness/checkerboard contradictions have a separate failure.
No bounded search is presented as a feasibility proof. Full rectangular
coverage only; masks, arbitrary fixed endpoints and separate route/branch/run/loop
controls remain future work. A separate descriptor preserves DFS v1.

Assumptions: this normalized coverage profile is useful as a first measurable
prototype (engineering choice, medium confidence); rectangular serpentine
construction is sufficient for auto-endpoint strict coverage (high confidence,
requires independent tests). PROCGEN-002 retains exact reference provenance and
counterevidence; neither legacy C nor v3 history establishes this mapping/API.
Implementation and execution are pending at this entry.

### PROCGEN-011 — Implemented profile and representation

Owners: `projects/tests/procedural-generation/configuration.h` and
`configuration.cpp`, `Configuration`, `normalize`, `preflight`,
`generate_configured` and `replay_configured`. Existing DFS source behavior and
logical replay stay unchanged; appended failure enumerators do not renumber old
reasons. Private spelling/mapping is an engineering choice, not new user taste
inferred from HyperMaze/v3. Source provenance remains PROCGEN-002 at its exact
revisions/paths/symbols; W018 supplies the strict endpoint and rectangular
construction suggestion, W019 supporting verification. No protected reference
was mutated, built or used as an output/cache owner during U3.

Requested traversability must be finite binary64 in [0,1]. Scale by 2^16 exactly,
truncate the nonnegative scaled value to u32, then add one iff its fractional
part is >=0.5. Unlike adding .5 before truncation, this preserves the just-below-
half distinction. IEEE binary64 (IEC559, 53-bit significand, exponent limit1024)
is checked statically. Negative zero is accepted as zero; positive subnormals
normalize to zero. Remaining construction is integer arithmetic. All successful
logical replay inherits U1's byte/integer/container contract plus this binary64
normalization requirement. No cross-compiler/OS execution is established.

Ticks q are in [0,65536]. For C>=2, select exactly
`L=2+floor(q*(C-2)/65536)` route cells. C=1 selects L=1. Distinct endpoints are
mandatory when C>1. Minimum two-cell route keeps zero solvable. Output coverage
is always C/C, selected-route coverage L/C and off-route cells C-L. These integer
counts are the exact achieved profile, not a probability or difficulty score.
The normalized input controls the fraction of cells beyond the minimum two
assigned to the route; it is not the raw ratio L/C. Quantized effective ticks,
route cells, requested options and full U2 measurements are returned. No targets
are silently relaxed. C<=2 collapses all requests to a whole-domain path; other
tiny domains have plateaus. Near-one values rounding to 65536 also construct the
strict endpoint. As q increases for the same domain, L never decreases and C-L
never increases; the auto goal moves along the same prefix order. Branch count,
false depth, number/placement of choices, and subjective difficulty have no
monotonicity guarantee. Branch attachments at entrance or goal count per U2;
the profile does not promise where a player encounters a choice.

### PROCGEN-012 — Construction proof, ordering and endpoint failures

New algorithm `ConfiguredAlgorithm::serpentine_tree(1)`, version1. Fresh U1 stage
stream uses exact StreamKey, derivation and SplitMix64/sampler. Initialize axis
ranks 0..D-1; descending Fisher–Yates counts D..2 draw uniformly below count and
swap rank count-1 with the selected rank. Then visit ranks ascending, computing
permuted strides and one sample_below(2) reflection each. These geometry draws
are independent of the traversal profile; no global registry or consumption by
another stage can perturb output. Draws at bound1 during attachment still count.

For ordinal i, rank r has permuted axis a, lower stride s and extent e. Start
with digit `(i/s)%e`; replace it by `e-1-digit` when `(i/(s*e))%2` is odd; then
apply the seeded rank reflection. Convert to canonical axis-0-fastest index by
the validated domain strides. Inverse mapping recovers higher ranks first, so
this is a bijection over all C cells. On incrementing i, a mixed-radix carry
changes one digit by one; lower wrapped digits also flip reflection parity and
therefore retain their coordinates. Higher coordinates stay fixed. Thus adjacent
ordinals differ on exactly one axis by one, including unequal/unit extents.
This is iterative O(C*D), not recursive Hamiltonian search.

Reserve the first L cells as an immutable main chain. Scan remaining ordinals
ascending; enumerate candidate neighbors in canonical axis ascending, negative
then positive order, keeping those already visited (nonzero mask). For C>1 the
core has L>=2 and every visited cell has a passage; the preceding ordinal is an
available visited neighbor. Choose uniformly among candidates and open exactly
one reciprocal edge. Each newly added vertex attaches once, so connectedness
and acyclicity are preserved, and the unique selected-endpoint route remains
exactly the reserved chain. At L=C no attachment occurs: selected endpoints have
degree1, all others degree2, E=C-1. Singleton has E=0, endpoint0/0 and degree0.
Axis order/reflection randomization is not uniform spanning-tree/path sampling.
Strict internal traversal has one onward edge after excluding arrival; reversal,
orientation, player difficulty and playtesting are not addressed.

`Configuration::endpoints=nullopt` means auto, the prefix's first/last cells.
A fixed pair must be valid indexes. C>1 equal fixed endpoints contradict a simple
route of L>=2 cells. For L=C, checkerboard parity is a necessary obstruction:
even C needs opposite colors; odd rectangular C means all extents odd and color0
has one extra cell, requiring both endpoints color0. These failures are
`FailureKind::contradiction/Reason::endpoint_policy`, proved by alternating colors
along a spanning path, not inferred from construction failure. Passing the check
is not a proof of feasibility. This implementation accepts the seeded prefix pair
in either orientation, reversing the owned main route for the reverse pair.
Another pair returns `unsupported/endpoint_policy`, BEFORE allocating owned
buffers, without moving endpoints or claiming mathematical impossibility.
`configured_requirements` checks arithmetic/resources and necessary endpoint
conditions only: without a stream key it cannot certify that pair membership.
Full rectangular coverage is explicit; unknown Coverage values return
`unsupported/coverage`. No masks/subset/loops placeholders are implemented.
Bad scalar/endpoint/domain/version values are invalid input; overflow is invalid
input, finite budget limits/refusal or runtime exhaustion are budget_refused.
No construction-search exhaustion is confused with impossibility.

### PROCGEN-013 — Ownership, replay and peak/work model

`ConfiguredMaze` owns const-viewable masks and the selected route, a separate
versioned descriptor, effective configuration, endpoints, U2 Measurements and
Usage. Copies deep-copy both buffers; views expire on owner destruction/move/
assignment; moved-from values only destroy/assign. No scratch/global aliases.
The borrowed U2 MazeView adapts either old or new owner, without changing U2 cost/
heuristic behavior. The independent checker now has a view overload but retains
old Maze neighbor-query checks and all original topology/coordinate/union/BFS
checks. Synthetic cyclic/disconnected fixtures remain in U2's suite.

Replay retains Domain/extents, seed/stage/stream, exact requested binary64 value,
endpoint policy/pair, full coverage, budgets, algorithm and version. Effective q/L
are deterministically recomputed and returned; unsupported algorithm/version
reject before buffer allocation. Sufficient budgets change admission metadata
only, never topology/endpoints/main route. Reusing U1 numeric stage derivation
is intentionally a small consumer helper, neither cryptographic independence nor
collision-free streams. There is no file format or Engine/SDK export.

Preflight validates all checked products/sums and vector capacities before owned
buffer allocation. Hard caps: 1,000,000 cells, C-1<=999,999 edges, 64 MiB accounted
peak storage, 100,000,000 work. Effective limits are min(caller, hard). Let M be
`metric_requirements(domain).storage_bytes`. Accounted peak is
`8192 + 4*C + 8*L + max(8*C, M)` bytes. Output is masks 4*C and route8*L;
construction scratch is a C-index order (8*C). Release that scratch before
calling U2 measure, whose full initialized cell/queue buffers and 4096 fixed
allowance are in M. On this ABI M=4096+24*C, so peak=12288+28*C+8*L.
8192 covers bounded outer/candidate/metadata/return helpers, protected by a
static size assertion. This counts requested element payload plus allowances,
not exact RSS: allocator/runtime/sanitizer/exception/OS overhead and existing
caller-owned results/copies are excluded. No huge rejected allocation or memory
pressure experiment occurs. Allocation/programming exceptions propagate; only
the existing private typed Stop is caught for expected failures.

Work charges all initialized array elements (2*C+L), each order-map action/axis
(C*(D+1)), route copies L, core links and directional probes, each tail iteration,
all 2*D tail probes, each attachment action, stage setup/map actions and EVERY raw
sampling word including rejection. U2 measures the completed topology and chosen
route using the remaining work allowance; its actual work is charged into outer
Usage. No successful topology is published before measurement completes.

The checked lower bound is the sum of:

- `2*C+L`, `C*(D+1)`, `L`, `2*(L-1)`;
- `(C-L)*(2*D+3)` for tail loop/probes/link/minimum draw;
- `6*D+1` for axis setup/minimum geometry draws and two endpoint maps;
- U2 base `C*(7+4*D)`, chosen-route minimum `2*L-1`, and tree flood `C*(2+2*D)`.

Bounded O(D) input validation/metadata arithmetic precedes traversal work, as in
U1/U2. For this connected-tree construction the only extra charged work beyond
that lower bound is additional core and measured-route direction probes (at most
`2*(2*D-1)*(L-1)`), plus rejected raw draws. Minimum raw draws are
`2*D-1+(C-L)`. Unbiased rejection has no finite automatic worst-case draw bound;
runtime Work limits every draw, scan and metric operation. Admission at the
minimum cannot promise completion. Work exhaustion discards local buffers and
returns no partial topology/path/measurements. Time is O(C*D+raw draws), successful
charged work never exceeds the effective cap. Existing outputs remain unchanged
on invalid, unsupported, contradiction, early/runtime budget and allocation failures.

### PROCGEN-014 — Executed U3 evidence and handoff

Source: this coherent U3 checkpoint based on accepted
`e4a735bf793287eb3126edff0e3feab4828b7e81`. GNU16.2.1/C++23/Debug/Ninja,
`PROCGEN_UBSAN=ON`, no recovery, warnings as errors, same preserved
`builds/procedural-generation` cache. No new dependencies, pin/configuration
changes, Engine/GUI build or public extraction. All configure/build/native
commands use `/mnt/projects/cheryl-features/scripts/build --`, one job.
Queued commands retained their existing tool sessions until completion; no
resource refusal/exit75, cancellation, unchanged admission retry or model
substitution occurred.

New tests in `configuration-tests.cpp` contribute ten cases/5955 assertions;
all 22 old cases/2680 assertions remain, including U1's 1470 golden/topology
assertions. Independent `checks.cpp` checks masks/coordinates/reciprocity/
boundaries/edge count/union acyclicity and BFS connectivity. New small Oracle
extracts edges by pairwise independently decoded coordinates, then FIFO BFS;
it does not reuse construction order, Domain neighbor traversal or A* internals.
Verify every route index/edge/endpoints/uniqueness/cost, both U2 heuristics versus
BFS, literal and reported metrics, strict degree/coverage invariant, and exact
monotonic L/C-L definition (not branch/depth/difficulty). Three seeds on {5,3}
and four seeds on unequal true4D {3,2,2,3} exercise the profiles. At .5 and1 every
4D fixture route has a real fourth-axis step. Each full topology reaches C cells
with E=C-1. Largest constructed test is C36; references cap at64, checker at4096.

Other coverage: 1D/unit/singleton domains and axis15; half-tick and adjacent
binary64 rounding, near-one and subnormal/negative-zero quantization; invalid
finite/out-of-range/NaN/infinity/options/dimensions/extents/endpoints/versions;
compatible fixed pairs/reversal, proven even/odd color conflicts and unsupported
pairs; replay and unrelated stage isolation; deep copies/moves, previous values
preserved on invalid/unsupported/contradiction/budget/allocation errors; checked
product/byte overflow and exact/just-too-small C/E/storage/work. Allocation denial
confirms preflight and unsupported-pair refusals avoid owned buffers, and actual
bad_alloc propagates without memory pressure. Algebraically derived U1 key with
first raw word0 forces rejection of the initial 3-axis shuffle draw; finite work
refuses without publishing topology. Exact observed work and one-unit-short
runtime refusal are checked, not merely minimum-work admission.

Pinned hand-traced new 2x2 replay, seed42/stage0x6d617a65/stream7:
axis order [1,0], no reflection, ordinal cells [0,2,3,1]. At q32768, selected
route [0,2,3] and remaining1 attaches to3, masks [8,8,6,5], off-route cells1/
branches1/false dead ends1/depth1, strict false. Work147/raw draws4. At q65536
selected route [0,2,3,1], same masks, no off-route cells/branches/depth, degrees
one:2/two:2 and strict true. Work150/raw draws3. Same graph with different
selected endpoints illustrates why a unique route alone does not establish the
strict endpoint. This does not require seed uniqueness.

Complete stdout/stderr and immediate exit manifest are clone-local/ignored in
`builds/procedural-generation/logs/u3/` (not truncated terminal excerpts):

| Wrapped command | Logs and actual result |
| --- | --- |
| `clang-format -i` on owned changed C++ | `format-1.log`, `format-2.log`, exit0 |
| `cmake --build builds/procedural-generation --parallel 1` | `build-1.log` (standalone CMake regeneration), `build-2.log`, `build-3.log`, all exit0 |
| `ctest --test-dir builds/procedural-generation --no-tests=error --output-on-failure --verbose -R '^procgen[.]' --parallel 1` | `ctest-1.log`: exit0, 33 passed/0 failed/0 skipped, 32 cases/8620 assertions before final test additions |
| Same CTest selection after added literals | `ctest-2.log`: exit8, 32 passed/1 failed/0 skipped; config_profile fails at a mistaken hand-summed work expectation247 instead of147; the unreached second literal250 is also corrected to150. No production accounting change; complete failing diagnostic retained |
| Final same CTest selection | `ctest-3.log`: exit0, 33 passed/0 failed/0 skipped; 32 cases/8635 assertions (1470 U1 +1210 U2 +5955 U3), plus consumer; zero UBSan diagnostics |
| Separate `builds/procedural-generation/procgen-consumer` | `consumer-1.log`: exit0, 8 demonstrations passed/0 failed/0 skipped; final CTest consumer also passes8/8 |
| Immediate command statuses | `exits.tsv`, including resolved exit8; no failed diagnostics discarded |

Text consumer retains both original DFS demonstrations, then 0/.5/1 on the SAME
2D {6,4} and 4D {3,2,2,3} domains, exact key above. All full topology checks and
U2 route checks pass. Measured achievements:

| Domain/value | Route cells/edges(cost) | Off-route/branches/max false depth | Endpoints | Full topology edges by axis |
| --- | --- | --- | --- | --- |
| 2D/0 | 2/1 | 22/2/11 | 0→6 | 12,11 |
| 2D/.5 | 13/12 | 11/2/4 | 0→21 | 9,14 |
| 2D/1 | 24/23 | 0/0/0 | 0→5 | 5,18 |
| 4D/0 | 2/1 | 34/3/10 | 27→24 | 9,5,10,11 |
| 4D/.5 | 19/18 | 17/6/8 | 27→1 | 9,12,4,10 |
| 4D/1 | 36/35 | 0/0/0 | 27→35 | 2,18,3,12 |

All 2D outputs C24/E23/reached24; all 4D C36/E35/reached36. Values1 are strict,
with every cell on the selected route and 12 actual fourth-axis edges in the
4D route. These are full N-D topology reports, not projections/slices/viewers.
DFS masks/seed/work and original route metrics remain accepted U1/U2 fixtures.
No claims about player experience, other compilers/OSes, ASan, large-domain
performance or public Engine/SDK behavior are verified.

Confidence: high for the bounded rectangular construction and finite measured
contracts; medium that this coverage-coupled profile is the best long-term user
control. No unresolved source/resource blocker. Next useful proposed unit is a
wider bounded fixed-endpoint construction with explicit coverage/length constraints,
not implementing every advanced control. Route length, endpoints/waypoints,
coverage/masks, loops and branch/depth/style controls remain separate future work.
General HyperMaze endpoint/port/loopback graphs remain separate from axis steps.
Caves/dungeons may need regions/room graphs, geography fields/drainage,
settlements sites/roads/parcels, tectonic changes affected regions and explicit
remap/revalidation; no implementation or universal World architecture is implied.
Stop at this local checkpoint for coordinator review; publisher owns transport.

Staged review covers the 14 owned task files and matching records; no unrelated
work, root dependency/export or protected/sibling content is staged. Full staged
diff is saved locally as `logs/u3/staged-review.diff` under the existing build.
Whitespace checks pass. The final C++ sources are those built/tested above; only
internal handoff records change after final CTest.

## U4 authority and proposed caller (PROCGEN-015–018)

U3 is accepted at `533fabba686653500dd6a247e46855c88bd79a0c`. Initial branch
`feat/procedural-generation` and index/worktree are clean; recent U1–U3 commits
and current records were inspected. U4 broadens the utility collection to first
2D caves; further maze controls remain proposed. Read workspace/clone AGENTS,
autonomy/memory policy, brief/backlog/workstream, assignment and generation-storage
coordination. No sibling header/history/import, protected workspace mutation,
serialized recipe/save policy or integration is selected.

Proposed caller before dependent API implementation (executable checks errors):

```cpp
const std::array<ProcGen::Index, 2> extents{32, 24}; // width, height
const ProcGen::StreamKey key{42, 0x63617665, 7};
const ProcGen::CaveBudgets budgets{4096, 128 * 1024, 2'000'000};
const ProcGen::CaveConfiguration a{.solid_fill = 4200, .steps = 3,
    .birth = 0x1e0, .survival = 0x1f0,
    .connectivity = ProcGen::CaveConnectivity::retain_components};
const ProcGen::CaveConfiguration b{.solid_fill = 5200, .steps = 4,
    .birth = 0x1c0, .survival = 0x1f0,
    .connectivity = ProcGen::CaveConnectivity::largest_floor};
auto cave_a = ProcGen::generate_cave(extents, key, a, budgets);
auto cave_b = ProcGen::generate_cave(extents, key, b, budgets);
```

Engineering choices, not inferred user taste: 0=floor/1=solid bytes, x axis0
fastest, integer fill in [0,10000] (one unit=0.01 percentage point). A solid is
born/survives iff the corresponding rule bit for its Moore-8 solid count is set.
The outer perimeter is always solid, including tiny domains. Double-buffer all
steps; cardinal-4 FIFO component analysis; largest ties keep smallest root index.
No floor fabrication, diagonal walking, corridor carving or geology claim.

Plan: separate cave algorithm/version and owned output, reuse only checked Domain,
integer stream/sampler/Work/error vocabulary; add bounded occupancy/component
buffers and conservative work reservation plus runtime charging. Test literal
initial/step/component fixtures and independent cardinal BFS, then both consumer
configs and new tests plus all 33 maze regressions once after implementation.
Capture all wrapper logs/exits, reconcile both decision views, review/stage/commit
and stop for coordinator review. Status here: proposed, not executed.

Assumptions/confidence: these rules are a useful first stylized occupancy prototype
(medium); simultaneous rules/cardinal connectivity and empty conventions are
precise (high, verification pending). No blockers. Existing source provenance is
PROCGEN-002; maze history does not establish a cave API or cellular rule preference.

### PROCGEN-015 — Implemented cave domain and cellular controls

Owners relative to this clone: `projects/tests/procedural-generation/cave.h` and
`cave.cpp`: `CaveConfiguration`, `Cave`, `preflight`, `generate_cave`, `replay_cave`.
The small two-configuration proposal above now matches `cave-consumer.cpp`.
These exact names/rules are engineering choices, not user preference evidence.
PROCGEN-002 retains exact intentional-v3/HyperMaze provenance and counterevidence;
neither that history nor recent agent APIs establishes a cave algorithm/style.
No new protected-source inspection, import, build, cache or Git operation occurred.

Domain is exactly two positive u64 extents `{width,height}` through existing
allocation-free `Domain::checked`, x=axis0 fastest, y=axis1; stride[0]=1,
stride[1]=width, index=x+width*y. Coordinates/indexes round-trip in existing Domain;
positive y advances rows, with no physical up/down convention. C=width*height is
checked before unsafe arithmetic/allocation. Every outer cell is solid, including
all cells when width<=2 or height<=2. There is no masked/subset domain. Occupancy
is one byte: 0=floor, 1=solid. Floor means walkable in this cardinal point-cell
model, not body clearance, material identity, asset selection or physical geology.

`solid_fill` is an integer in [0,10000], exact probability numerator over10000
for INTERIOR cells; one unit is 1/10000 (0.01 percentage point). No floating-point
input/rounding/clamping is offered, so NaN/infinity are not representable options.
Requested==effective on success. Reject out-of-range u32 values. Visit cells in
ascending index, skip perimeter draws, and call U1 `sample_below(10000)` for EVERY
interior cell, including fill0 and fill10000. Set solid iff sample<solid_fill.
Initial measured interior solid count/I is observed, not guaranteed coverage.
When I=0, the interior count/draws are0 and interior density is undefined; final
whole-domain density is still well-defined as0/C.
Forced boundary cells are separate from that probability denominator. Final floor/
solid counts use the WHOLE C-cell domain; floor/C is the achieved density.

Smoothing count S is 0–64, rejecting larger values rather than silently clamping.
The `birth` and `survival` u16 masks allow only bits0..8. For an interior cell,
count all eight Moore neighbors' solid bytes, excluding self. If current is floor,
new solid=(birth has bit k); if current is solid, new solid=(survival has bit k).
All other outcomes are floor. Set perimeter solid in every next buffer. Read
only the current buffer for the entire step and swap after all C updates. No
in-place asynchronous evolution or random smoothing order. Default B5678/S45678
is a useful narrow prototype choice, not a quality/difficulty/geology guarantee.
Config B uses B678/S45678. Meaningful k0/k8 and all/empty rule masks are supported.

### PROCGEN-016 — Component policy and measurements

Only two modes: `retain_components` and `largest_floor`. After smoothing, scan
root indexes ascending, flood floor cells via negativeX/positiveX/negativeY/
positiveY neighbors, mark labels before queue insertion, and count every component.
Moore-8 is ONLY smoothing; walking/connectivity is cardinal-4, so diagonal-only
contacts never connect. Each floor enqueues/pops once; no recursion or corridor
carving. Root label is root+1, zero reserved for solid/unseen. The admitted C cap
proves label increment and queue size safe. Compare component sizes with strict >,
so equal-size ties retain the first (smallest root index). `largest_floor` then
solidifies all floors whose label differs from the selected one. Never add floors.

Report initial interior solid, generated floor/components (after smoothing before
filter), final floor/solid/components, removed_floor and largest_floor. Under
retain mode removed0 and generated==final; largest_floor is the largest existing
component. Under largest mode final floor=largest_floor and removed=generated-
final; final components is1 iff final floor>0, else0. The two explicit flags are
`connectivity_required` (largest mode) and `connected` (exactly one NONEMPTY
cardinal component). Empty success is allowed in either mode: floor0, solidC,
components0, largest0, removed0, connected=false. In largest mode required=true
and connected=false explicitly reports that a nonempty connected result was not
achieved. No floor is fabricated and no proof of nonempty feasibility is implied.
Tiny all-perimeter domains follow the same empty convention, not singleton maze
conventions. A retain-mode result can happen to be connected without a guarantee.

### PROCGEN-017 — Replay, ownership and bounded resource contracts

`CaveReplay` identifies family `cave_cellular_2d`, `CaveAlgorithm::cellular_2d(1)`,
version1, exact Domain/extents, StreamKey, all requested controls and budgets.
Effective controls equal requested integer values. Cave v1 fixes probability
sampling/consumption, index order, rule semantics/perimeter, synchronous steps,
cardinal traversal, component ties and filtering. New reasons append existing
Failure vocabulary without renumbering old reasons; no old maze algorithm,
fixture, consumer source or test registration is changed. Unknown cave versions/
algorithms use dedicated cave_version/cave_algorithm reasons and text; invalid
options and domain/byte/work overflow reject explicitly.

Fresh U1 stage-derived SplitMix64 is local to every call; no clock/global RNG,
mutable registry, std::hash or library distribution. Numeric tuple encoding,
modulo-u64 arithmetic and unbiased rejection are exactly PROCGEN-004. Cave
consumer key is seed42/stage0x63617665/stream7; maze stage0x6d617a65 is unrelated.
Replaying the same algorithm/version/domain/key/controls yields the same logical
occupancy and measurements. Sufficient budgets affect admission metadata only.
Portability inherits U1's 8-bit bytes/exact u32/u64/64-bit container and fixed
metadata constraints. Cave adds no floating-point replay dependency. Actual
execution remains Linux/GNU only; not a statistical independence, collision-free
stream or cryptographic guarantee.

Cave owns its occupancy vector and descriptor/measurements/usage. Copies deep-copy
bytes, moves transfer them; a moved-from object only destroys/assigns. Const spans
borrow the owner until destruction/move/assignment. No alias of scratch/input is
returned. No existing result is an output parameter, so invalid/preflight/runtime
refusal or exceptions cannot modify a previous caller-owned Cave. `bad_alloc`,
length and programming exceptions propagate; only a local typed Stop carrying
expected Failure is caught. There is no generic exception conversion.

Use a separate `CaveBudgets{cells,storage_bytes,work}`: maze edge budgets have no
meaning for an occupancy value. Practical hard caps are1,000,000 cells,32MiB
accounted bytes,100,000,000 work, S<=64. Effective budgets=min(caller,hard).
Cave preflight checks C/I/product/byte/work arithmetic and container capacities
before any owned buffers, then cells/storage/work admission. I is0 when width<=2
or height<=2, otherwise (width-2)*(height-2). A mathematically valid huge Domain
can still overflow byte/work planning; it never triggers a huge buffer allocation.

Accounted peak is `4096 + C*(2*sizeof(u8)+2*sizeof(Index)) = 4096+18*C` bytes.
Two C-byte occupancy buffers coexist; one becomes output by move, not a third
copy. Full C-entry u64 labels and queue coexist with both buffers; no hidden
traversal/visited/recursive buffer. Static assertion protects fixed metadata/
return helpers. This requested payload+fixed allowance is not exact RSS: excludes
allocator bookkeeping/over-allocation, runtime/exception/sanitizer/OS overhead,
prior caller-owned results/copies and the separate bounded test/consumer oracle.
No actual memory pressure/OOM test or rejected huge allocation is attempted.

Work is an operation model. Charge initialized elements of all four arrays (4C),
initialization scan C, every raw initialization draw including rejection (minimum I),
each smoothing step action plus C cell updates and8I neighbor probes, component
root scan C, and each floor enqueue/pop/four probes (6F, F=generated_floor).
Largest mode additionally charges C filter actions. Bounded O(D) header/preflight
arithmetic precedes this runtime work, as in U1/U2. Let K be1 for largest mode,
0 otherwise. Preflight reserves

`W = 6*C + 7*I + S*(1+C+8*I) + K*C`.

This conservatively reserves the flood for ALL possible I floors, not the exact
necessary work for the final configuration. It can refuse a budget that would
fit a solid-heavy realized cave; this is explicit admission policy. Exact actual
work is `W - 6*(I-F) + (raw_draws-I)`. Unbiased rejection has no finite automatic
worst-case draw bound; the remaining total Work cap is enforced before every draw,
initialization, probe, flood and filter action. W suffices only when its slack
covers additional draws. Expected sampling work is not used as a completion
promise. Runtime exhaustion returns no partial occupancy or metrics. Complete
successful operation complexity is O((S+1)*C + raw_draws), bounded by effective
charged work, without a recursion proportional to C.

Generation/storage implications: the shared coordination note is read-only
requirements evidence, not a dependency. Return domain/coordinates/family/version/
seed/stage/stream/options so a future caller can identify a reproducible base.
No persisted recipe encoding, save/cache policy, exact-base edit fingerprint,
version retention/migration service or map-storage adapter is implemented. A seed
alone cannot identify a base; recording version1 does not promise indefinite
old-version execution. Persistent edits would need an exact base witness/identity
and explicit mismatch/migration behavior in a separately selected integration.
Do not turn occupancy into tile IDs to fit a sibling format. Existing maze and
cave results retain distinct semantic meanings.

### PROCGEN-018 — Executed cave acceptance and next boundary

Source: this U4 checkpoint based on accepted U3
`533fabba686653500dd6a247e46855c88bd79a0c`. Added `cave-tests.cpp` and
`procgen-cave-tests`, plus a dedicated `cave-consumer.cpp`/`procgen-cave-consumer`;
the existing procgen-tests and consumer sources are unchanged. Test-only
`cave-checks.cpp::inspect` independently scans byte counts/perimeter, checks row/
coordinate round trips, and uses pairwise coordinate Manhattan-distance1
adjacency with FIFO BFS. It does not call the generator's neighbor/label/queue
helpers or reuse its labels. Oracle caps C4096; largest new test C187, consumerC768.
Tiny initialization/steps use literal floor sets and independent Python integer/
coordinate derivations saved in `logs/u4/reference-fixtures.py/.log` under the build.
The Python code is a fixture calculation, not executed project native acceptance.

Pinned initial 4x4 seed8/stage0x63617665/stream7, fill5000/S0 has interior samples
[7647,2549,2069,5797], floor indexes[5,10], two separate diagonal components and
initial interior solid2. Largest tie retains5, removes10. B7/S(empty) one-step
fixture swaps floor indexes to[6,9]; an in-place x-fast update would incorrectly
leave10 floor too, so the literal distinguishes simultaneous semantics. On 5x5
fill0 with defaults, successive floor sets are [7,11,12,13,17], then[12], then[].
6x3 fill5000/S0 seed42 has floor[7,9,10], components sizes1/2; largest retains9/10.
These logical fixtures pin new replay without changing any maze golden vector.

Tests also cover two genuinely different configurations across seeds0/1/42/991,
independent final counts/BFS and largest selection versus unfiltered reference,
repeatability, budget-independent content, unrelated maze-stage consumption,
copy/move/local-input lifetime, previous output across invalid/refusal/allocation
exceptions, solid finite boundaries, tiny/all-solid/empty-required outcomes,
mask bits0/8/all/none, invalid rules/fill/step/policy/domain/versions, checked
extent/product/storage/work overflow, hard cell caps and exact/one-less admission.
Allocation denial proves early rejections avoid owned buffers and real bad_alloc
propagates without host pressure. 3x3 fill0/S0 requires C9/I1/4258 bytes/W61 and
uses exactly61 work/one draw under the normal key. Fill10000 uses55 but still
requires reservation61. Existing algebraic forced-zero key
{0xd78884aaeab763e4,0,0} rejects its first word for bound10000 (threshold1616):
exact61 then exhausts at runtime without a result;62 succeeds with two raw draws.
Previous owned output is preserved.

Preserved build: Linux/GNU16.2.1, C++23, Debug/Ninja, UBSan ON/no recovery and
warnings as errors. No new deps/pins, global settings, Engine/assets/GUI build,
protected/sibling change, native queue cancellation or resource refusal. All
configure/build/native operations use `/mnt/projects/cheryl-features/scripts/build --`,
one job. The shared wrapper/monitor owns adaptive limits/admission; no cap/slot
substitution or retry. Complete stdout/stderr logs plus immediate exit statuses
are local/ignored under `builds/procedural-generation/logs/u4/`:

| Wrapped operation | Log/result |
| --- | --- |
| `clang-format -i` on owned C++ changes | `format-1.log`, exit0 |
| `cmake --build builds/procedural-generation --parallel 1` | `build-1.log`, exit0; preserved standalone cache regenerated, compile/link pass; `build-2.log` checks the later diagnostic-only change |
| `ctest --test-dir builds/procedural-generation --no-tests=error --output-on-failure --verbose -R '^procgen[.]cave_' --parallel 1` | `cave-ctest-1.log`, exit0;10 passed/0 failed/0 skipped: nine cases/844 assertions plus cave consumer2/2 |
| `ctest --test-dir builds/procedural-generation --no-tests=error --output-on-failure --verbose -R '^procgen[.]' -E '^procgen[.]cave_' --parallel 1` | `maze-regressions-1.log`, exit0;33 passed/0 failed/0 skipped:32 cases/8635 assertions plus maze consumer8/8 |
| Diagnostic-only incremental build and affected case | `build-2.log`, `cave-invalid-2.log`, both exit0;1/1 entry,36 assertions,0 failures/skips; cave-specific replay diagnostics replace maze-named reasons |
| Immediate exit statuses | `exits.tsv`: all0; no failed run or diagnostic omitted |

Both nonempty selections ran once after the initial implementation, covering all43
registered entries,41 cases/9479 assertions and two consumer entries (ten
demonstrations). Later source review adds dedicated cave replay-version/algorithm
reasons instead of reusing the maze-named messages; accepted output, rules, budgets
and existing maze messages are unchanged. The affected invalid-input case is
rechecked separately as recorded in the table; successful unaffected suites are
not repeated.
There are zero skips/failures/UBSan diagnostics. No separate extra consumer rerun
or broad build is needed after these successful checks. The task's native evidence
is those runs, not source inspection or the Python calculation.

Cave consumer uses same32x24 domain C768/I660 and key42/cave/7:

| Configuration | Initial interior solid | Generated floor/components | Final floor/solid/components | Removed | Required/connected | Work/draws |
| --- | --- | --- | --- | --- | --- | --- |
| A: fill4200/S3/B5678/S45678/retain |290/660 |422/1 |422/346/1 |0 |false/true |25947/660 |
| B: fill5200/S4/B678/S45678/largest |353/660 |369/3 |337/431/1 |32 |true/true |32446/660 |

Both demonstrations pass independent boundary/count/BFS checks and print exact
semantic coordinates/replay/options/achieved density, not terrain rendering or
assets. Retaining a naturally connected A result is observed for this seed, not
a general guarantee. Full prior maze implementation/endpoint limitations/replay,
including strict true4D, remain accepted by all33 regressions.

Confidence: high for finite occupancy, simultaneous update, component and resource
contracts under tested toolchain; medium for default cellular options' usefulness.
No unresolved blocker. Broader compilers/OSes, ASan, large-domain performance,
public Engine/SDK integration and subjective cave quality/physical geology are
unverified. Next useful bounded proposal: cave clearance/distance measurements
with explicit floor-neighbor/empty conventions and independent tiny fixtures,
without activating tunnels/entrances/masks, dungeons or persistence. Advanced maze
endpoints/route/branch/loop controls stay in backlog. Geography, settlements and
tectonic modification retain separate related output/invalidation concerns;
no universal world framework, physics or AI implementation is implied.
Stop for coordinator review and publisher checkpoint transport.

Final staged review covers exactly12 task-owned files: cave production/checks/
consumer/tests, private target registration, appended Failure reasons and the
three matching records. Existing maze generation/search/configuration/tests/
consumer code and old diagnostic messages remain unchanged. Full staged diff is
saved locally as `logs/u4/staged-review.diff` under the preserved build; staged
whitespace checks pass. No unrelated or sibling work is included.
