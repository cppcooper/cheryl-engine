# World integration contracts and provenance

WORLD-U6 is a standalone continuation on exact clean reviewed feat/tilemap
`9cfa376a6366012fff2e1399af2c5d87acfa471c`. Before implementation, require the
committed U5 old-batch receipt ready/true, nine nonempty logs and119 passing XML
entries. WINT-015/016/017 extend the current world namespace. Completed plans /
receipts and all feature archives remain immutable; no sibling/climate input or
queue job is selected. Commands/results and standalone receipt belong in next.md.

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
[execution receipt](integration/WORLD-U1.json). Stable WINT-001–012 IDs match the
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

LOAD uses public checked storage load, then WINT-013 admission (axes1–64,
exactly two layers, <=8192 layer cells) BEFORE numeric conversion. Valid but incompatible shapes reject without
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

## WINT-008 — Explicit ordered SAVE/LOAD actions

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

WORLD-U2 explicitly rejected terrain actions before file access. WORLD-U3 replaces
that refusal with WINT-009/010's separate semantic documents/files. The normal
keyboard barrier still flushes an earlier terrain run; no whole-tick rollback is
promised. No derived visual IDs are serialized as meaning. B/V storage and brushes
remain independent, including literal document replacement.

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

## WINT-009 — Two-layer semantic ownership and atomic adoption

Proposed-before-implementation caller, implemented in the same private sources:

```cpp
#include "tilemap-editor.h"
DemoTilemapEditor editor(existing_directory);
// press()/paint() construct actual admitted records in world-consumer/main.cpp.
const std::array records{press(Key::B), press(Key::G), paint(40,128),
                         press(Key::T), paint(72,144), press(Key::F8)};
editor.consume(records, {{400,200},{800,600}});
// Later: erase, F9 Press; both semantic layers and all metadata are restored.
editor.write(frame, bindings, style);
```

The actual WORLD-U1/U2 adapter world-document.h/cpp adds only a private
TerrainDocument composition. It owns two TerrainOwner pointers (each containing
the SAME public nonmovable Engine TerrainMap) plus copied key/semantic tables,
rule and target vector used for future reconstruction. Existing single-layer
TerrainOwner/LiteralOwner algorithms stay unchanged. Two editor semantic-document
pointers replace only the four raw terrain owner pointers; terrain(index,layer)
remains the selected-layer read facade. Literal document owners, terrain brushes,
ordered event barriers, geometry/picking and pass submission policy remain.

Each document retains map ID, admitted extent (WINT-013), two ordered arbitrary valid layer names
and every ordered spawn. from()/replacement() validate the storage snapshot and
fixed shape, build layer 0 then layer 1 in a LOCAL owned array, copy reconstruction
configuration, and return a complete candidate. Editor F9 swaps exactly one pointer
only after all of this succeeds. A missing rule/target reached solely in layer 1
cannot publish layer 0 or metadata. Candidate allocation/derivation/validation
exceptions destroy only local candidates. Allocation/programming exceptions still
propagate; no exact allocator/RSS quota or no-throw promise is made.

snapshot() exports both selected semantic layers, combining layer 0 from the first
owner with layer 1 from the second, then validates the complete result. A TerrainMap
inside one owner has a construction-time complementary numeric-meaning layer;
that layer is neither live sibling terrain nor a combined render map. Do not
export/resolve it as such. The editor resolves only each owner's selected layer.
This small composition intentionally repeats bounded metadata/maps/configuration
instead of refactoring the proven Engine or single-layer integration adapter.
Old borrowed document/terrain views expire on replacement; full owned snapshots,
packet ranges/matrices and retained resources remain independent. Engine edit_batch
continues to validate every request, coalesce last-wins and publish only its chosen
layer. Invalid/unmapped labels reject even when overwritten later in a run.

Construction and reconstruction borrow source snapshots/tables/rules/targets only
for the call. Future replacement uses OWNED copies, proven after original inputs
are destroyed/cleared. Neither event spans nor sampler closures survive. Platform
constructs state before simulation; simulation update/preparation serializes
access; cleanup follows completed frames. Concurrent editing/preparation/recycling
is excluded. High engineering confidence from owner source and checks, low as
personal naming/API taste. Reversal is private source wiring; no public header,
Engine ABI, strict-v1 byte/schema or replay change.

## WINT-010 — Semantic identity, kind compatibility and file actions

The actual editor owns an explicit reversible table:

| Qualified key | Opaque label | Numeric meaning ID | Visual/atlas policy |
| --- | --- | --- | --- |
| null |0 |0 | empty, no packet |
| terrain:one |1 |101 | cardinal mask selects target; no reverse inference |
| terrain:two |2 |203 | same policy, still a DIFFERENT meaning |
| terrain:three |3 |307 | same policy |
| terrain:four |4 |409 | same policy |

