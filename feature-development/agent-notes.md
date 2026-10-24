# World integration contracts and provenance

WORLD-U2 starts clean on feat/tilemap at accepted WORLD-U1
`e9289fcb992feddf85deb942a560562a54971f78`. The coordinator has promoted that
exact source into worker1; this worker performs no further merge/publication.
Current private literal-document actions are WINT-007/008 below. WORLD-U1 history
and its execution report remain preserved; final U2 evidence belongs in next.md.

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
[execution receipt](integration/WORLD-U1.json). Stable WINT-001–008 IDs match the
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
records rather than replacing a feature's evidence. WORLD-U1 performed no worker1 promotion or integration-ref publication;
the coordinator subsequently promoted its exact accepted checkpoint. Publisher/coordinator own review and
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
#include "world-document.h" // private policy; normal shared Engine linkage
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
not a copied grammar. Semantic table keys have the same cheap byte admission. The WORLD-U1 caller
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
maps explicitly to label 11 and key proof:wall (numeric meaning ID 2017); floor 0 to
label 23/key proof:floor (numeric ID 4051). Terrain label 0 means absent, NOT cave
floor 0. A second marks layer retains flag911 and its beacon spawn. Every label,
metadata field, complementary layer and semantic numeric reconstruction is checked.

Private TerrainOwner copies the key/semantic tables and metadata and owns a fresh
unique_ptr<TerrainMap>, because TerrainMap intentionally is nonmovable. Semantic
table keys and labels must each be nonzero/unambiguous, with known persistent keys;
empty labels export null, unknown labels reject even if later overwritten. It
borrows inputs only during the call. Engine copies rule/targets/base/labels. The
rule authors cardinal N/E/S/W bits1/2/4/8; all 16 cases explicitly select target
mask, mapped by a literal 16-entry table to derived visual IDs 1001..1016. No
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

## WINT-007 — Share real document ownership with the private editor

Move the verified bridge header/source to [world-document.h](../projects/apps/demo/src/world-document.h)
and [world-document.cpp](../projects/apps/demo/src/world-document.cpp). Keep namespace
WorldProof and every declaration/body unchanged; only the source include spelling
changes. The actual demo source glob and explicit private unit/world-consumer
requirements compile that SAME source, without an alternate Engine archive or
copied conversion algorithm. TerrainOwner remains available to the original world
caller but the demo's four TerrainMaps/labels/batches stay separate and unchanged.

The literal editor replaces its two Map fields with independent unique_ptr
LiteralOwner documents. map(index) retains its const facade; a private mutable_map
accessor supplies ordinary painting. document(index) is a const borrowed view for
metadata/snapshot observation. Successful LOAD destroys the replaced document;
borrowed old document/Map references expire. Owned Snapshot/Map copies and prepared
packet resources/matrices/ranges remain independent. No broad semantic-owner or
geometry refactor is selected. Two explicit signed-index casts and unsigned seed
loops keep the same proven application code warning-clean under the stricter
world consumer; no selection/mask/transform algorithm changes.

Explicit diagnostic table: tilemap:brown ->7, tilemap:black ->8,
tilemap:grey ->9, tilemap:red ->10, null ->0. Atlas binding remains original targets
0..3, so logical 7 draws target 0. These names are private diagnostic meanings,
not user game-asset preferences or inferred semantic terrain labels. Original
literal numeric layout is retained byte-for-value. Seed IDs are tilemap:west/east;
ordered layers ground/detail; ordered spawns entry(ground,0,0) and marker(detail,
1,0 west /2,0 east). A spawn references a cell without implying walkability.

LOAD uses public checked storage load, then requires exactly width 3/height 2/two
layers BEFORE numeric conversion. Valid but incompatible shapes reject without
truncation or resize. Named layers may differ: retain authored order and names,
map ID and all ordered spawns separately, while rendering/picking keep the existing
fixed ordinal layers and geometry. Complete key conversion/admitted owner creation
precedes noexcept pointer swap. A refused load preserves every previous cell and
metadata field, sibling pointer/content, current mode/layer/selection/view/brush
and old successful results; earlier events remain ordinary committed edits.

Public CPU headers/sources, schema bytes, replay, archived records/provenance and
pins are untouched. A static relocation check proves identical header bytes and
source bodies except include spelling; original unit consumer bodies remain an
unchanged prefix. Symbol/compile/loader checks supply actual source/link evidence.
Reuse costs snapshot validation and copied metadata at editor construction/load,
but avoids a second identity authority. High engineering confidence from the
existing owner and focused checks; exact names and storage UI have low taste
confidence. Reversal is private source/caller wiring; changing persisted key
meanings later would need an explicit mapping/migration choice.

## WINT-008 — Explicit ordered literal SAVE/LOAD actions

