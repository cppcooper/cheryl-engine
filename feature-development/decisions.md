# Tilemap milestone decisions

U1–U6 provide CPU maps, computed-edge picking/visibility, retained submission and
a private editor with compiled demo wiring. U7 proved cardinal terrain ownership;
U8 promotes that same implementation to Engine, with a public-header consumer and
shared-library diagnostic ownership. Stable IDs match [agent-notes.md](agent-notes.md).
These are engineering choices under delegated authority, not user API preferences.
[next.md](next.md) records the caller, executed checks and remaining acceptance.

U8 starts clean at accepted e97f6e8. U7 evidence remains 61 unique passing names
across an initial 60/61 run and an assertion-only correction followed by 1/1;
no all-green U7 61-case invocation is claimed. U8 builds/links Engine and the
external consumer/header probe. Its one accumulated selection passes 64/64,
zero failed/skipped, 37.21s (all 61 retained plus three new modes). Export/import
inspection confirms sole Engine production and diagnostic RTTI ownership; normal
GNU PIE COPY relocations are recorded in next.md. Initial consumer setup spelling
and local symbol-parser errors were corrected before the passing checks;
full diagnostics and immediate exits remain saved. No sanitizer diagnostic occurs.

The source-tree shared GNU/Linux ABI check does not establish installed SDK,
Windows/cross-toolchain compatibility, Release, leaks, actual input/display,
app upload or graphical teardown. X11 :1's recorded failure is not retried.
The publisher owns transport; the finite queue controller selects later units.

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
samples empty too, and frames retain copied output. U7 separately resolves all
invalidated semantic-neighbor results before publishing one terrain edit;
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
