# World integration contracts and provenance

WORLD-U1 is a new coordinator assignment after the completed finite queue. It
starts at clean tilemap `edf710a79ef4fc59c7949f7bc78d9bf7d617e29a`, fetches only
`aca49bdbfc5ac8d1f736d78f9f4ad8f4f815b56d` from the prepared candidate, verifies
all three exact source ancestors and adopts it with ordinary --ff-only. No
sibling/protected checkout, ref, process, cache or output is changed. The candidate
is the implementation checkpoint's parent, not an automatically accepted world.

[sources.json](integration/sources.json) keeps exact source identities. Complete
archived feature records/provenance remain linked from [decisions.md](decisions.md).
All original text outside rewritten link targets remains preserved, including its
historical claims/limits. WORLD-U1 statically checks all 65 relevant archived,
procedural and public CPU implementation/header paths against the candidate.
Current executed checks and limits belong in [next.md](next.md), with the committed
[execution receipt](integration/WORLD-U1.json). Stable WINT-001–006 IDs match the
compact decision view; recent agent APIs are compatibility, not user taste evidence.

## WINT-001 — Preserve exact source history and feature records

Inputs are tilemap edf710a79ef4fc59c7949f7bc78d9bf7d617e29a, storage
def28ccfc63eb057995f6e1fb95ee48cfeb8fd75 and procedural
946c2db7d9a14311a398664444fc4d1fe0417a90. The preserved maps merge is
97f53a085efd5095392edf4d77e8cd9ec4fc796f; its procedural descendant is the exact
candidate above. Each source commit is an ancestor; local adoption requires the
exact clean original tilemap head/assigned branch and candidate identity.
Those gates passed. Original nine full archived records, their three provenance
files and merge ancestry are untouched. Root integration records extend those
records rather than replacing a feature's evidence. No promotion into worker1 or
integration-ref publication is performed. Publisher/coordinator own review and
transport; source ancestry and executed behavior remain separate gates.
Confidence is high; changing inputs requires a new candidate and affected checks.

## WINT-002 — One normal shared Engine and a private real caller

[world-consumer](../projects/engine/tests/world-consumer/CMakeLists.txt) builds one
small process caller outside unit src. It links normal Cheryl::Engine and a
private cave-only archive of the actual committed maze.cpp/random.cpp/cave.cpp.
The archive supplies private generation only, never storage/tilemap/Engine objects.
The standalone PG project intentionally rejects add_subdirectory; this explicit
three-source target avoids copying an algorithm or changing that public boundary.
No public generator promotion, World framework, recipe format, JSON include or
visibility-macro patch is added.

Engine's existing shared-test owner registers this caller and the existing storage
consumer. The latter now reuses an already configured Cheryl::Engine; its original
standalone bootstrap still runs when Engine is absent. Normal shared target/source
checks remain. The original eight names, process harness and four header probes
are unchanged. Six combined public header-first probes plus the existing terrain
probe build through normal Engine usage requirements. Actual nm/readelf/ldd,
compile and link evidence check exported operations/imports, owning diagnostics,
one producer for each storage/tilemap source and the resolved clone-local DSO.
CMake registers tests but does not execute production runtime/rendering APIs.
High confidence locally; GNU/Linux source-tree Debug is not installed SDK, static,
Windows/other-toolchain, Release or broad ABI evidence. Reversal is private wiring.

## WINT-003 — Explicit identities and owned metadata

Small implemented literal caller (the raw input file is independently authored):

```cpp
#include <cheryl/core/resources/map-storage/cell-patch.h>
#include "bridge.h" // private policy; normal shared Engine linkage
const auto untouched_base = CE::Maps::load(input);
auto live = WorldProof::LiteralOwner::load(input, keys);
live->map().set({0}, {0, 0}, CE::Tilemaps::TileId{613});
auto target = live->snapshot();
auto patch = CE::Maps::CellPatches::diff(untouched_base, target);
auto applied = CE::Maps::CellPatches::apply(untouched_base, patch);
CE::Maps::save(output, applied);
// A separate verifier process loads output and compares complete literal fields.
```