Existing N/E/S/W mask bits 1/2/4/8 select target mask%4 and visual IDs 7/8/9/10;
bindings still select atlas targets 0/1/2/3. Different isolated labels (and masks
with the same remainder) can render identically without sharing semantic keys.
The document does not contain derived visuals, rule metadata or a recipe/version.
Reconstruction uses the document owner's copied active cardinal rule/target table;
the caller must keep this semantic-key meaning consistent across sessions. Visual
palette changes may rederive different appearances while preserving meaning. No
rule fingerprint, asset identity or automatic migration is promised.

Semantic map IDs must begin terrain:; seed IDs are terrain:west/east. This is a
PRIVATE kind convention in the existing opaque v1 ID, not a schema property.
Literal editor loads reject that reserved prefix; semantic loads require it.
Disjoint qualified cell tables reject cross-mode nonempty files; the ID convention
also distinguishes all-null files. Literal keys/colors/layouts and normal metadata
stay compatible; the new reserved prefix narrows previously arbitrary literal
editor IDs. The standalone LiteralOwner/storage API still accepts valid IDs
unchanged. This reversible private restriction is preferable to guessing empty
file kind or introducing public format changes.

Key tables require 1..256 unique keys and nonzero numeric IDs. Semantic tables
require 1..256 unique known keys and nonzero unique labels. Each loaded nonnull
cell must map in both directions; empty/unknown/duplicate tables and unknown labels
refuse. The editor supplies exactly four entries above; private composition can
use other reversible tables. Rules remain finite cardinal-only with supported
bit order. Missing mask cases or missing/zero/out-of-range targets are permitted
until needed, including initially empty layers; required failures reject whole
candidate reconstruction or the current Engine batch. No equality is inferred
from visual IDs; null is the ONLY empty semantic meaning.

Only admitted keyboard Press F8/F9 performs synchronous SAVE/LOAD. B selects
west-terrain.json/east-terrain.json, V retains west.json/east.json. Selected layer
controls paint but each file action covers BOTH named layers/all spawns. Empty
directory default disables both modes; an explicit existing caller-controlled
single-writer directory is required. No startup/automatic I/O, directory creation,
held input or cross-mode fallback. Release/Repeat/unadmitted and invalid/minimized
sizes remain ignored; each pointer comes from its own event, with unchanged
independent framebuffer scales, half-open computed edges, pan and zoom.

An earlier terrain run flushes at the keyboard barrier before I/O. F8 builds a
complete semantic Snapshot then uses unchanged checked save; a failed save retains
runtime edits and predecessor bytes under the native controlled-directory profile.
F9 owns full parsed input before shape/kind/conversion refusal, and stages both
owners before adoption. Missing/malformed/wrong-kind/wrong-shape/unknown meaning
failures preserve document pointer, all semantic/visual values/metadata, sibling
and literal documents, state and old frames. Current demo update catches anticipated
bad_request/storage/conversion errors, logs and drops the rest of the tick;
earlier completed runs stay applied and later updates continue. CPU tests execute
that SAME update catch without starting a renderer or loading the atlas.

Storage retains 8 MiB input/encoding,262144 cells/16 layers/256 spawns/128-byte keys;
bridge patch admission retains 65536 cells. Adopted geometry is exactly 12 cells per
document; reconstruction temporarily owns old/new snapshots, two complete Maps,
labels and several configuration/metadata copies. Semantic tables are capped 256;
the real editor uses four target entries and 16 mask cases. Private callers of the
underlying owner remain responsible for sizing supplied target vectors; these are
logical operation bounds, not allocation-byte budgets. U5 updates batch input caps to
32 requests/160 affected cells/1024 work per run (WINT-014). No giant/OOM probes occur.

Tradeoff: synchronous I/O can stall simulation, but avoids asynchronous ownership
policy. Rename file publication is SEPARATE from authored edits/runtime adoption:
no distributed transaction, fsync/power-loss, hostile-race or multiwriter guarantee.
Private keys/prefix/hotkeys are engineering choices under delegation, not inferred
user preference; compatibility and successful CPU checks support high engineering
confidence. Changing persisted keys requires explicit conversion; other reversal
cost is private only. Exact executed counts, ownership inspections and remaining
visual/input/upload/teardown/fun/leak/platform gaps are in next.md and the receipt.

## WINT-011 — Owned document commands and complete restoration

Small proposed-before-implementation caller, now exercised by world-consumer's
actual admitted-record factories and the same editor source compiled into demo:

