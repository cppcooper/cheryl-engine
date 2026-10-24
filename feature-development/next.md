# WORLD-U3 semantic editor document handoff

Status: implemented and verified; ready for coordinator source review. Exact clean
starting HEAD/parent is `8522b2976013c10c23266b6413e3d46a408b1251` on feat/tilemap.
Its WORLD-U2 receipt, nonempty saved logs and 97 passing XML entries were inspected
before dependent work. No geography fetch/merge, protected/sibling work or automatic
promotion occurs. This is the first map milestone of feature-round-20261024-0500;
the controller alone selects a later exact task after the successful run drains.
The [receipt](queue/feature-round-20261024-0500/WORLD-U3.json) reports execution,
not coordinator source acceptance or proof of an unexecuted platform.

WINT-009/010 in [compact](decisions.md) and [detailed](agent-notes.md) views match.
WINT-008 now selects files by mode; its historical U2 terrain refusal is explicitly
superseded. All historical archives/provenance, U1/U2 receipts and 375 inspected
public Engine/production/replay/archive/unchanged-suite paths remain unchanged.
Standalone storage/procedural suites are unaffected and not repeated.

## Small real caller and controls

```cpp
#include "tilemap-editor.h" // private application owner, SAME source in all callers
DemoTilemapEditor editor(existing_directory);
// press()/paint() are actual admitted-record factories in world-consumer/main.cpp.
const std::array records{press(Key::B), press(Key::G), paint(40,128),
                         press(Key::T), paint(72,144), press(Key::F8)};
editor.consume(records, {{400,200},{800,600}});
// A later update changes a cell, then F9 reconstructs BOTH saved layers.
editor.write(frame, bindings, style);
```

F8 SAVE / F9 LOAD uses the selected mode and map, regardless of selected layer:

| Mode | West / east files | Meaning |
| --- | --- | --- |
| V literal | west.json / east.json | Existing color keys -> numeric IDs 7..10 |
| B semantic | west-terrain.json / east-terrain.json | terrain:one/two/three/four -> labels 1/2/3/4 -> numeric meaning IDs 101/203/307/409 |

Zero/null is empty. Semantic IDs are separate from visual IDs 7..10 and atlas
targets 0..3. Existing cardinal N/E/S/W bits 1/2/4/8 -> target mask%4 -> visual IDs
remain diagnostic application policy. Distinct isolated labels 1 and 2 both render
ID 7 but persist different keys. No rendered-ID reverse lookup reconstructs meaning.

Existing option --tilemap-document-dir PATH flows unchanged into the editor.
The directory must already exist; empty disables both modes. No startup/automatic
I/O, directory creation or path browsing. Only admitted keyboard Press acts;
release/repeat, unadmitted events and minimized/invalid sizes ignore actions.
Captured per-event pointers, independent framebuffer scales, computed edges,
pan/positive zoom/reset, selected map/layer and separate brushes remain unchanged.
Demo console and option help now describe both modes.

## Ownership, compatibility and failure boundaries

Private TerrainDocument in world-document.* composes two existing TerrainOwners,
each containing the actual public nonmovable Engine TerrainMap. It also copies
reconstruction tables/rule/targets. Stage layer 0 AND layer 1, retained ID/ordered
names/all ordered spawns and configuration before a single selected pointer swap.
Late second-layer rule/target/conversion refusal destroys only the candidate.
No first-layer or metadata adoption occurs. Export combines each selected semantic
layer; complementary construction-time numeric layers inside each owner are not
live sibling visuals. The existing terrain(index,layer) read facade stays useful.
Literal owners/layouts/files remain separate, including while switching B/V.

Semantic table keys and labels must be nonzero/unambiguous and known in the
reversible key/meaning-ID table; null is empty. Unknown labels reject before Engine
batch publication, even if a later request overwrites them. Missing rule cases or
unmapped targets may remain unused on empty geometry but reject when required.
Both private table kinds are capped at256 entries. No references to borrowed
snapshots, tables, event spans, samplers or rule/mapping inputs survive calls.
Future reconstruction uses owned copies; input destruction is exercised.

