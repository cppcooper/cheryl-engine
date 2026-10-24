# WORLD-U2 literal editor document handoff

Status: implemented, demo linked and focused CPU checks passed; coordinator review
pending. Exact clean starting HEAD/parent on feat/tilemap is
`e9289fcb992feddf85deb942a560562a54971f78`. WORLD-U1 was independently accepted
and already promoted by the coordinator. No new merge/fetch/push or worker1 edit
occurs. This assignment is outside the completed finite queue; stop after this
checkpoint. The [receipt](integration/WORLD-U2.json) reports execution, not automatic
source review, graphical acceptance or promotion.

WINT-007/008 extend the matching [compact](decisions.md) and [detailed](agent-notes.md)
decisions. Existing archived feature records/provenance, WORLD-U1 receipt/logs,
public CPU headers/sources, strict v1 schema and generator replay remain unchanged.
Source preservation checks 375 production/header/archive/unchanged-suite paths;
all previous unit consumer bodies remain an unchanged prefix. The relocated
adapter header is byte-identical to WORLD-U1, and its source differs only in its
include spelling. Standalone storage/procedural suites are unaffected and not rerun.

## Small real caller and controls

```cpp
#include "tilemap-editor.h" // private application code
DemoTilemapEditor editor(document_dir); // default empty disables actions
editor.consume(events, {{400,200},{800,600}}); // paint, F8 Press, change, F9 Press
editor.write(frame, bindings, style);
```

Real option flow is --tilemap-document-dir PATH -> DemoOptions -> main ->
DemoGame/GameState/GameImpl -> DemoTilemaps -> this editor. Only --tilemap-display
constructs the fixture. No startup load/save, auto-save, directory creation, file
browser or public Engine/startup/input API is added. The supplied directory must
already exist when an action runs; fixed names are west.json/east.json. Empty
default, missing/not-directory path and terrain mode produce explicit diagnostic
refusals. Relative paths require stable cwd; native component/symlink-ancestor
meaning and the caller-controlled single-writer directory profile remain.

Admitted keyboard Press F8 SAVE / F9 LOAD operates on the selected literal
DOCUMENT in full, regardless of selected layer. Release/Repeat/unadmitted records
and invalid/minimized dimensions ignore actions. Existing1/2 map, G/T layer,
3–6 palette, captured per-event pointers, independent framebuffer scales, computed
edges, pan/zoom/reset and B/V mode behavior stay compatible. Terrain actions refuse
before file access and never serialize derived visuals. The existing keyboard
barrier still flushes earlier semantic work; no whole-tick rollback is promised.

## Ownership, mapping and publication

[world-document.h/cpp](../projects/apps/demo/src/world-document.h) is the actual
WORLD-U1 bridge moved into private demo source. Its declarations/algorithms are
unchanged. The SAME adapter and editor source compile into actual demo, unit cases
and world consumer. No copied conversion, alternate Engine object archive or
public promotion is used. WorldProof naming stays to avoid gratuitous renaming.

Only two literal Map owners become unique_ptr LiteralOwner documents. map(index)
retains the old read facade; a private mutable accessor feeds paint/erase. The
new const document(index) view exposes owned snapshots/metadata. LOAD invalidates
borrowed old document/Map references; owned copies and prepared frames remain
independent. Four terrain owners, labels, brushes and batches stay separate.

Explicit diagnostic keys brown/black/grey/red in namespace tilemap map to logical
IDs 7/8/9/10. Zero/null is empty; atlas targets remain 0/1/2/3, so nonzero7 draws
original target 0. Original literal cells/rendering are retained. Seed documents
have map IDs tilemap:west/east, ordered ground/detail layers and ordered entry/
marker spawns. LOAD retains arbitrary valid map ID, ordered two layer names and
all ordered spawns, rather than deriving them from Map or sorting them.

F9 first uses checked storage load, then requires exactly 3x2/two layers BEFORE
numeric conversion. Malformed/unknown keys and other valid shapes refuse; complete
candidate creation precedes noexcept pointer swap. Failed load preserves prior
cells/metadata, sibling document, selection/mode/brush/view and held results/frames.
F8 exports complete owned semantic keys and metadata via LiteralOwner::snapshot,
then calls checked storage save. Unknown reverse IDs refuse without replacing
file or prior result. Prior authored runtime edits remain; there is no combined
runtime/file transaction. A final-symlink native refusal preserves predecessor
bytes and the link. No fsync/power-loss, hostile-race, multiwriter or distributed
transaction guarantee is added.

The real DemoTilemaps::update catches anticipated Maps::Error, WorldProof::Error
and Engine bad_request, logs context, drops the rest of that tick and continues
on later updates. Earlier successful events remain committed. Its catch branches
are exercised with synthetic CPU records WITHOUT atlas loading/startup; actual
graphical input is still unverified. Allocation/programming/unexpected exceptions
propagate through existing runtime handling; no blanket exception conversion.

LOAD can temporarily own a full storage-bounded input before geometry refusal
(8 MiB document,262144 cells/16 layers/256 spawns/128-byte keys). Adopted documents
have 12 cells plus bounded metadata; bridge admission/export validation remains.
These are logical bounds, not exact allocator/RSS quotas; no OOM probes occur.
I/O is synchronous on the serialized simulation update owner and may stall it.
Platform constructs/loads assets/captures before simulation and destroys state
after frames complete. No borrowed event spans, platform reads, camera service,
async file queue or retained temporary mappings are introduced.

## Focused executed evidence

Six new tilemap_consumer cases exercise the exact app helper:

- editor_documents: custom ID/layer names/spawns, selected layer paint, full
  edit/save/change/load, selected west/east independence, old frames and metadata.
- editor_document_filters: Press-only, gameplay admission, minimized suspension,
  default-disabled/missing directory and terrain mode refusals; no automatic I/O.
- editor_document_load_failures: valid prior setup, missing/malformed/unknown key,
  two different incompatible valid shapes; pointer/full values/selection/old
  prepared frame stay intact. No truncation/resizing occurs.
- editor_document_save_failures: deliberately inject an arbitrary unsupported ID
  into the actually mutable Map through its read facade (negative fixture only),
  then prove reverse refusal preserves predecessor and current runtime. Separate
  native final-symlink refusal leaves exact regular predecessor/link intact.
- editor_document_frames: replacement and owner/provider destruction preserve
  copied matrices/ranges/resources; weak handles expire at last frame/vector.
- editor_document_diagnostics: execute real update catches for storage/conversion/
  mode errors; remaining-tick selection/save is dropped, earlier paint persists
  and a later update saves successfully. No atlas/native startup is invoked.

Synthetic bind/draw/upload counters remain zero where resources are used.
world.editor_documents adds an actual editor author and FRESH verifier process.
Both west/east complete expected snapshots are independent literals; verifier
checks every metadata/key field and 24 numeric cells through actual F9. Exact file
bytes remain unchanged by verification. The existing world scenarios remain.

## Actual commands, logs and outcomes

All configure/build/native/format/symbol checks use the shared scripts/build
wrapper, one job and existing adaptive/no-swap/admission policy. No resource
refusal, unchanged retry, installs/downloads/pin changes, sibling/cache access or
protected work occurs. Preserve both existing Debug configurations and outputs.

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug \
  --target tests-engine tests-terrain-consumer map_storage_consumer map_storage_header_probes tests-world-consumer --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u4-display-debug --target demo --parallel 1
/mnt/projects/cheryl-features/scripts/build -- python3 build/world-u2-logs/check-ownership.py
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug --show-only=json-v1 \
  -R '^(tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.|tests-terrain-consumer\.|map_storage\.consumer\.|world\.)'
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error --verbose --parallel 1 \
  --output-junit /mnt/projects/cheryl-features/projects/tilemap/build/world-u2-logs/results.xml \
  -R '^(tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.|tests-terrain-consumer\.|map_storage\.consumer\.|world\.)'
```

| Operation | Immediate exit / actual result |
| --- | --- |
| Initial shared build |1: new editor header expansion exposed pinned GLM sign-conversion diagnostics under the stricter world caller |
| Corrected shared build |0: links; only existing GLM headers are SYSTEM in that private target, app Werror/conversion flags and Engine requirements stay intact |
| Demo incremental build |0: actual option/game/editor/document wiring compiled/linked; no warning |
| Ownership/source inspection |0: same private source in three callers; single actual shared Engine; public/header probes built |
| Discovery |0:97 unique names, exactly all 90 old names plus seven new checks |
| Accumulated execution ONCE |0:97/97 passed, zero failures/skips,52.83s |
| Format and final dry-run |0 each; final GameImpl changed ranges also pass after unrelated formatting is restored |
| Final scoped demo rebuild |0: unrelated GameImpl formatting restored; tested helper/adapter source unchanged |

Full commands, immediate .exit files, logs/XML/LastTest, discovery correspondence,
private source hashes/compile inputs, nm/readelf/ldd/link outputs and source
preservation are saved under ignored build/world-u2-logs/. commands.json/sh records
every actual wrapped operation, including failed/corrected compilation. World
process paths/argv/stdout/exits are indexed by world-processes.json: eight child
invocations exit 0, including the two new editor author/fresh-verifier children.
Unit scratch files live only under this clone's build/world-u2-checks and are kept
as evidence. No CPU test failed or reran. Final staged review restores unrelated GameImpl
formatting only; demo rebuild and changed-range format check both exit 0. No
behavior change follows the green run, and tested helper/adapter bytes match.

Actual symbol/load checks find all 14 prior Engine export/import operations, normal
consumer imports without static/producer macro patches, one Engine storage producer
per source, and exactly one clone-local Engine DSO in each demo/unit/world binary.
Both source databases compile the same adapter/editor bytes; the world link has
only a generator-specific archive, no alternate Engine implementation archive.
Header-first probes remain normal Engine usage. Source-tree GNU/Linux Debug/shared
is the tested ABI boundary; SDK/static/Release/other platforms remain unverified.

ASan/UBSan and halt_on_error remain enabled without diagnostics. Invocation-local
detect_leaks=0 is solely for the recorded tracing limitation; no global flags or
build selections change. Leaks remain unverified. Standalone storage/procedural
suites are unchanged and deliberately not repeated in this unit.

## Limits and next action

No unresolved U2 CPU/build prerequisite remains. Ready receipt covers these
executed checks only. Real display/input, atlas decode/upload, capture, graphical
teardown, visual correctness/fun and leaks remain open; X11 :1 is not retried.
No new broad editor/scene framework, semantic persistence, compact retention,
file recipe/schema, storage migration, undo, resize/chunks or public API is selected.
Native filesystem durability/race/multiwriter limits and wider ABI gaps remain.

Next action: coordinator reviews this exact checkpoint/receipt/logs and publisher
backup after the run drains, then chooses any future merge with worker1 geography.
Worker2 remains idle. No automatic promotion, sibling integration or later unit
starts; the publisher alone transports assigned feat/tilemap checkpoints.