```cpp
DemoTilemapEditor editor(existing_directory);
const std::array literal{press(Key::Digit6), paint(72,144), press(Key::F6),
                         press(Key::F7), press(Key::F8), press(Key::F9)};
editor.consume(literal, {{400,200},{800,600}});
const std::array semantic{press(Key::B), press(Key::G), paint(40,128),
                          press(Key::T), paint(72,144), press(Key::F6),
                          press(Key::F7), press(Key::F8), press(Key::F9)};
editor.consume(semantic, {{400,200},{800,600}});
auto available = editor.history(0, DemoTilemapEditor::Mode::Terrain); // owned counts/byte charge
editor.write(frame, bindings, style);
```

Private DocumentHistory lives beside the existing owners in world-document.*.
Four independent timelines (west/east x literal/semantic) each own complete before
and after Snapshots; one cursor represents undo/redo stacks. A command is exactly
one accepted changing literal paint/erase or one completed changing Engine semantic
input batch, after last-write-wins coalescing. Labels and qualified semantic keys
are history, never selected visual/atlas IDs. Complete map ID, dimensions, ordered
layer names, all ordered spawns and all cells participate in equality/restoration.
Layers share one document history; selection/brush/view/mode are deliberately NOT
saved. Undo after changing the selected layer can restore another layer without
changing that selection. Map/mode switching merely selects a different timeline.

Each semantic document owns immutable copied key/label/rule/target configuration
for its epoch. Every candidate replacement copies it forward; undo/redo rebuilds
BOTH public nonmovable Engine TerrainMaps from saved keys under that same owned
configuration. LOAD replaces the epoch and clears only its history. No history
entry borrows an old owner/configuration, event record, source snapshot or temporary
sampler. There is no mutable palette/table setter or cross-epoch replay. This
retains all reconstruction inputs without repeating a configuration inside each
entry; current live owner/configuration is baseline state, outside retained-history
byte charging. Distinct labels rendering ID 7 remain distinct on undo/redo and disk.

Literal edits now copy a complete LiteralOwner candidate, change only that
candidate, export it, and prepare a candidate timeline. Semantic edits reconstruct
a complete TerrainDocument candidate and use the ACTUAL Engine edit_batch; final
owned keys determine whether a command exists. Undo/redo first checks availability
and current content against its exact history witness, copies the candidate
history/cursor, and validates a complete replacement owner. Only then do unique
pointer and timeline swaps publish both. Swaps allocate nothing and are noexcept;
serialized update/preparation excludes observing an intermediate swap. No hardware
atomicity or concurrent writer/recycler guarantee is claimed. Construction,
conversion, rule/target, budget or allocation refusal leaves live content and both
stacks unchanged at that action boundary. Allocation/programming exceptions still
propagate; no blanket catch or no-allocation claim is added.

Any changing edit, undo, redo or LOAD replaces an owner and invalidates borrowed
document/map/terrain references. No-ops preserve the owner pointer. Copied snapshots,
prepared frame matrices/ranges/resources and other maps/modes remain independent.
The private mutable Map facade is removed; actual controls use the candidate path.
Negative tests deliberately write through the const facade to simulate invalid
out-of-band content: exact-witness/reverse-conversion refusal preserves current
values and stacks. Such alias writes remain outside the ordinary caller contract.

Alternatives: cell deltas would need extra metadata/witness policy and special
semantic reconstruction; keeping live owners per entry would retain derived state
and heavier ownership. A public command bus or generic history framework would
grow an unproven boundary. Full semantic snapshots and tiny owner composition
trade copies/latency for a reviewable prototype. Engineering confidence is high
from source and executed checks; naming/hotkeys are not personal preference
inferences. Reversal changes private app code, not Engine/schema/replay contracts.

## WINT-012 — Bounded retention, event ordering and file epochs

HistoryLimits now defaults to32 entries and1 MiB per map/mode (WINT-014;
U4 initially used256 KiB). Constructor limits
are at most 256 entries and 8 MiB; zero is permitted and rejects changing commands.
The combined undo+redo timeline is bounded, not two independently growing stacks.
HistoryState returns owned undo count, redo count and retained byte charge. Empty
undo/redo throws WorldProof::Error with Refusal::History, preserving all state.
F6/F7 are private engineering controls alongside unchanged F8 SAVE/F9 LOAD; no
public input API, held-pointer history, native key injection or new control service.

Byte charge is EXACTLY encode(before).size()+encode(after).size() for each command,
using the unchanged deterministic v1 encoder. This covers cell/null values, all
key and metadata bytes (including escaping/ordered spawns), extent and format
representation. Adjacent commands intentionally duplicate their shared snapshot.
Addition and prefix accumulation check size_t overflow; constructor caps and
storage encoding caps reject huge limits without huge allocation probes. New
history storage reserves only its admitted <=256 entries, not arbitrary caller
limits. A changed command that cannot fit BY ITSELF rejects before any live edit,
redo clear or eviction. After acceptance, the candidate discards redo and evicts
oldest undo entries until combined entry/byte caps fit. The newest command always
retains its complete undo state; the oldest remaining before-snapshot becomes the
reachable boundary. Empty-history rejection never fabricates the epoch's original
content after older entries have been evicted.