KeyTable owns 1..256 unique qualified keys/nonzero uint32 IDs. Zero maps only to
nullopt. Duplicate keys OR IDs reject, even duplicate equal entries; each key is
byte-admitted before probe copies and validated by the public storage validator,
not a copied grammar. Semantic table keys have the same cheap byte admission. Current explicit
IDs are earth101, stone307, water613, flag911, flower1201, wall2017, floor4051.
They are neither atlas ordinals nor semantic terrain labels/derived visuals.
Every entry checks both lookup directions. Unknown key/ID and invalid/ambiguous
mapping refuse before replacement adoption; no fallback or truncation.

LiteralOwner copies metadata {map ID, extent, ordered layer names, ordered spawns},
KeyTable and an actual Engine Map. from/load return a fresh owned unique_ptr;
assignment adopts only after the candidate finishes. Metadata is not secretly
stored in Map. All source keys are preflighted before map population. snapshot()
preflights all numeric IDs, creates a complete candidate and revalidates before
return. It rejects whole-value Map assignment with changed dimensions/layer count;
same-shape cell edits remain ordinary Map behavior. Borrowed map views die with
the owner. No caller snapshot/table/span is retained. Destruction of source inputs
and old snapshots/maps does not change newly owned values.

The literal fixture is asymmetric 3x2, ground/detail in authored order, explicit
nulls, map proof:literal and entry/lookout spawns. Every cell in both layers and
every metadata field is checked before/after editing. Independent full literal
expected snapshots AND numeric arrays do not call the conversion bridge. A fresh
process compares full equality and reconstructs every numeric cell. This costs
bounded dense copies/scans; confidence is high in identities and ownership, low
as user preference evidence. Reversal affects this private caller only.

## WINT-004 — Separate complete-result and file publication boundaries

Storage validate() remains the semantic/encodability authority. Adapter adoption
also admits at most 65,536 total cells, dividing before multiplying after public
positive shape validation. Fixed storage caps still apply (1024 axes, 16 layers,
256 spawns, 128-byte keys, 8 MiB encoded bytes). Lookup scratch is bounded by the
admitted cells and 256-entry table; logical limits are not an exact allocator/RSS
quota. Export always validates, including changed keys' encoded byte cost.

Direct Map edits are immediate; failed reverse conversion preserves the current
runtime values and the caller's previously held successful snapshot. It does not
undo an already authored edit. Replacement owner creation preserves the previous
pointer and all values on failure. Terrain batches separately stage complete
semantic/visual results in Engine. New export/diff/apply results are owned before
adoption; snapshot() itself mutates nothing. There is no combined map/edit/file or
whole-frame/distributed transaction.

Patch::diff uses the exact untouched base and the complete final snapshot. Only
cell keys may change; metadata/dimensions/layer order and all ordered spawn fields
must match. apply compares the whole canonical base witness, not map ID/shape
alone. At most 1,024 unique changed addresses and 8 MiB witness/output budgets are
allowed. A full final difference, rather than raw brush samples, bridges terrain's
validated last-wins duplicates and storage's duplicate-address refusal.

save() is a separate fully encoded, checked same-parent stage/write/close/rename
operation. A refused invalid save preserves exact predecessor bytes and fresh
reload; successful rename is publication, not durability. No fsync/power-loss,
hostile-directory race, network-filesystem, multiwriter or compact-retention claim.
Refusals cover unknown/ambiguous conversion, shape/patch cells, change 1025,
metadata/layer/spawn differences, exact-base mismatch, duplicate patch addresses,
late terrain rule/mapping refusal and invalid prepublication output. Existing
storage fault tests retain native I/O refusal coverage. Allocation/programming
errors propagate; no OOM probes occur. Confidence high; policy is reversible private
adapter code, leaving public/v1 bytes untouched.

## WINT-005 — Persist generated meaning, derive separate visuals

The actual private 2D cave generator receives extent {5,4}, numeric stream
{42,0x776f726c64,7}, fill 4500/10000, one simultaneous Moore-8 step, birth/survival
masks 0, retain-components, and budgets {64 cells,8192 bytes,10000 work}. RNG fill
and smoothing really execute; the declared masks yield an independently known
solid perimeter and six interior floor cells. Complete occupancy is compared to
a literal 20-byte array. This is deliberately simple verifiable generation, not
an attractive cave/layout claim or an N-D maze flattening.

