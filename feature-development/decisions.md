# Tilemap milestone decisions

U1 implements CPU values/ranges; U2 implements retained bindings and ordered CPU
packet vectors. U3 composes them through existing pass writers in synthetic cases.
The first graphical consumer remains a proposal.
These choices are not user-confirmed API preferences.
The matching IDs in [agent-notes.md](agent-notes.md)
hold contracts, alternatives, provenance and pending acceptance. Each entry below
states the outcome, reason, tradeoff and confidence. The proposed next unit is
described in [next.md](next.md), which records actual checks and acceptance limits.

U3's final run passed six consumer cases plus all 30 U1/U2 regressions: 36/36,
no skips, 28.26 seconds, Debug ASan/UBSan. Build, discovery and execution exited 0.
The checks establish caller-controlled order, old/new frame independence, separate
resolver/group failure preservation and weak resource release. Leak checking,
real graphics-domain/visual behavior, installed SDK/Windows and other ABI/Release
contexts remain unverified. The next proposed action is U4's minimal two-map,
two-layer display fixture, after suitable local-art validation and a later
coordinator assignment; no next unit starts here.

Commands executed from this clone (one job through the shared wrapper; leak
detection disabled only per invocation):

```sh
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug --target tests-engine --parallel 1
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug -N \
  -R '^tests-engine\.(tilemap|tilemap_submission|tilemap_consumer)\.'
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error -R '^tests-engine\.(tilemap|tilemap_submission|tilemap_consumer)\.'
```

## TM-001

Own finite dense CPU cells, independent of renderers and levels. Deep copies and
whole-value assignments own independent storage; assignment may replace dimensions
without a resize API. Moves transfer data and leave a safely rejected zero-state
source; self-assignment preserves values. This makes coexistence and ownership
predictable, at the cost of dense allocation and copy work. Engineering confidence
is high; the representation remains a proposal about taste, with little v3 evidence.

## TM-002

Retain handles and original atlas targets for nonzero IDs; zero stays empty,
atlas zero draws. Retention neither deep-copies nor freezes an asset. Metadata
must remain unchanged through every alias while any binding retains it, including
table copies. Per-call table/resource stability also applies. Setup validation
preserves earlier entries, trading flexibility for bind-once safety. Confidence
is high from the explicit user contract and source; exact API ergonomics remain
a low-confidence taste inference.

## TM-003

Keep signed map points/layers checked before storage access. Place Y-down cells
in Y-up world space using normalized source pivots and independent positive X/Y
source-to-cell scaling. Require identity base model, unit style scale and finite
float-representable placement/pass matrices. This supports non-square frames
without reversed artwork, but excludes arbitrary transforms and extreme-scale
precision guarantees. Engineering confidence is high from synthetic corner checks;
coordinate/naming preference confidence remains low and the adapter restriction
can change without altering CPU maps.

## TM-004

Intersect views with the finite map, then binary-search computed double grid
edges for half-open cell bounds. Exact touches exclude neighboring cells without
an epsilon, including decimal sizes where bare division misclassifies edges.
Future picking must agree with those boundaries. Logarithmic search costs more
comparisons than division but avoids rounded-index repairs. Engineering confidence
is high from the independent intersection oracle; no picking API or personal
coordinate preference is implied.

## TM-005

Compose two maps/two layers by adding resolved vectors in explicit caller order.
Existing PassWriter assigns consecutive authored_order values across groups;
vector row/column order remains intact. U3's synthetic consumer changes group
sequence to expose implicit sorting or Z policies. This keeps composition
flexible without a scene abstraction, at the cost of explicit calls. Engineering
confidence is high in the source contract; actual commands/results are in next.md.
Rendering and user preference beyond the explicit requirement remain unverified.

## TM-006

Edits affect subsequent resolutions and independently prepared frames. U3 edits
one map and moves its view before preparing a newer frame; an older frame keeps
copied matrices, selected ranges and retained resources, while the other map's
values/output stay independent. This requires separate frame ownership and
observations outside writer/recycle phases, not concurrent-read guarantees.
Repeated packet generation costs CPU work. Confidence is high in the engineering
boundary; native execution status is recorded separately from future visual acceptance.

## TM-007

Begin with explicitly authored tile IDs and leave automatic terrain/Wang editing
for a later unit. Existing selection remains a synchronous borrowed sampler
boundary, with distinct outside/unknown observations and ordinary failure values.
This avoids inventing terrain meaning from visual IDs or animating selected cells
twice. The tradeoff is no automatic neighbor redraw in this milestone. Confidence
is high about compatibility and medium about this scope choice, without v3 preference proof.

## TM-008

Use existing vector insertion after CPU resolution. U3 separately checks
resolver failure after a valid cell and group rejection after invalidating only
a later validated packet's range. Earlier pass draws survive both. A frame
alone retains resources after fixture destruction; a copied vector survives
frame recycling. Clearing it expires resource weak handles. This gives group
preservation, not frame rollback or graphics-domain teardown. Confidence is high
from executed cases;
actual native results and leak/visual limits are recorded in next.md.

## TM-009

Use a dedicated sample helper in the existing demo for two editable 8×6,
two-layer maps, with separate placement, visibility and clocks. Reuse the current
tileset loading and input/frame path, keeping map authoring entirely in memory.
This minimizes new application setup but needs available artwork for visual
acceptance; synthetic CPU checks remain independent. Scope confidence is high,
scaffold confidence medium, and unavailable artwork must never count as a pass.

## TM-010

Keep the feature independent of sibling branches, persistence, level lifecycle,
physics and new dependencies. Record palette identity, empty cells, coordinate
and layer order as future integration seams rather than adopting another branch's
format now. This permits immediate local implementation but postpones agreements
needed for saves and streaming. Confidence is high because independence is an
explicit task requirement; no shared backend or cross-feature merge is prerequisite.
