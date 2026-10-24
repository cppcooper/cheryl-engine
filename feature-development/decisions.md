# Tilemap milestone decisions

U1–U6 provide finite CPU maps, computed-edge picking/visibility, retained
bindings/packets, pass consumers and a private tested editor with compiled demo
wiring. U7 adds a private CPU terrain consumer with separate semantic labels,
explicit target-to-logical mapping and transactional local updates. These choices
are engineering decisions under delegated authority, not user-confirmed tastes.
Stable IDs match [agent-notes.md](agent-notes.md); [next.md](next.md) records the
caller, exact commands, executed results and remaining acceptance.

U7 starts clean at accepted fb0feef (51/51 Debug ASan/UBSan cases). The preserved
CPU runner builds and registers 61 nonempty names: all 51 accepted plus eight
terrain cases and two retained-frame consumers. Initial execution passes 60/61
(all 51 regressions), zero skips, in 36.52 seconds; the negative consumer fixture
expected 11 setup packets but correctly produced 15. That assertion is corrected;
the focused case passes 1/1 (0.42s, exit 0). All 61 unique names have passing
evidence across these runs; no whole-suite exit-0 rerun is claimed. No helper
behavior changed for the fixture error.

Only private unit consumer files/records change. Public Engine/editor/input/
camera/renderer APIs, build selections and pins stay unchanged; demo compiled
inputs do not change and require no rebuild. Synthetic CPU evidence does not
establish graphical input/display, app asset upload, native teardown, leaks,
installed SDK/Windows or cross-toolchain ABI acceptance. ASan/UBSan/halt_on_error
remain selected, with only command-local detect_leaks=0 for the diagnosed tracing
limitation. U4's `Error: unable to open display :1` remains unchanged and is not
retried. Publisher owns transport; stop after the coherent U7 checkpoint.

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

Own semantic labels separately from copied visual tiles; map selection targets
through a caller-owned table copied at construction. Reuse four-neighbor select
with authored cardinal bit order and empty outside samples. Empty labels clear;
unchanged labels do nothing. Resolve center plus unique bounded cardinal neighbors
before publication; failures preserve both. This avoids visual-ID
terrain inference but copies dense state and lacks Wang/diagonals or live rule
replacement. Engineering confidence is high; private spelling has low taste
confidence and local reversal cost.

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
