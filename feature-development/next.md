# WORLD-U6 owned generated base plus semantic edits

Standalone continuation on exact clean reviewed parent
`9cfa376a6366012fff2e1399af2c5d87acfa471c`, feat/tilemap. The committed U5 receipt
was required as old batch feature-round-20261024-0500 / WORLD-U5, ready/true; all9
nonempty evidence paths and119 successful XML entries were inspected before work.
This is NOT a fourth batch unit or a new queue. Older plans/receipts/provenance
archives and worker1 climate remain untouched. Current matching decisions are
WINT-015/016/017 in [compact](decisions.md) and [detailed](agent-notes.md) records.
Implementation and selected CPU acceptance are verified; ready for coordinator
review. The standalone [receipt](continuation/WORLD-U6.json) reports executed behavior,
not automatic coordinator or graphical/platform acceptance.

## Actual private caller

```cpp
DemoTilemapEditor editor(existing_directory);
const std::array mode{press(Key::B)};
editor.consume(mode, {{400,200},{800,600}});
editor.install_generated(recipe, WorldProof::GeneratedReplace::DiscardCurrent);
editor.set_view({0,-3072},0.125);
const std::array brush{press(Key::Digit6),paint(13,67),paint(13,67)};
editor.consume(brush, {{400,200},{800,600}}); // repeated31x19 west cell(2,2)
const auto retained = editor.generated_state(0); // deep owned value
editor.regenerate_selected();                  // retain current final semantic edits
const std::array save{press(Key::F8)};
editor.consume(save, {{400,200},{800,600}});
```

This is the SAME private DemoTilemapEditor/generated-document/world-document code
compiled in actual demo, CPU unit and world process callers. press/paint build
actual admitted ordered records. Existing F8/F9, F6/F7, B/V, map/layer/brush, pan /
zoom controls are unchanged; no new keyboard generation UI. DemoTilemaps exposes a
narrow forwarding installation entry for deliberate setup; no startup generation,
automatic replacement or new native loop. Generation requires explicit B mode and
caller policy. Existing logical-pointer/independent framebuffer scales/Y conversion /
computed half-open picks, active document extent/culling and authored pass order
remain compatible. No new camera/input/backend/public World API.

## Owned contracts and refusal boundaries

- CaveRecipe owns family, actual Domain and CaveReplay algorithm/version1,
  seed/stage/stream, requested options/budgets and versioned CaveAdapter. Effective
  options are stored from the actual result. Budget values above hard ceilings
  refuse rather than normalize; accepted budget-only changes may update provenance
  without changing content/history/views. Every other seed/config/domain/adapter
  input must remain exact under PreserveEdits, even if output coincides.
- Adapter version1 uses X-right/Y-down/x-fast rows; occupancy0 floor ->terrain:one /
  label1, occupancy1 solid ->terrain:two/label2 by explicit owned policy. Map ID,
  exactly two ordered arbitrary valid layer names and all ordered spawns are owned.
  Optional initial_detail is absent/all null or exactly C explicit semantic keys /
  nulls, bounded as BASE input; changed input requires discard. This supports base
  markers and tests real late second-owner refusal. No labels derive from colors.
  Numeric meanings101/203/307/409, labels1..4, derived IDs7..10, atlas targets0..3
  and storage keys remain separate. Shared private meaning tables preserve U5 policy.
- GeneratedDocument retains full semantic base Snapshot and actual Patch witness.
  Use real capture/diff/apply; no hash/seed/opaque identity or forged Witness.
  Export complete FINAL content after Engine last-write-wins input batches, then
  diff to unique authored-layer/row-major addresses. Null edits erase; metadata /
  extent/order/spawn changes refuse as unsupported cell-only transitions.
- DiscardCurrent stages full recipe/base/empty patch, both nonmovable terrain
  owners and reset selected semantic history before adoption. PreserveEdits requires
  an attachment and unchanged attached limits. Replay validates retained base /
  witness/patch, replays the real producer, compares complete semantic base values
  AND canonical bytes before applying the final patch, then validates both owners.
  Same final document is a validated no-op preserving owner/history/borrowed views.
  A different base/seed/config/adapter needs explicit discard; no implicit rebase.
- Attached edits, undo and redo check current complete content, stage full target,
  overlay and history BEFORE nonallocating swaps. An overlay cap failure or a later
  history refusal preserves content, both stacks and attachment. No-ops still
  validate but preserve redo/owner; accepted new edits alone clear redo. Input
  admission/Press/event-pointer and completed-run boundaries stay unchanged; there
  is no whole-tick rollback. Independent west/east and literal documents/history
  remain isolated. Prepared frames/owned observations survive later replacements.