A literal equal-ID paint/empty erase is a no-op. Semantic batches still validate
ALL requests and Engine work/affected caps, including duplicate samples and
cancellations, then compare complete final keys. A valid final no-op consumes no
entry/bytes, does not clear redo, does not evict and does not replace the owner.
Changing a label with identical visuals still creates a command. Undo/redo only
moves the candidate cursor: retained entries/bytes are constant and redo does not
evict anything. New changing edits invalidate redo only after all checks succeed.

F6/F7 follows the same admitted keyboard Press filter as F8/F9. Release/Repeat,
unadmitted records and invalid/minimized sizes do nothing. A keyboard Press first
flushes the earlier semantic run, so a history action can immediately undo that
completed command. Ordered pointer observations, independent framebuffer scales,
computed edges, pan/zoom and existing batch barriers remain unchanged. Camera,
selection and brushes are excluded from document undo. Existing platform capture
leases raw Events before the tilemap-mode early return; TickContext supplies
captured sizes/records to the shared helper. No new platform/backend binding is
necessary; compile evidence still does not establish real graphical key delivery.

Successful LOAD resets only the replaced document/mode's timeline, even for
identical content; candidate owner AND empty history exist before adoption.
Failed LOAD preserves both stacks. SAVE/failed SAVE preserves all history and does
not roll back earlier authored edits. Runtime edits/undo/redo, file rename
publication and a whole input tick are separate boundaries. The actual
DemoTilemaps::update reports history errors distinctly, drops remaining tick
records and permits later updates. Earlier completed edits/runs remain. Native
controlled-directory/single-writer/no-fsync/power-loss/race limits stay unchanged.

Retained canonical byte caps are NOT exact allocator/RSS budgets. C++ object/vector
capacity overhead, current live owners/configuration, transient encodings and
old/new document/timeline copies coexist. Public snapshot field/count limits plus
entry/byte caps bound logical content; allocations can still fail. I/O/encoding,
copy and full rederivation run synchronously on serialized simulation and may
stall it. No OOM probe, async service, compact history format or persistence of
history is introduced. A fresh process reconstructs saved content with empty
histories. Default four-document history charge is now at most4 MiB (WINT-014); allowed maximum
is 32 MiB, excluding the stated overhead/transients/baseline state.

Executed evidence: 112/112 in one accumulated run, zero failed/skipped, 54.58s; all 104
predecessor names and old unit bodies remain. Seven focused history cases cover
full values/metadata, semantic oracles, repeated/cancelled samples, aliases,
redo invalidation, exact/one-short entry+byte caps, oldest eviction, file boundaries,
empty/witness/refusal preservation, admission and retained resources with zero
synthetic native counters. world.document_history adds two of the12 successful
child invocations: actual edit/undo/redo/save/reload, then fresh independent full
literal/semantic/meaning-ID/cardinal checks. Shared runner, header probes and
actual demo compile/link without warnings. Same-source symbols/compile inputs,
normal Engine imports/exports/loader and format checks pass. No CPU failure or
rerun. Exact logs/commands/receipt and remaining graphical/input/upload/teardown/
fun/leak/platform/durability gaps are in next.md. Confidence high for tested CPU
policy, low as personal taste; changing caps/hotkeys is private and reversible.

## WINT-013 — Bounded variable domains and active-mode geometry

Small actual caller (press/paint are admitted Record constructors in the process
consumer; they use each event’s own pointer):

```cpp
DemoTilemapEditor editor(existing_directory);
const std::array literal{press(Key::F9)}; // west.json may now be7x5
editor.consume(literal, {{400,200},{800,600}});
const std::array semantic{press(Key::B), press(Key::F9)}; // independent west-terrain.json
editor.consume(semantic, {{400,200},{800,600}});
// For an admitted7x5 west semantic document: last-cell center under this explicit view.
editor.set_view({0,-3072},0.125); // explicit view; no automatic layout
const std::array actions{press(Key::T), paint(29,71), press(Key::F6),
                         press(Key::F7), press(Key::F8)};
editor.consume(actions, {{400,200},{800,600}});
editor.write(frame,bindings,style);
```

The shared private admission function first runs unchanged storage value/byte
validation and adapter patch admission, then rejects axes outside[1,64], layer
count other than exactly2 or a checked division product exceeding8192 cells.
No clamp, truncation, reshaping, resize, resampling or crop. General U1
LiteralOwner/TerrainOwner conversion bounds stay65536 patch cells; only the
private editor/TerrainDocument gate narrows them. Semantic prefix/table/rule
policies stay WINT-009/010. Public Engine headers/sources, strict-v1 bytes,
replay and all source/provenance archives remain unchanged.