Semantic documents require a terrain: map-ID prefix and exactly 3x2/two ordered
layers. Literal editor loads reject that prefix; semantic loads reject absent
prefix or unknown keys. This PRIVATE ID convention distinguishes all-null kinds
without changing strict v1 bytes/properties. It narrows formerly arbitrary literal
editor IDs only for that reserved prefix; public storage/LiteralOwner contracts
stay unchanged. Rule/mapping is not serialized: caller semantic-table consistency
across sessions is required; an active visual palette can rederive appearances.
No recipe, fingerprint, migration or public World API is added. Names/prefix/keys
are reversible engineering choices, not evidence of personal API taste.

F8 exports complete owned semantic keys/metadata and calls actual checked save.
F9 owns the full checked load before fixed-shape/kind/conversion checks, builds a
complete candidate, then swaps. Failed load preserves prior full semantic/visual
cells/metadata, sibling and literal documents, selection/mode/view/brushes, held
results and old frames. Borrowed document/terrain views expire on replacement;
owned values and prepared packets retain copied matrices/ranges/resources.

The existing keyboard barrier flushes earlier terrain edits before a file action.
Anticipated bad_request/storage/conversion failures are logged by the actual
DemoTilemaps::update and drop the rest of that tick; later updates continue.
Earlier completed edits stay authored. A failed save preserves the predecessor
under the native profile but does not roll back runtime edits. Runtime adoption
and file rename publication are separate boundaries; no whole-tick/file transaction.

I/O is synchronous on serialized simulation update/preparation and may stall it.
Platform constructs state before simulation and destroys it after completed frames;
concurrent writing/preparation/recycling is excluded. Logical storage bounds remain
8 MiB, 262144 cells/16 layers/256 spawns/128-byte keys; adapter patch bound 65536 cells.
Fixed adopted documents have 12 cells, copied metadata/tables and two Maps/labels.
LOAD can temporarily own full storage input plus old/new candidates; composition
adds configuration/metadata copies. Actual editor has four targets/16 mask cases;
underlying private callers size their supplied vectors. Batch caps remain 32
requests/6 affected/256 work. These are logical bounds, not exact allocator/RSS
quotas. Allocation/programming exceptions propagate; no OOM probing occurs.

## Executed checks and saved evidence

One accumulated shared selection passed **104/104**, zero failed/skipped, 54.78s,
retaining every one of the 97 prior names plus six unit cases and one process case:

- semantic_documents: custom ordered metadata, aliased labels, two-layer edits /
  SAVE/change/LOAD, separate east/west and literal files, full cardinal oracle.
- semantic_load_refusals: missing/malformed/unknown/wrong-key/kind/shape/layer-count
  inputs, including null-only cross-mode files, preserve full owner/state/frames.
- semantic_atomic_layers: establish valid first-owner reconstruction, then fail
  ONLY second-owner missing rule/out-of-range/zero target; no partial adoption.
- semantic_tables: unknown/empty/duplicate/zero mappings and overwritten unknown
  labels, invalid layer, full owned results and borrowed-input destruction.
- semantic_actions: actual Press/admission/minimized policy, disabled/no-directory,
  failed-save regular predecessor/final-symlink preservation, actual update catch
  and rest-of-tick drop with a later successful SAVE of the preceding terrain edit.
- semantic_document_frames: resources, matrices/ranges/order survive complete
  document replacement and owner destruction, then weak handles release with the
  last frame/vector. Synthetic bind/draw/upload counters remain zero.
- world.semantic_documents: actual event author paints both layers of two documents,
  saves/changes/loads, then FRESH verifier checks complete literal snapshot fields,
  all 24 labels, all 24 numeric meaning IDs and all 24 derived visuals against literal
  arrays/an independent cardinal oracle. Literal-file bytes and both semantic
  files stay unchanged by verification.