- SAVE writes only existing complete strict-v1 meaning Snapshot, preserving live
  attachment/history. Failed SAVE retains authored runtime edits and predecessor
  under the existing file profile. Successful ordinary semantic LOAD explicitly
  detaches only selected generated state and resets its history; literal LOAD
  leaves it intact. Failed LOAD retains everything. Fresh process LOAD derives
  saved labels/visuals with NO recipe/history and regeneration refuses until a new
  explicit installation. No persisted recipe/patch/schema extension.
- Actual DemoTilemaps::update catches owned generation/patch diagnostics and drops
  remaining tick records; a later tick may still SAVE. Earlier completed actions
  remain. Serialize update/preparation, exclude concurrent writers/recycling, and
  clean up platform resources after simulation joins. Changing adoption invalidates
  borrowed views; owned values/packet resources remain independent. Allocation /
  programming exceptions propagate normally; no broad exception conversion.

## Bounds and measured logical retention

Existing editor axes1..64/exactly two layers/8192 total layer cells, storage8 MiB /
256 spawns/128-byte keys, patch hard1024 unique changes/8 MiB witness and current
history32 entries/1 MiB (hard256/8 MiB) stay unchanged. Default generated limits:
128 final changes,1 MiB witness,4 MiB attachment payload,1 Mi adapter work. Hard
limits1024/8 MiB/32 MiB/4 Mi work; recipe payload<=64 KiB. Reject excess/overflow,
never truncate. Zero edited-address cap permits validated unchanged content.

Final work admission reserves64 full-layer-cell scan passes +32 copy passes,
8 units per allowed overlay address and1 per spawn:96*L+8*cap.edits+spawns, L=2*C.
This is conservative cell-pass/copy accounting for the synchronous staging path,
not machine instructions/time; codec/table/selection internals retain their own
fixed limits. The initial narrower32*L reservation was corrected during review;
all8 U6 names were rerun afterward. Real CaveBudgets govern generator replay and
Engine input selection separately retains32 requests/160 affected/1024 work.
The cave descriptor rejects above-hard budgets; actual requirements and work are
reported independently. Exact/one-short cave cells/storage/work, adapter work,
witness/retained bytes and overlay addresses use small5x4 inputs, not stress/OOM.

Measured31x19/four-change logical charges from the actual caller:

| Category | Bytes / units |
| --- | ---: |
| Owned recipe/effective config/limits + owned field sizes |964 bytes |
| Full canonical semantic base representation |12802 bytes |
| Exact canonical witness string |12802 bytes |
| Four final sparse edit fields |94 bytes |
| Attachment total (recipe+base+witness+edits) |26662 bytes |
| Current semantic history canonical charge |51202 bytes |
| Attachment + history charge |77864 bytes |
| Existing full encoded Snapshot |12812 bytes |
| Current two terrain-owner numeric map/label payload,24*C |14136 bytes |
| Final adapter work reservation,96*1178+8*128+3 |114115 units |
| Actual Cave requirements peak payload |14698 bytes |
| Actual Cave work used |14643 units |
| Cave requested admission |4096 cells /1048576 storage bytes /1048576 work |

The full final file is12812 bytes; attachment+history is already larger. No
compactness or saving is claimed. Recipe accounting uses C++ sizeof fixed recipe /
limits/effective config plus strings and spawn/detail object/content sizes, not
capacity. Base/history use canonical encodings; edits charge layer text +two u32
coordinates +presence byte +optional key. These are mixed LOGICAL charges, not a
wire format or measured heap/RSS. Container/control/capacity overhead beyond stated
recipe charge, live TerrainOwner metadata/config, temporary encodings and old/new
owner/history copies are additional. A declared32 full canonical snapshot copy
allowance is409984 bytes here (32*max(12802,12812)); two recipe copies1928 bytes,
two witness strings25604 bytes and edits94 give437610 bytes of reconstruction
reservation, plus any copied history charge51202 and generator/current numeric
payload separately. Categories can overlap; do not add them into an asserted
allocator peak. Both live and candidate attachments/owners/timelines coexist.
Allocation can fail and synchronous copy/replay/I/O may stall simulation.

## Actual verification and evidence

All configure/build/native checks use `/mnt/projects/cheryl-features/scripts/build --`
with one job and preserved clone-local Debug incremental configurations, shared
native lock and adaptive/no-swap/admission policy. No resource bypass, unchanged
refusal retry, new dependency/pin, install, display/X11 attempt or sibling operation.
Command-local ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1 only; instrumentation/global selections unchanged.
Leak detection remains unverified. Exact commands/environment/immediate exits /
times are in `build/world-u6-logs/commands.jsonl`; full logs and `.exit` sidecars
are preserved there, including the original compilation failure.

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug \
  --target tests-engine tests-terrain-consumer map_storage_consumer \
  map_storage_header_probes tests-world-consumer --parallel 1