LOAD stages full parsed Snapshot, metadata, reversible key/meaning conversion and
both nonmovable terrain owners before unique-pointer adoption and selected history
reset. A larger valid first owner cannot publish before a failing second owner.
Success clears only selected map/mode history; failure preserves full content,
metadata, both stacks, sibling/modes and old owned frames/results. Existing
ordered Press/admission/path behavior and actual update diagnostics/drop/later-tick
policy remain. SAVE file rename is separate from runtime edits/history/adoption.

Picking now uses selected terrain extent in B and selected literal extent in V,
not the independent literal Map for both. Both terrain owners derive from the
same admitted geometry. Preparation already resolves each actual Map’s extent;
no new culling algorithm is needed. Local cells remain64x48, X-right/Y-down,
computed half-open edges through existing pick_cell. Current captured logical /
framebuffer sizes scale axes independently, full framebuffer viewport is Y-up,
with explicit pan/positive zoom. Origins stay west{48,240},east{328,208}; draw
order west ground/detail then east ground/detail, detail alpha0.5. Large footprints
may overlap or extend beyond the viewport. No packing, physical scale or gameplay
walkability promise. Different west/east and literal/terrain extents coexist.

Test-owned variable-documents.h supplies independent patterned expected keys/nulls,
metadata, literal numeric IDs, semantic meaning IDs/labels and a direct cardinal
neighbor oracle. This is expected-data sharing between CPU callers, not a copied
production conversion/selection. All cells of7x5,1x7,7x1,1x1,64x64 are checked;
three ordered spawns include each document’s last cell. The fresh process
reconstructs all20 files from literal expected patterns and the final authored
cell, comparing every field/label/meaning/visual, without using author results as
expected data. A separate rectangle oracle checks visible cell intersections,
row/column/group order and pivot placement; retained frames outlive owners.

Invalid axes0/65, wrong layer counts, invalid cell count/spawns/key byte bounds,
unknown keys and a late needed second-layer mask reject before adoption. Existing
negative cases formerly using1x6 or2x3 are updated to65x1 because their old shapes
are now valid; their registered names and every other predecessor behavior remain.
High confidence in the executed CPU boundary; exact policy/diagnostic spelling and
view choice are delegated engineering, not intentional-v3 personal preference.
Reversal is private admission/caller wiring; persisted meaning changes still need
explicit mapping rather than a schema workaround.

## WINT-014 — Checked work, retained content and output limits

Defaults become32 requests/160 affected cells/1024 work per semantic run. Each
request consumes validation, changed centers visit5 candidate positions (outside
included), and each unique affected cell consumes resolution plus at most4
sampler units. Therefore R<=32 and A<=5R<=160 bound work by R+5R+5A<=992.
This is a finite event-run allowance, not unrestricted larger strokes or a CPU-time
guarantee. Duplicates/no-ops still consume request/validation budget under Engine
rules; unchanged/cancelled final semantics produce no history entry. Explicit
caller limits remain enforced unchanged.32 separated interior edits on a full64x64
semantic board establish the exact992/160 boundary and one-short refusals.

Default history raises256 KiB to1 MiB per map/mode, keeping32 entries and hard
256-entry/8 MiB maxima. A command charges BOTH complete canonical encoded
Snapshots, including all8192 keys/nulls and ordered metadata. Single-command
one-short refusal precedes mutation; accepted changes evict oldest entries only
in the staged timeline. Default across four histories is4 MiB; hard maximum32 MiB.
No promise that every permitted rich-metadata command fits default: rejection is
explicit and preserves its complete undo contract. Full maximum patterned and
32-edit semantic commands fit; exact/one-short full-domain charges are checked.

Private geometry caps bound current content to8192 layer cells per document,
semantic labels4096 per layer, two owners each copying a two-layer base, and a
resolver vector to4096 packets. Four fully visible nonempty map/layer groups can
produce at most16384 packets. Staged Snapshots, per-layer base/labels/visuals,
previous/new owners, canonical strings and bounded timeline copies coexist.
Selection/sampling limits bound the incremental Engine batch; private full
candidate reconstruction remains O(admitted cells) before that call. Entry/byte
charges are logical, not exact allocation/RSS quotas; vector/string overhead,
current configuration and transient copies are additional. Allocation/programming
exceptions propagate. No deliberate OOM/huge allocation probe or new service.
Synchronous copy/rederivation/I/O can stall simulation; serialize writer/preparer
access and respect borrowed-view invalidation. High engineering confidence in
finite accounting, low as taste evidence; private caps can be reversed without
Engine ABI or storage-format changes. Actual counts/logs and limits stay in next.md.

