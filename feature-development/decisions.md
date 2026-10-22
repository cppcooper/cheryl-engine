# Tilemap milestone decisions

These are proposed branch-local design choices, not accepted runtime behavior or
user-confirmed API preferences. The matching IDs in [agent-notes.md](agent-notes.md)
hold contracts, alternatives, provenance and pending acceptance. Each entry below
states the outcome, reason, tradeoff and confidence. The proposed next unit is
described in [next.md](next.md); implementation has not started.

## TM-001

Use a finite CPU-owned dense map with fixed dimensions/layer count and independent
mutable cell storage. This keeps ordinary construction and editing independent
of a renderer, asset cache or level owner, while letting two instances coexist.
Dense storage costs memory proportional to every cell and defers sparse worlds.
Confidence is high from the requested scope and current ownership boundaries;
v3 offers little evidence of a preferred map representation.

## TM-002

Store application-defined logical tile IDs, reserving zero for empty, and resolve
nonzero IDs through separate retained tileset/target bindings. This distinguishes
an empty map cell from drawable atlas cell zero and avoids copying resource
handles into every cell. The extra binding step can fail for unregistered IDs;
it does not define a save format. Engineering confidence is medium, and personal
API preference confidence is low.

## TM-003

Use checked column/row coordinates increasing right/down and half-open map
bounds, with explicit top-left world placement converted to existing Y-up asset
geometry. Reject invalid edits rather than wrapping. This makes row-major
authoring predictable and keeps atlas coordinates distinct from map positions.
The tradeoff is a documented coordinate conversion, including pivot adjustment.
Confidence is medium: source constrains geometry, while this map convention is
our reversible proposal.

## TM-004

Compute visible cell ranges by intersecting a finite map-local rectangle before
floor/ceil conversion to half-open cell bounds. Empty/touching regions emit
nothing, and malformed rectangles reject. This avoids negative-to-unsigned
errors and scans only potentially visible cells. The caller must supply the
camera-to-map conversion; selection itself does not clip pixels. Engineering
confidence is high, supported by existing positive-area overlap semantics rather
than a known user preference.

## TM-005

Submit layers explicitly in caller order, traversing each visible layer by row
then column. The consumer draws ground before detail and chooses the order of
map instances or inserted actors. Existing packet playback already preserves
insertion order with depth-disabled ordinary materials. This gives up automatic
texture sorting and requires deliberate submission calls. Confidence is high
from the brief and current rendering contracts; exact layer naming remains provisional.

## TM-006

Apply edits immediately on the simulation owner and regenerate visible packets
from current cells. Advance each instance's elapsed time during update, resolving
animation from original targets during preparation. This avoids shared tileset
mutation and stale edit caches, including when frames are skipped. Repeated CPU
resolution costs more than cached meshes. Compatibility confidence is high;
confidence that recomputation is sufficient remains medium until the small
consumer is exercised.

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
