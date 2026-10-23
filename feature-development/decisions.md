# Tilemap milestone decisions

U1 implements CPU values/ranges; U2 implements retained bindings and ordered CPU
packet vectors. The first graphical consumer and pass integration remain proposals.
These choices are not user-confirmed API preferences.
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

Bind nonzero logical IDs to retained immutable tilesets and original atlas targets;
zero stays empty while atlas zero remains drawable. Setup rejects duplicates,
null assets/images, invalid grids/pivots/targets and invalid timelines before
insertion. Resource compatibility stays with existing per-cell resolution. This
retains metadata longer and restricts directly assembled assets, but avoids
per-cell handles and delayed metadata failures. Engineering confidence is high;
naming and palette ergonomics have low personal-preference confidence.

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

Return each requested layer's visible nonempty cells in increasing row/column
vector order. The caller chooses layer/instance order; packets retain default
zero authored_order until the existing pass writer assigns it. This preserves
explicit composition without texture sorting, at the cost of deliberate layer
calls and one packet per cell. Engineering confidence is high from ordered
synthetic outputs and current pass contracts; actual pass/graphical integration
is deferred, and exact API vocabulary has low preference confidence.

## TM-006

Read current cells and resolve each visible binding's original target exactly
once at caller-provided nonnegative milliseconds. Return new packets without
mutating shared assets or owning a clock. Immediate edits and independent times
therefore affect only subsequent resolutions; older vectors retain their values.
Repeated lookup/resolution costs CPU work instead of cache invalidation machinery.
Engineering confidence is high from animation-boundary, independent-instance and
erase cases; future consumer scheduling and performance remain unverified.

## TM-007

Begin with explicitly authored tile IDs and leave automatic terrain/Wang editing
for a later unit. Existing selection remains a synchronous borrowed sampler
boundary, with distinct outside/unknown observations and ordinary failure values.
This avoids inventing terrain meaning from visual IDs or animating selected cells
twice. The tradeoff is no automatic neighbor redraw in this milestone. Confidence
is high about compatibility and medium about this scope choice, without v3 preference proof.

## TM-008

Resolve one layer through existing resolve_tile into a function-local owned
packet vector, with copied matrices and retained geometry/material/images.
Failure after a valid cell exposes no partial vector and preserves caller inputs;
there is no whole-frame transaction. Native calls and pass insertion remain
outside this adapter. This accepts temporary vectors and repeated validation.
Engineering confidence is high from synthetic retention/failure checks; native
domain lifetime still belongs to the backend, and performance suitability remains
unmeasured.

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