Small proposed-before-implementation caller, now implemented privately:

```cpp
#include "tilemap-editor.h"
DemoTilemapEditor editor(document_dir); // default empty disables file actions
editor.consume(events, captured_sizes); // paint, F8 Press, change, F9 Press
editor.write(frame, bindings, style);
// F8 writes selected document in full; F9 adopts a complete compatible candidate.
```

Existing options -> main -> DemoGame/GameState/GameImpl -> DemoTilemaps -> editor
constructor flow copies --tilemap-document-dir PATH. Empty disables actions with
a useful bad_request diagnostic. There is no startup load/save, directory creation,
auto-save, held action, file browser or new public startup/input API. Each action
requires an existing caller-controlled directory and uses only fixed west.json
or east.json according to selected map. Relative paths require stable process cwd;
ancestor-component/native path meaning is preserved. These are caller-supplied
paths, not an untrusted-directory sandbox or permission service.

Only admitted keyboard Press F8/F9 acts. Release/Repeat/unadmitted records and
invalid/minimized captured dimensions ignore the action, preserving the existing
editor suspension contract. Selected layer affects preceding paint; SAVE/LOAD
always processes the WHOLE selected literal document (both named layers/spawns).
Each preceding event still uses its own captured pointer; no final-axis/history
substitution is added. Selection, view, brushes and order contracts remain.

Terrain mode explicitly rejects both actions BEFORE file access/export. The normal
keyboard barrier still flushes an earlier terrain run; no whole-tick rollback is
promised. It never serializes derived visual IDs as terrain meaning. B/V and all
four semantic owners/brushes remain independent of literal document replacement.

F8 exports complete owned keys/metadata through LiteralOwner::snapshot and checked
CE::Maps::save. Unknown reverse IDs refuse before publication, preserving previous
snapshots/files and the authored runtime edit. F9 adopts only its prepared owner.
Storage/conversion/Engine bad_request errors propagate from the exact shared helper.
Actual DemoTilemaps::update catches only those anticipated categories, logs useful
context on stderr and drops the remaining current tick records; the next update
continues. Earlier successful edits/loads/saves remain. Allocation/programming and
other failures retain normal runtime handling, with no blanket catch, async queue
or retry. The real update catch is exercised synthetically without atlas loading;
that CPU evidence still does not establish graphical input execution.

Load may transiently own a complete public-storage-bounded input before rejecting
its geometry (8 MiB document, <=262144 cells/16 layers/256 spawns/128-byte keys).
Adopted docs have 12 cells and public-bounded metadata; export retains validation
and 8 MiB encodability limits. Existing bridge patch admission remains65536 cells.
These are logical bounds, not allocator/RSS quotas. No giant allocations/OOM probes
are attempted. Filesystem I/O runs synchronously on the serialized simulation
update owner and may stall it; platform constructs state before simulation and
cleans up after frames complete. No platform/window queries or borrowed event spans
survive a call. Native single-writer regular-or-absent target, checked staging/
close/rename and no-fsync/power-loss/hostile-race/multiwriter limits stay unchanged.

Alternatives: a copied adapter would invalidate shared-source proof; public Engine
promotion or a general scene/document service would grow an unproven boundary.
Automatic loading or silent shape conversion could replace unrelated state; async
I/O would require new lifetime/scheduling policy. This explicit synchronous path
trades latency for a small tested caller. Confidence high in CPU ownership/failure
contracts, low in hotkey/key-name taste. Later latency/UI changes remain a separate
assignment. Actual commands/results, remaining graphics/leak/platform/fun gaps and
coordinator next action are recorded in next.md and the U2 receipt.

Executed WORLD-U2 evidence: one accumulated shared selection passes 97/97, zero
failed/skipped,52.83s. All 90 prior names and unit bodies remain, with six new editor
cases and one fresh-process scenario. Eight world children exit 0; the new two
independently verify full west/east metadata/keys and 24 literal numeric cells.
Both preserved Debug builds link; final source/symbol/loader/format checks pass.
One initial compile attempt exposed pinned GLM sign-conversion diagnostics in the
stricter world target; only that target's existing vendor includes become SYSTEM,
leaving application Werror/conversion flags and public usage requirements intact.
Complete failed/corrected diagnostics remain saved. No CPU test rerun follows acceptance. Final staged review restores only unrelated
pre-existing GameImpl formatting; the actual demo relinks successfully, and its
changed constructor ranges pass the format check. Tested helper/adapter bytes stay
identical; no behavior change follows CPU acceptance. Actual demo update catches execute synthetically, without
atlas loading or runtime startup. Display/input/upload/graphical teardown/fun,
leaks and broader ABI/durability remain open. next.md holds commands and handoff.