Executed U5 evidence: one accumulated119/119 Debug ASan/UBSan invocation, zero
failed/skipped,89.52s, retaining all112 names plus6 new unit cases and1 process
scenario. All14 process child exits0. The new author and fresh process each check
20 files/33168 layer cells, complete keys/nulls/metadata, literal numeric values,
semantic meaning IDs/labels and direct cardinal-derived visuals. Process file
hashes remain unchanged during read-only verification. Unit checks establish
actual active-domain edge picking, large-domain full-value edits/undo/redo,
32/160/992 and one-short refusals, history charge boundaries, late second-owner
preservation, independent rectangle culling/pivot/pass order, old frame ownership
and zero synthetic bind/draw/upload counters. Actual DemoTilemaps::update diagnoses
axis refusal, drops remaining current tick and successfully loads/saves next tick.

Shared runner/header probes and actual demo compile/link without warnings.
The first successful build was followed by one source-review fixture correction /
incremental unit rebuild BEFORE any test execution; there is no test failure or
CPU rerun. Saved nm/readelf/ldd/compile/link checks establish same adapter/editor
sources in demo/unit/world,14 actual Engine imports/exports, one normal shared
Engine per executable and unchanged import/sanitizer settings.375 Engine/public /
replay/archive/unchanged-suite hashes and14 earlier committed receipt files are
unchanged. Format check passes; standalone unaffected storage/procedural suites
are not repeated. Exact commands, immediate exits, full logs and ready receipt
are in next.md. CPU/link proof leaves real input/display, atlas decode/upload,
visual correctness/fun, graphical cleanup, leaks, installed/static/Release/other
platform ABI and file durability gaps open. No unchanged X11 retry, fsync,
power-loss/race guarantee or further assignment is implied.

## WINT-015 — In-memory cave recipe, semantic base and witness

Small actual application caller (press/paint construct admitted Records in the
existing world process caller; no new keyboard binding is implied):

```cpp
DemoTilemapEditor editor(existing_directory);
const std::array mode{press(Key::B)};
editor.consume(mode, {{400,200},{800,600}});
editor.install_generated(recipe, WorldProof::GeneratedReplace::DiscardCurrent);
editor.set_view({0,-3072},0.125);
const std::array brush{press(Key::Digit6),paint(13,67),paint(13,67)};
editor.consume(brush, {{400,200},{800,600}}); //31x19 west ground cell(2,2), coalesced
const auto retained = editor.generated_state(0); // owned deep observation
editor.regenerate_selected();
const std::array file{press(Key::F8)};
editor.consume(file, {{400,200},{800,600}});
```

Private generated-document.* owns CaveRecipe: family cave, actual CaveReplay
Domain/algorithm/version1, x-fast coordinates, seed/stage/stream, integer fill,
steps, birth/survival masks, connectivity and requested budgets. Effective options
are copied from the actual Cave. Requested options equal effective in v1.
Budgets above actual hard ceilings are refused rather than silently normalized;
successful budget changes within those ceilings change admission/provenance, not
content. Recipe inputs other than budgets must remain exactly equal under
PreserveEdits, even if a new seed/config coincidentally produces identical output.
No automatic rebase/migration is selected. Unsupported algorithm/version/family /
adapter version/axes refuse with owning diagnostics. This proves current compiled
integer-v1 replay, not indefinite generator/platform/version support.

Version1 CaveAdapter owns X-right/Y-down convention, map ID, two ordered names,
all ordered cell spawns and explicit occupancy0 floor/1 solid -> key/label pairs.
Default pairs terrain:one/label1 and terrain:two/label2 remain unrelated to numeric
meaning IDs101/203, derived IDs7..10 and atlas targets0..3. Shared private meaning
tables in world-document.* remove the former constructor-local duplicate policy;
all four keys/labels remain exactly compatible. No asset/game meaning enters
Engine. Optional initial_detail owns exactly C semantic keys/nulls, or absent
means all null. It belongs to the versioned BASE adapter input, not player edits.
This narrow addition supports authored base markers and a real late second-owner
refusal; every key is validated, no callbacks/spans are retained. Changed adapter
inputs require explicit discard, with no color/visual reverse inference.

GeneratedDocument owns the complete accepted recipe/effective config/limits,
full semantic base Snapshot and public CellPatches::Patch. Its witness is the
actual full canonical base bytes from diff/capture; no hash, forged witness,
opaque ID or seed substitutes for equality. On each completed semantic run,
export the complete final Snapshot, then actual diff counts unique changed
addresses in authored-layer/row order. Repeated input samples coalesce through
Engine last-write-wins and this final diff, not duplicate-address patch records.
Actual apply reconstructs/checks the complete final target. Metadata (ID, extent,
ordered layer names and all spawns/order) stays exact; non-cell change refuses.
Both label and key domains are checked before adoption. Default128 changed
addresses may be lower than storage’s hard1024; never truncate a stroke.

