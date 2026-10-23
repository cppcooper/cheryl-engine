# Tilemap milestone decisions

U1 implements CPU values/ranges; U2 implements retained bindings and ordered CPU
packet vectors. U3 composes them through existing pass writers in synthetic cases.
U4 adds a private fixed demo display; editing remains proposed.
These choices are not user-confirmed API preferences.
The matching IDs in [agent-notes.md](agent-notes.md)
hold contracts, alternatives, provenance and pending acceptance. Each entry below
states the outcome, reason, tradeoff and confidence. The next bounded action is
described in [next.md](next.md), which records actual checks and acceptance limits.

U4 configures and builds target demo successfully with GNU 16.2.1, Debug,
shared Engine, WARN and ASan/UBSan instrumentation. The preserved CPU runner
registers and passes all 36 U1/U2/U3 regressions (no skips, 26.48 seconds).
The capability probe exits 255: `Error: unable to open display :1`. No demo,
watchdog, capture or rendered observation is executed. Required atlas Reader/
decode/manager checks are compiled but not executed by the app. The local source
PNG is inspected; it is not display evidence. Visual acceptance remains blocked.

Actual commands below use the shared wrapper/one job. The full separate graphics
configuration, independent existing-pin preparation, logs and unexecuted bounded
display command are in [next.md](next.md#actual-commands-and-results).

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u4-display-debug --target demo --parallel 1
/mnt/projects/cheryl-features/scripts/build -- timeout --signal=TERM --kill-after=2s 15s glxinfo -B
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug -N \
  -R '^tests-engine\.(tilemap|tilemap_submission|tilemap_consumer)\.'
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error -R '^tests-engine\.(tilemap|tilemap_submission|tilemap_consumer)\.'
```

The six consumer cases still establish caller order, old/new frame independence,
isolated resolver/group failure preservation and resource weak lifetimes, with
zero synthetic native counters. U4 changes only private app files/records; no
reusable API/ABI or pin changes. Leak checking remains unverified (detect_leaks=0
only per command); ASan/UBSan halt_on_error stays enabled. Visual blends/placement,
app cleanup, installed SDK/Windows exports, other compiler/STL/GLM ABIs and Release
remain unverified. Next action: after display access changes, one wrapper probe
then the preserved bounded owned-window observation. Do not retry unchanged
access or start editing/picking; transport stays with the coordinator publisher.

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
vector row/column order remains intact. U3 changes group sequence to expose
sorting/Z policies. U4 code submits four atlas-backed groups, with image observation
blocked by display access. This keeps composition flexible at the cost of
explicit calls. Engineering confidence is high from source and CPU cases;
visual outcome and personal API taste remain unverified.

## TM-006

Edits affect subsequent resolutions and independently prepared frames. U3 edits
one map and moves its view before preparing a newer frame; an older frame keeps
copied matrices, selected ranges and retained resources, while the other map's
values/output stay independent. This requires separate frame ownership and
observations outside writer/recycle phases, not concurrent-read guarantees.
Repeated packet generation costs CPU work. Confidence is high in the engineering
boundary; U4 is fixed display only and does not establish visual edit acceptance.

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
defers editing/animation. Engineering confidence is high; fixture ergonomics
have low taste confidence and local reversal cost. Actual build/display/image
evidence is separate in next.md; an unavailable observation never counts as acceptance.

## TM-010

Keep the feature independent of sibling branches, persistence, level lifecycle,
physics and new dependencies. Record palette identity, empty cells, coordinate
and layer order as future integration seams rather than adopting another branch's
format now. This permits immediate local implementation but postpones agreements
needed for saves and streaming. Confidence is high because independence is an
explicit task requirement; no shared backend or cross-feature merge is prerequisite.
