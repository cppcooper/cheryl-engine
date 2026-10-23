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