Replay validates the retained base against capture and applies its saved patch
as a full valid semantic snapshot, runs the actual retained supported CaveReplay,
builds the explicit semantic base and compares BOTH complete base values and full
canonical witnesses BEFORE applying saved edits. A same-sized altered cell with
an old witness refuses; even a newly captured legitimate witness for a modified
base cannot match regenerated original content. Owned observations/copies cannot
mutate live attachments. Input descriptor/table/detail destruction is safe.
Allocation/programming exceptions propagate normally; only anticipated generator,
conversion/storage/patch refusals are diagnosed. No persisted recipe/patch format,
v1 property, digest/crypto, migration or universal World framework is introduced.

Alternatives rejected: seed-only identity misses options/adapter/metadata; raw
brush patches contain duplicate addresses; deriving labels from colors loses
aliases; a replay result without a full witness cannot establish exact retention.
High engineering confidence in native tested CPU behavior, low as personal API
preference. The generation entry and cap spellings are reversible private policy;
persisting recipes later needs selected disposable integration and a versioning
policy under coordination/generation-storage.md.

## WINT-016 — Three-part publication and independent file fallback

DemoTilemapEditor retains an optional independent generated attachment for each
semantic west/east document. Generation requires explicit B mode; it never changes
mode/selection/view. DiscardCurrent prepares complete generated state and BOTH
nonmovable terrain owners, then a new empty selected history before swapping.
It deliberately replaces authored content only at the caller’s explicit request.
PreserveEdits requires an attached epoch and its unchanged limits; all caller
inputs except admission budgets remain exact. Changing limits requires discard.
regenerate_selected uses the owned recipe/limits, not a live sampler.

Every attached semantic edit and undo/redo prepares a complete candidate document,
checks the current full content against the attachment, and prepares a complete
updated diff/accounting BEFORE any content/history/attachment swap. History
admission follows overlay admission. A later history failure therefore cannot
silently publish a valid overlay while losing its undo contract. All final swaps
are nonallocating/noexcept. No-ops validate full requests/overlay but publish
nothing, preserving both stacks/current owner. Rejected commands preserve every
cell/metadata field, recipe/base/witness/patch, history and sibling/mode values.
New accepted changes alone clear redo. Successful replay reconstructs/validates
both owners; when final semantics equal current content, it discards the candidate
and preserves existing owners/history/borrowed views. Only explicit admitted
budget-only provenance changes may replace the attachment alone.

This is serialized application update/preparation, excluding concurrent writes,
frame recycling and reads through invalidated borrowed views. Changing edit /
undo/redo/discard/LOAD invalidates borrowed document views; no-op replay does not.
Prepared packets independently own matrices/selections/resources and survive
later adoption/owner destruction. Selection/camera/brush are not document history.
Per-event pointers, admission/Press filtering, pan/zoom/HiDPI, dimensions/culling
and caller-authored pass order remain U5 contracts. No new input UI is required.
The private DemoTilemaps forwarding entry enables deliberate setup in the same
real helper; normal demo modes and startup paths remain compatible.

SAVE still encodes only complete strict-v1 Snapshot meaning, preserving attachment
and history. It is not a recipe publication. Failed SAVE retains predecessor under
the existing profile and does not undo earlier runtime edits. Successful ordinary
semantic LOAD has no recipe information and explicitly detaches only its selected
map/mode attachment, resetting that history. Failed LOAD preserves all generated
state/history. Literal LOAD never detaches semantic state. Fresh-process LOAD
reconstructs all saved meanings/visuals with no attachment/history and regeneration
refuses until explicit installation. Actual DemoTilemaps::update now diagnoses
expected generation/patch refusals, drops remaining tick records and permits a
later valid tick, while earlier completed actions remain. No full-tick transaction
or implicit migration/fallback to another mode.

High engineering confidence for tested serial ownership. Costs are repeated full
bounded copies and synchronous replay/I/O; no async service, editor command bus or
public camera/input/tilemap/Engine API change. File rename publication, runtime
adoption/history and full input ticks remain separate boundaries.

## WINT-017 — Finite accounting and the actual producer boundary

Existing editor axes1..64, exactly two layers, <=8192 layer cells, storage8 MiB
encoding/input/256 spawns/128-byte keys, patch65536 base cells/1024 addresses /
8 MiB witness and history32 entries/1 MiB (hard256/8 MiB) remain unchanged.
The private recipe accepts at most64 KiB fixed-plus-owned-field payload; adapter
text/spawns/detail sizes validate before copying. Default GeneratedLimits admits
128 final changes,1 MiB witness,4 MiB recipe/base/witness/edit payload and1 Mi
adapter work. Hard private ceilings1024 changes/8 MiB witness/32 MiB retained /
4 Mi work refuse oversized settings rather than clamp. Zero budgets admit only
operations actually fitting them; a zero edit cap allows validated no-ops.