/mnt/projects/cheryl-features/scripts/build -- python3 build/world-u6-logs/discover.py
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error --verbose --parallel 1 \
  --output-junit /mnt/projects/cheryl-features/projects/tilemap/build/world-u6-logs/results.xml \
  -R '^(tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.|tests-terrain-consumer\.|map_storage\.consumer\.|world\.)'
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u4-display-debug \
  --target demo --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error --verbose --parallel 1 \
  --output-junit /mnt/projects/cheryl-features/projects/tilemap/build/world-u6-logs/final.xml \
  -R '^(tests-engine\.tilemap_consumer\.generated_|world\.layered_documents$)'
/mnt/projects/cheryl-features/scripts/build -- python3 build/world-u6-logs/check-ownership.py
```

- First shared build exit1,219.99s: new fixture omitted the adapter initializer
  under Werror and used Events::Modifiers instead of CE::Input::Modifiers. Corrected
  without flag relaxation; fixed shared build exit0,52.05s. New cave usage requirements
  rebuilt existing unit collection; seven dangling-else warnings in unchanged
  asset/font/runtime/shader/math cases remain in full log. No unrelated cleanup.
- Discovery exit0:127 unique entries, retaining all119 accepted names +7 new unit
  cases and1 new process scenario; exact registration correspondence saved.
- Accumulated run ONCE:127/127,0 failed/0 skipped, exit0,110.94s. All16 world child
  exits0; complete argv/stdout/stderr/immediate exits saved. New layered author and
  fresh verifier each compare1178 cells +every key/label/meaning/visual/metadata
  against independent literals/cardinal oracle; read-only fresh file hash unchanged.
- Source-review extra LOAD isolation assertions: unit rebuild exit0,17.49s; affected
  generated_files recheck1/1 exit0,1.06s. Literal LOAD preserves attached state and
  semantic LOAD detaches west while east remains exact. No production change here.
- Final work-reservation correction: shared rebuild exit0,43.64s; ALL8 U6 names
  pass8/8,0 failed/skipped, exit0,8.84s. Two additional fresh child exits0, for18
  successful world children in total (initial16 +final2). Final demo rebuild/link
  exit0,20.89s (initial demo build exit0,56.30s). This is NOT another full127 run.
- Final ownership exit0:17 actual Engine imports/exports, SAME private sources,
  original cave producer and normal single shared Engine link/loader/symbol proof.
  Wrapped format dry-run/Werror exit0 for11 C++ files.136 total passing test
  executions across127 unique names; no test failure/skip. One corrected build
  failure remains honestly recorded. Final command/source hashes are saved.

The seven new names test seed-only diversity (42 vs43), a third stage/config,
complete31x19 base/keys/labels/visuals/metadata; empty/no-op replay and borrowed-view
identity; both-layer repeated paint/erase/undo/redo; aliases sharing visual IDs;
new/failed redo policy; exact/one-short caps and history refusal after staging;
unsupported recipe/version/adapter, unknown meanings, same-sized altered bases
with old AND new legitimate witnesses; metadata/layer/spawn changes; actual valid
first-owner then late second-owner refusal; copies/input destruction/old frames;
SAVE preservation/LOAD detachment; actual update diagnostic/drop/recovery. Synthetic
bind/draw/upload counters remain zero. Frozen expected rows come from independent
Python integer reference in ignored logs; actual production ALWAYS calls original
CaveReplay. All589 cells per choice match; floor counts258/290/233 are derived
from full literal comparisons, not accepted as producer-only summaries.

Shared producer/consumer probes inspect actual source hashes/compile commands,
17 real Engine imports/exports (including capture/canonical bytes/encode), symbols /
readelf/ldd/link. The same generated-document/world-document/editor code compiles
into unit/world/demo. Private cave archive contains ONLY existing maze/random/cave
sources once per configuration. Normal shared Engine alone owns storage/tilemap;
no alternate implementation archive/static macro workaround. Public headers,
replay/strict-v1 bytes and375 preserved production/archive paths stay unchanged;
15 previous committed receipts remain exact. Logs total about19.5 MB, preserving
complete diagnostics rather than relying on truncated tool output. Standalone unchanged storage/PG
campaigns are not repeated. Final source hashes, XML, registration and process
correspondence are saved with logs. Format check is wrapped and scoped.

## Remaining limits and exact next action

Real display/input/atlas decode/upload/capture, graphical teardown, visual
correctness/fun and leak checking stay unverified. Prior X11 :1 failure is unchanged
and NOT retried. Source-tree GNU/Linux Debug/shared proof does not establish
installed packaging/SDK, static/Release, other platforms/toolchains/ABI, indefinite
old-version replay or production latency/allocation. Controlled existing directory /
stable cwd/single-writer native checked-rename profile only: no fsync/power-loss,
hostile-race/multiwriter/distributed transaction. No persisted recipe, compact
storage, wire migration, geometry/chunks/physics, sibling integration or new queue.

Next action: coordinator reviews the exact drained standalone checkpoint, receipt
and saved evidence. Publisher alone backs up assigned feat/tilemap. Worker2 remains
IDLE; root alone selects any subsequent assignment/materialization/integration.