Application convention adopts axis0=X-right, axis1=Y-down, index=x+5*y. Solid 1
maps explicitly to label 11 and key proof:wall (numeric meaning ID2017); floor 0 to
label 23/key proof:floor (numeric ID4051). Terrain label 0 means absent, NOT cave
floor 0. A second marks layer retains flag911 and its beacon spawn. Every label,
metadata field, complementary layer and semantic numeric reconstruction is checked.

Private TerrainOwner copies the key/semantic tables and metadata and owns a fresh
unique_ptr<TerrainMap>, because TerrainMap intentionally is nonmovable. Semantic
table keys and labels must each be nonzero/unambiguous, with known persistent keys;
empty labels export null, unknown labels reject even if later overwritten. It
borrows inputs only during the call. Engine copies rule/targets/base/labels. The
rule authors cardinal N/E/S/W bits1/2/4/8; all 16 cases explicitly select target
mask, mapped by a literal 16-entry table to derived visual IDs1001..1016. No
selection/visual ID or atlas destination is persisted as meaning. Missing mappings
and needed cases retain Engine rejection, not fallback.

A three-request brush at (2,1) requests wall/floor/wall. Engine validates all and
publishes final wall once, invalidating center/N/E/S/W in existing order. Full
literal target + independent label-adjacency/bit-weight visual oracle check every
cell. Final storage diff has exactly one terrain edit. Old copied tiles/snapshot,
sibling owner and other layer remain unchanged. Fresh reload reconstructs all
20 semantic labels/visuals and 40 numeric meaning cells against independent
literals WITHOUT replaying the generator. Removal of initially unused mask 4
creates a later east-neighbor refusal at (3,1), after earlier valid candidates;
complete old labels/visuals survive. Source replay and strict bytes are unchanged.
Confidence high in the CPU bridge; terrain meanings/configuration are delegated
engineering choices, not inferred game design or user API preference.

## WINT-006 — Focused combined evidence and honest limits

Use the preserved tilemap-u1-debug configuration/incremental outputs. Build only
focused runners and public probes. Existing storage fault-injection tests need
private hidden FileOps, so their unchanged standalone CPU suite gets a clone-local
Debug output; it never substitutes for the shared combined caller. The unchanged
standalone procedural suite needs its own output because its CMake refuses
subdirectory use. Both new outputs are preserved, not borrowed from siblings.
All configure/build/native/format/symbol commands use scripts/build, job1, normal
queue, adaptive monitor/no swap/admission guards. Leak suppression stays invocation
local for instrumented checks; no global sanitizer/build selections change.

Compare discovery, execution and prior names. Run the accumulated selections once,
with --no-tests=error; recheck only affected cases after a necessary correction
or review change.
Full logs, immediate exits, child argv/stdout/stderr, registration correspondence,
source preservation and actual symbol/load paths remain clone-local. next.md and
the receipt record outcomes, including any initial failed attempts, rather than
turning a runner exit/commit into whole-feature acceptance.

No unchanged X11 :1 probe, demo/input run, atlas decode/upload, capture or graphical
teardown occurs. Real input/display/visual correctness/fun and leaks stay open.
No compact retention, saved recipe/patch format, storage migration, public procedural
API, SDK campaign, broad platform tests, sibling promotion or recurring scheduler
is selected. Next is coordinator source/evidence review of this exact descendant;
worker1 promotion requires coordinator selection, publication and drained processes.

Executed WORLD-U1 evidence: the accumulated selections pass 90/90 shared, 189/189
storage and 71/71 procedural (350 unique entries; zero failures/skips). Review then
adds pre-copy 128-byte key admission with small 129-byte refusal fixtures; only the
three affected world scenarios are rebuilt/rechecked, passing 3/3 in 1.23s. This
means 353 passing executions and 12 combined child invocations, not a second full
accumulated campaign. Final source hashes, import/loader proof and formatting
checks pass. Two initial caller compile errors were resolved before test execution;
all diagnostics remain saved. next.md records the precise invocations and limits.