All five world scenarios execute ten successful child invocations (including two
new semantic children). Their exact commands/stdout/stderr/immediate exits and
owned fixture paths are indexed/copied under build/world-u3-logs/world-processes.json.
No CPU case failed or reran. U2's accepted names editor_document_filters and
editor_document_diagnostics remain registered; their old terrain-refusal assertions
now check separate semantic I/O and missing-file diagnostics.

All configure/build/native/format/symbol commands use the shared wrapper, one job,
existing Debug/shared configurations and adaptive/no-swap/admission policy. No new
configuration/dependency, download/install, pin/global flag change or resource
refusal occurred. Actual commands and immediate exits are in commands.jsonl:

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug \
  --target tests-engine tests-terrain-consumer map_storage_consumer map_storage_header_probes tests-world-consumer --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u4-display-debug --target demo --parallel 1
# discover.py saves exact ctest --show-only=json-v1 output and checks prior names.
/mnt/projects/cheryl-features/scripts/build -- python3 build/world-u3-logs/discover.py
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error --verbose --parallel 1 \
  --output-junit /mnt/projects/cheryl-features/projects/tilemap/build/world-u3-logs/results.xml \
  -R '^(tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.|tests-terrain-consumer\.|map_storage\.consumer\.|world\.)'
/mnt/projects/cheryl-features/scripts/build -- python3 build/world-u3-logs/check-ownership.py
/mnt/projects/cheryl-features/scripts/build -- python3 build/world-u3-logs/check-final.py
```

| Operation | Immediate exit / actual result |
| --- | --- |
| Initial shared build |0; one new test range-loop copy warning; corrected to const reference before execution |
| Final shared build |0; no warnings, empty-table negative coverage added before sole accumulated run |
| Initial demo build |0; actual adapter/editor/demo wiring compiles and links |
| Discovery |0; 104 unique entries, exactly all 97 old plus seven new |
| Accumulated tests ONCE |0; 104 passed, 0 failed/skipped, 54.78s |
| Ownership |0; same actual adapter/editor bytes compiled in demo/unit/world; normal shared Engine producer/imports/loader |
| Scoped format checks |0; all touched C++ files conform |
| Final demo build/checks |0; mode-specific option-help text corrected after review; relink + final loader/format pass, tested helper bytes unchanged |

Source/compile/symbol/loader inspection establishes the SAME world-document.cpp and
tilemap-editor.cpp in all three callers, private TerrainDocument symbols in each,
14 prior Engine export/import operations, exactly one normal Engine storage producer
per source and exactly one clone-local Engine DSO per binary. World link retains
only actual private cave sources in its generator archive, no alternate Engine
implementation archive. Existing Engine-linked public header probes compile.
Final help-only change needs no CPU rerun; adapter/editor bytes match tested hashes.
Full logs/XML/LastTest, discovery correspondence, source hashes, commands and exits
remain under ignored build/world-u3-logs/. Scratch files are clone-local only and
preserved. ASan/UBSan/halt_on_error remain enabled without diagnostics; only
command-local detect_leaks=0 accommodates the recorded tracing limitation.

## Limits and next action

No unresolved U3 CPU/build prerequisite remains. Ready is an execution report.
Real display/input, atlas decoding/upload, capture, graphical teardown, visual
correctness/fun and leaks remain unverified. No unchanged X11 :1 retry. GNU/Linux
Debug/shared source-tree linking is checked; installed SDK/static/Release/other
platform/toolchain ABI and native file durability remain open. Controlled single
writer/cwd/directory assumptions, no-fsync/power-loss/hostile-race/multiwriter and
synchronous allocation/latency limits remain. No history/undo, variable geometry,
new schema, procedural work or public API promotion is included.

Next action: coordinator reviews this exact checkpoint and passing batch receipt /
saved logs after the run drains. The finite controller may then select the next
exact assigned milestone under its own gates. Worker2 stays idle; no later unit,
background scheduling, sibling integration or branch transport starts here.
