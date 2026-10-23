# Tilemap milestone decisions

U1 implements CPU values/ranges; U2 implements retained bindings and ordered CPU
packet vectors. U3 composes them through existing pass writers in synthetic cases.
U4 adds a private fixed demo display; U5 adds checked CPU point picking and a
transformed pick/paint/erase consumer. Private interactive demo wiring remains proposed.
These choices are not user-confirmed API preferences.
The matching IDs in [agent-notes.md](agent-notes.md)
hold contracts, alternatives, provenance and pending acceptance. Each entry below
states the outcome, reason, tradeoff and confidence. The next bounded action is
described in [next.md](next.md), which records actual checks and acceptance limits.

U5 starts from accepted source/build checkpoint 6a85f16. The preserved Debug
shared Engine runner builds/discovers successfully and passes **44/44**, no skips,
39.52 seconds, under ASan/UBSan with halt_on_error. This is all 36 accepted cases
plus five point-query and three transformed consumer cases. Header-first core
compilation retains no asset/render/camera dependency. No unchanged display retry
runs: U4's probe exited 255, `Error: unable to open display :1`. App atlas loading,
actual display/edit/capture and graphical cleanup remain unverified.

Actual commands (one job, existing wrapper/lock, preserved configuration):

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

All immediate exits are 0; full commands, diagnostics, registration correspondence
and saved u5-results.json are in [next.md](next.md#actual-commands-and-results).
The independent rectangle oracle covers exact edges/neighbors and small/large/
subnormal sizes; maximum uint32 geometry needs no Map allocation. Pick/paint/erase
changes only one layer, preserves sibling cells/output and old retained frames,
keeps authored order and leaves synthetic native counters zero. Leak checking is
unverified (command-local detect_leaks=0 only); no sanitizer/build setting changes.

The API addition requires new users to relink a compatible Engine; Map layout and
existing methods are unchanged. Installed SDK/Windows exports, other compiler/
STL/GLM ABIs and Release remain unverified. Next proposed unit: private interactive
demo wiring after coordinator selection. Graphical checks need changed display
access; no later unit, retry loop or transport is started here.

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
is high from source and this coordinator-selected engineering contract under
delegated authority; it is not an explicit user API preference. Exact ergonomics
remain a low-confidence taste inference.

## TM-003

Keep signed cells checked and place Y-down rows in Y-up world space with source
pivots and positive source-to-cell scales. U5's local consumer inverts pan/zoom
before picking and derives visibility from its viewport. Validate finite origins,
points, float matrices and noncollapsed viewport bounds before editing. Adapter
base model remains identity and style scale one. This excludes general camera
transforms and arbitrary precision. Engineering confidence is high from CPU
cases; naming/preference confidence remains low.

## TM-004

Return owned optional<Point> from pick_cell; invalid geometry/nonfinite input
throws invalid_args, while valid exterior points miss. Share computed-edge
validation/search with visible_cells: top/left inclusive, internal edges choose
the following cell, final edges exclusive. Support subnormals and maximum uint32
geometry without allocation or enumeration. Binary search costs comparisons but
avoids quotient/epsilon ownership errors. Engineering confidence is high from
independent rectangle oracles; exact spelling has low taste confidence and local
source/relink reversal cost.

## TM-005

Compose maps/layers in explicit caller order; PassWriter assigns consecutive
authored_order across row/column vectors. U3 reverses groups to expose sorting.
U5 derives visibility from pan/zoom, adds a picked ground cell before its existing
detail and checks 6/7/6 packets across paint/erase frames. Shifted views cull
exactly at cell edges. Explicit calls cost verbosity but avoid hidden Z/resource
policy. Engineering confidence is high from source and CPU cases; actual visual
order remains unobserved.

## TM-006

Validate transforms and pick before mutating one chosen layer; exterior misses
and invalid inputs preserve all cells. U5 paints, prepares, erases and prepares
again, preserving the other layer, sibling map/output and older frames' copied
matrices/ranges/resources after fixture destruction. This gives synchronous CPU
ownership, not concurrent publication or graphical edit acceptance. Repeated
resolution costs CPU work. Engineering confidence is high from focused cases;
private interactive wiring remains a later unit.

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

Use private --tilemap-display mode for two fixed 3×2/two-layer maps, separate
placements and translucent details. Validate the local four-color atlas through
Reader/decode/managers before binding; fail with required-source diagnostics.
Reuse platform loading/cleanup and existing packet/pass/camera contracts,
bypassing optional samples and controls. This limits setup and ambiguity but
defers interactive editing/animation. Engineering confidence is high; fixture ergonomics
have low taste confidence and local reversal cost. Actual build/display/image
evidence is separate in next.md; an unavailable observation never counts as acceptance.

## TM-010

Keep the feature independent of sibling branches, persistence, level lifecycle,
physics and new dependencies. Record palette identity, empty cells, coordinate
and layer order as future integration seams rather than adopting another branch's
format now. This permits immediate local implementation but postpones agreements
needed for saves and streaming. Confidence is high because independence is an
explicit task requirement; no shared backend or cross-feature merge is prerequisite.
