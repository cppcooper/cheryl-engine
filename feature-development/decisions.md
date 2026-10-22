# Tilemap milestone decisions

U1 implements CPU values and visible ranges; rendering and the first graphical
consumer remain proposals. These choices are not user-confirmed API preferences.
The matching IDs in [agent-notes.md](agent-notes.md)
hold contracts, alternatives, provenance and pending acceptance. Each entry below
states the outcome, reason, tradeoff and confidence. The proposed next unit is
described in [next.md](next.md), which records actual checks and acceptance limits.

## TM-001

Own finite dense CPU cells, independent of renderers and levels. Deep copies and
whole-value assignments own independent storage; assignment may replace dimensions
without a resize API. Moves transfer data and leave a safely rejected zero-state
source; self-assignment preserves values. This makes coexistence and ownership
predictable, at the cost of dense allocation and copy work. Engineering confidence
is high; the representation remains a proposal about taste, with little v3 evidence.

## TM-002

Store application-defined logical tile IDs, reserving zero for empty, and plan to
resolve nonzero IDs through separate retained tileset/target bindings. This distinguishes
an empty map cell from drawable atlas cell zero and avoids copying resource
handles into every cell. The extra binding step can fail for unregistered IDs;
it does not define a save format. Engineering confidence is medium, and personal
API preference confidence is low.

## TM-003

Use signed 64-bit points and layer indices, checked against nonzero uint32 map
dimensions and layer count before indexing. Columns/rows increase right/down
within half-open bounds; invalid edits reject without mutation. This makes
negative external input safe and keeps atlas coordinates separate. Future drawing
must convert to Y-up geometry and adjust pivots. Engineering confidence is high
for checked access, medium for the reversible coordinate choice; personal-preference
evidence remains low.

## TM-004

Intersect the view with the finite map, then binary-search computed double grid
edges for half-open cell bounds. Exact touching edges are excluded without an
epsilon, including decimal sizes where bare floor/ceil division misclassifies
edges. Empty regions return nothing and malformed inputs reject. Logarithmic
search costs more comparisons than raw division but avoids rounded-index repairs.
Engineering confidence is high; an executed independent intersection oracle covers
the boundary risk, while personal-preference confidence stays low.

## TM-005

Submit layers explicitly in caller order, traversing each visible layer by row
then column. The consumer draws ground before detail and chooses the order of
map instances or inserted actors. Existing packet playback already preserves
insertion order with depth-disabled ordinary materials. This gives up automatic
texture sorting and requires deliberate submission calls. Confidence is high
from the brief and current rendering contracts; exact layer naming remains provisional.

## TM-006

Apply CPU edits immediately on the caller's owner; no allocation, palette check or
notification accompanies a cell replacement. Future rendering will regenerate
visible packets and resolve animation using independently updated instance clocks.
This separates map mutation from shared asset state and avoids stale edit caches,
at the cost of repeated resolution. Confidence is high for CPU ownership and
existing playback compatibility, medium for future rendering cost, and low for
inferred personal taste.

## TM-007

Begin with explicitly authored tile IDs and leave automatic terrain/Wang editing
for a later unit. Existing selection remains a synchronous borrowed sampler
boundary, with distinct outside/unknown observations and ordinary failure values.
This avoids inventing terrain meaning from visual IDs or animating selected cells
twice. The tradeoff is no automatic neighbor redraw in this milestone. Confidence
is high about compatibility and medium about this scope choice, without v3 preference proof.

## TM-008

Resolve each visible nonempty cell through existing CPU tile submission, returning
an ordered retained packet vector for one layer. Add that group through the
existing pass writer so failure preserves earlier draws. Packets copy placement
and retain resources independently of maps; native domain lifetime still belongs
to the runtime. This accepts per-cell draws and temporary vectors. Compatibility
confidence is high; performance suitability is unmeasured and therefore medium.

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
