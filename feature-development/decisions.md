# Tilemap milestone decisions

U1–U5 provide finite CPU maps, checked ranges/picking, retained bindings/packets,
pass composition and the required atlas demo fixture. U6 shares a private editor
state implementation between synthetic consumers and compiled demo event wiring.
These are engineering choices under delegated authority, not user-confirmed API
or control preferences. Matching IDs in [agent-notes.md](agent-notes.md) retain
contracts, alternatives, provenance and confidence/reversal costs.

U6 starts clean at accepted ff76e5d (44/44 CPU cases). It adds seven editor cases,
private app wiring and explicit private unit source/include requirements. All
**51/51 pass, zero failed/skipped, 32.16 seconds**, under Debug ASan/UBSan. Both
tests-engine and demo build/link with immediate exit 0. Public
Map/picking/resolver/camera/input/frame/pass/backend/startup/persistence APIs and
pins stay unchanged. Exact commands, immediate exits, registration correspondence,
logs and executed-vs-pending results are in [next.md](next.md#actual-commands-and-results).

The unchanged graphical gap remains: U4's probe exited 255 with
`Error: unable to open display :1`. U6 does not retry it. Compiled demo input and
synthetic events cannot establish real display/input, asset decode/upload,
capture, graphical cleanup or leak acceptance. Only command-local detect_leaks=0
is permitted for the diagnosed tracing limitation; ASan/UBSan/halt_on_error remain
selected. No installed SDK/Windows/other-toolchain ABI acceptance is inferred.
No later unit or recurring scheduling is activated; publisher owns transport.

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

Share one private editor state between demo and synthetic events. Consume
ordered gameplay records; selection acts immediately and each click/wheel uses
its own captured pointer. Only Press edits; middle samples including empty,
left paints, right erases. Invalid operations preserve values/view, while earlier
valid events remain applied. Frames retain copied output independently of later
edits or helper destruction. This costs repeated resolution and lacks tick
rollback. Engineering confidence is high; controls are reversible private choices.

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