Adapter work is a conservative reservation, not instruction/time measurement:
64 full-layer-cell scan passes plus32 full-layer-cell copy passes,8 units per
admitted overlay-address allowance and one per adapter spawn. Work =96*L +8*cap.edits
+spawns where L=2*C. Store/report the reservation; one-short refuses before replay /
copy staging. It covers the private cell-pass/candidate-copy allowance, separately
from real CaveBudgets and Engine’s per-input-run32/160/1024 incremental selection
work. Lower-level codec/table/selection operations retain their own fixed bounds;
no exact machine-work/latency claim. Cave peak payload uses the actual requirements
(18*C +4096 bytes), with actual generator work reported from usage. No unbounded
iteration or expensive stress/OOM probe.

GeneratedBytes.recipe counts sizeof(CaveRecipe), sizeof limits/effective config,
string contents, spawn object/contents and optional-detail object/contents at size,
not vector capacity. Base and witness are separately charged canonical bytes.
Edits count layer bytes +two u32 coordinates+presence byte+optional key bytes.
Retained is recipe+base+witness+edits only; full current encoded Snapshot and history
canonical charge are reported separately. This MIXED logical accounting is neither
RSS nor a wire format. C++ container/control/capacity overhead beyond the stated
recipe charge, TerrainOwner metadata/config copies, current maps/labels and
transient encodings are additional. Reconstruction can simultaneously hold live
and candidate attachments, full before/after/base Snapshots, two replacement terrain
owners and old/new timelines. A conservative declared32 full canonical snapshot
copy allowance is32*max(base,full_snapshot) bytes, plus two recipe copies and two
witness strings/edits; history copying adds its charge, and Cave requirements are
separate. Current two-owner numeric payload alone is24*C bytes (two two-layer
TileId maps plus two TerrainLabel grids). These are explicit logical reservations,
not measured allocator peaks. The retained result may exceed a full snapshot.

The original maze.cpp/random.cpp/cave.cpp compile through one private application
world-cave-private target per configuration, shared by unit/world/demo wiring.
Only that private archive contains generator sources; storage/tilemap definitions
remain solely in normal shared Cheryl::Engine. generated-document.cpp and the
same world-document/editor sources compile in all three callers. No standalone
PG add_subdirectory, copied algorithm, source archive substitute or sibling source
is used. Frozen31x19 TEST occupancy rows come from an independent Python integer
reference retained with ignored logs, never linked or used in production. All
keys/metadata/labels/visuals compare against literal rows and direct cardinal
oracles; seed42 vs43 with identical remaining inputs must differ. A third stage /
configuration is declared separately. Broad standalone PG/storage suites remain
unchanged and are not repeated. High confidence for exercised source/link bounds;
exact executed counts/logs, measured charges and material limits are in next.md.

Executed U6 evidence: initial accumulated127/127 (all119 predecessor names plus
7 new unit cases/1 world scenario),0 failed/skipped,110.94s. Review adds1/1 LOAD
isolation recheck,1.06s, then corrects the conservative reservation and reruns ALL8
new names,8/8,8.84s. There is no second full127 invocation. All18 world child exits0
(initial16 +final2), including final fresh full-snapshot verification of1178 cells /
all metadata with no restored recipe. A first build failed on two consumer compile
errors; the complete diagnostic/exit1 is preserved and corrected builds retain
Werror. Seven unchanged unrelated dangling-else warnings remain, no cleanup.

Final source/header/link proof passes with17 actual normal Engine imports/exports,
one original private cave archive per configuration and SAME private app sources
in all three consumers. Actual demo compiles/links after final correction, no run.
Format check passes;375 preserved source/archive hashes and15 earlier receipt
hashes match, all119 old names remain. Final source hashes and XML/registration /
child correspondence are saved. New fixture recipe/base/witness/edits/history
charges964/12802/12802/94/51202 bytes, attachment26662 versus full Snapshot12812;
current numeric terrain payload14136 and32-snapshot reconstruction allowance409984
are separate. Final reserved adapter work114115; actual Cave payload14698/work14643.
These mixed logical categories are not heap/RSS or compactness claims. Exact
commands/exits/rechecks/limits and standalone receipt are in next.md. CPU evidence
leaves real graphics/input/assets/teardown/fun, leaks, packaging/platform/ABI,
indefinite generator replay and stronger filesystem durability unverified. No
unchanged display retry, persisted recipe or subsequent job is selected.
