# Tilemap milestone decisions

U1–U8 establish CPU maps/picking, retained submission/editor consumers and Engine
cardinal terrain ownership. U9 adds one bounded atomic batch operation. These
are engineering choices under delegation, not user API preferences. Stable IDs
TM-001–011 match [agent-notes.md](agent-notes.md); [next.md](next.md) records exact
contracts, caller, executed checks and remaining limits.

U9 starts clean at completed U8 `79c02fb` (64/64 Debug ASan/UBSan cases plus
public header/link checks). U7's older 61-name evidence remains an initial 60/61
plus assertion-only correction and 1/1, not a green 61-case invocation.
U9 builds/exports and registers all 64 retained names plus nine new cases.
One accumulated invocation passes 73/73, zero failed/skipped, 42.53s. Header/link
checks pass; both builds have no warnings or errors. Post-test formatting corrects
only a trailing comment and its alignment; all C++/assertion tokens remain
identical. Full diagnostics and command exits are retained.

The source-tree GNU/Linux Debug/shared checks do not establish installed SDK,
Windows/cross-toolchain, static, Release, leaks or graphics acceptance. No demo
input, dependency/pin, backend or sibling changes are selected. The unchanged
X11 failure is not retried; publisher and finite queue controller own progression.

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

Convert each captured logical top-left pointer using independent framebuffer
scales and Y inversion, then invert the private pan/positive zoom into map-local
coordinates. Shared helper state supplies picking, culling and float pass
matrices. Invalid dimensions suspend events/preparation; invalid transforms
preserve the old view. Tick sizes are selected together, without per-event resize
history. This avoids live platform reads but retains double/float precision
limits. Confidence is high in engineering, low in UI taste; reversal is private.

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

Compose west ground/detail then east ground/detail; PassWriter assigns
consecutive authored_order across row/column vectors. The private editor derives
visibility from its viewport, paints before existing detail, and retains old
frames through erase/pan. CPU cases check 15/16/15 packets and exact-edge culling
to 12. Explicit submission costs verbosity but avoids hidden selected-layer Z
or resource sorting. Engineering confidence is high from source and focused
cases; actual visual order remains unobserved.

## TM-006

Consume ordered gameplay events in the shared private editor; selection acts
immediately and each click/wheel uses its captured pointer. Press edits, middle
samples empty too, and frames retain copied output. U7/U9 separately resolve all
invalidated semantic-neighbor results before publishing an edit or bounded batch;
failures preserve labels and tiles. Neither operation promises tick/frame
rollback. This costs repeated resolution and private state. Engineering confidence
is high; naming/controls remain reversible local choices, with low taste confidence.

## TM-007

Expose the proven TerrainMap/TerrainLabel through shared Engine, retaining copied
semantic labels, cardinal rule/mapping and transactional center/N/E/S/W edits.
Export operations and anchor existing diagnostic RTTI with Engine destructors;
tests import that implementation. This enables normal consumers without a World
or asset policy, but exposes STL layout and requires compatible rebuild/relink.
Confidence is high in preserved behavior and ownership; spelling has low user-taste
confidence and public source/relink reversal cost. SDK/other-platform proof remains
open.

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

Use private --tilemap-display: 1/2 select maps, G/T layers, 3–6 colors;
left/right/middle paint/erase/sample; WASD pan, wheel zoom, R resets view.
Retain required atlas diagnostics and platform loading/cleanup. Acquire Events
before early return; simulation serializes update/preparation. Private unit
requirements compile the same app source, trading a small app/test coupling for
one algorithm. Engineering confidence is high; UI taste confidence low, reversal
local. Compiled wiring leaves real input/display unverified.

## TM-010

Keep the feature independent of sibling branches, persistence, level lifecycle,
physics and new dependencies. Record palette identity, empty cells, coordinate
and layer order as future integration seams rather than adopting another branch's
format now. This permits immediate local implementation but postpones agreements
needed for saves and streaming. Confidence is high because independence is an
explicit task requirement; no shared backend or cross-feature merge is prerequisite.

## TM-011

Coalesce duplicates last-write-wins after full validation; derive the center/cardinal
union from final semantics. Preserve first occurrence in request-center/N/E/S/W
traversal. Require request/affected/work limits; duplicates/no-ops consume request
units, cancellations invalidate nothing. Owned overlays/results precede publication,
avoiding partial strokes without full-map copies. Ordered lookups cost logarithmic
work beyond logical budget units. Engineering confidence is high; spelling/budget
granularity has low taste confidence and public source/relink reversal cost.
