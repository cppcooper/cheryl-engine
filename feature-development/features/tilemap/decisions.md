# Tilemap milestone decisions

U1–U9 establish CPU maps/picking, retained submission, a private literal editor,
Engine terrain ownership and bounded atomic batches. U10 adds an optional
semantic mode to that same private helper/demo. Engineering choices under
delegation are not user API preferences. Stable TM-001–012 IDs match
[agent-notes.md](agent-notes.md); [next.md](next.md) records actual checks/limits.

U10 starts clean at completed U9 `bdead05` (73/73 Debug ASan/UBSan). Both demo
and CPU runner compile/link without warnings. One accumulated invocation passes
79/79, zero failed/skipped, 45.48s, retaining all 73 prior names. Public Engine
header/export checks and same-private-source proof pass. There are no code or
comment corrections after execution. Prior U7 evidence remains an initial 60/61
plus correction/1-case rerun; U8/U9 have their separately recorded green runs.

No public Engine/backend/input API, dependency pin or asset changes. The native
app is compiled, not executed. Real input/display, app decode/upload, graphical
teardown, leaks, installed SDK and wider ABI remain unverified. X11 :1 is not
retried. This is the last pinned tilemap unit in the finite batch; publisher owns
transport, and coordinator review owns any subsequent assignment.

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

Consume ordered captured events in the same private editor. Literal edits apply
immediately; semantic paint/erase runs use Engine batches, flushing before
keyboard/sample/wheel or call end. A refusal preserves its run; earlier runs
survive. This preserves pointer ownership and final semantics without tick/frame
rollback, but adds explicit barriers and separate stores. Engineering confidence
is high; controls and grouping are reversible private choices with low user-taste
confidence.

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

Keep --tilemap-display's literal default; B/V select separate semantic/literal
stores and brushes. Shared map/layer selection and view persist. Reuse required
atlas diagnostics, platform loading/capture and cleanup; simulation serializes
updates/preparation. The unit runner compiles the same private app source,
trading small test/app coupling and four tiny terrain owners for exact consumer
coverage. Engineering confidence is high; controls have low taste confidence and
local reversal cost. Real display/input remains unverified.

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

## TM-012

Use opaque labels in private terrain mode, mapping N/E/S/W masks through mask%4
to explicit IDs 7–10. Start four isolated semantic layers empty; preserve literal
cells across switches. Bound click runs to 32 requests, six affected cells and
256 logical work units; sample labels after flushing. This proves editing without
new art or label inference, but colors alias masks and errors skip the remaining
tick. Confidence is high in contracts, low in UI taste; reversal is private.
