# WORLD-U5 bounded variable-size documents

Implemented and verified; ready for coordinator source review. Exact clean parent
on feat/tilemap: `c8fead0de9424ea56639a846baa8be00bac617a5`. Before dependent work,
the committed U4 ready/true receipt, nonempty saved logs and112 passing XML entries
were inspected. No branch import or sibling/protected write. This is the THIRD /
FINAL map milestone of feature-round-20261024-0500. The
[receipt](queue/feature-round-20261024-0500/WORLD-U5.json) records worker execution,
not automatic source acceptance or a graphical/platform result.

Matching WINT-013/014 in [compact](decisions.md) and [detailed](agent-notes.md)
views supersede earlier fixed geometry/default caps. Historical source archives,
provenance and14 prior receipt files are unchanged.375 production/public Engine /
replay/archive/unchanged-suite hashes match previous evidence. No dependency pin,
schema/strict-v1 bytes, replay or public API changes. All112 accepted registered
names remain; two old refusal fixtures now use65x1 instead of newly valid1x6/2x3.

## Small actual caller and controls

```cpp
DemoTilemapEditor editor(existing_directory);
const std::array load{press(Key::F9)}; // west.json: independently authored7x5/two layers
editor.consume(load, {{400,200},{800,600}});
const std::array semantic{press(Key::B), press(Key::F9)}; // different west-terrain.json
editor.consume(semantic, {{400,200},{800,600}});
// For a7x5 west semantic document, the last cell under this explicit view:
editor.set_view({0,-3072},0.125);
const std::array action{press(Key::T), paint(29,71), press(Key::F6),
                        press(Key::F7), press(Key::F8)};
editor.consume(action, {{400,200},{800,600}});
editor.write(frame, bindings, style);
```

press/paint are actual admitted Record constructors in world-consumer/main.cpp,
not a new Engine input API. The process scenario computes pointers explicitly for
each domain/map. F8/F9 SAVE/LOAD selected map+mode, not just selected layer; F6/F7
undo/redo;1/2 west/east;G/T layer;V/B literal/terrain;3..6 brush;left paint/right
erase/middle sample;WASD pan/wheel zoom/R reset unchanged. Each event owns its
captured pointer. Only admitted Press acts; Release/Repeat/unadmitted and invalid
sizes remain ignored. Earlier completed commands survive a later rejection.
Default empty directory disables I/O; explicit existing caller-controlled directory
required. No startup/automatic I/O or creation. Distinct filenames west/east.json
and west/east-terrain.json remain independent. Terrain labels are never recovered
from visual IDs; label aliases remain distinct persisted keys.

## Implemented boundaries

- Full unchanged storage/adapter validation THEN private axes[1,64], EXACTLY two
  ordered layers and <=8192 layer cells. Checked division precedes product; reject
  unsupported geometry without clamp/truncate/reshape. General U1 converters keep
  their prior65536 patch-cell bound; the private editor/TerrainDocument gate narrows
  it. Current storage still caps8 MiB encoded/input,262144 cells/16 layers,256
  spawns and128-byte keys; reversible tables at most256 entries. Stricter limits
  remain operative. Null alone is empty; all ordered spawns retain validated cells.
- Load/replacement only: no in-place resize, crop, resample, streaming or chunking.
  Four mode/map documents may differ. Retain map ID, ordered names/spawns and all
  cells. Complete mappings/metadata and BOTH nonmovable semantic owners precede
  adoption. Successful LOAD resets only selected history; failure preserves full
  content/stacks, other maps/modes, selection/view and old owned frames/results.
- Active-mode selected extent drives pick_cell. Local64x48 cells are X-right /
  Y-down, computed-edge half-open ownership. Current captured logical/framebuffer
  sizes scale independently; full framebuffer viewport is Y-up, with explicit pan /
  positive zoom. Render/cull each actual Map extent. Fixed origins west{48,240},
  east{328,208}, explicit west ground/detail then east ground/detail; no automatic
  layout or nonoverlap/physical-size claim. Large documents may extend offscreen.
- Default semantic run caps32 requests/160 affected/1024 work. At most5R unique
  cells; work R+5R+5A<=992. Exact32 disjoint interior edits on64x64 require160
  affected/992 work; request31/affected159/work991 reject the whole candidate.
  Duplicate/no-op requests still charge Engine admission work. Full private owner
  reconstruction is separately O(admitted cells), not covered by incremental work.
- Default history32 entries/1 MiB canonical bytes PER document (raised from256 KiB
  to make maximum finite domains useful), hard256 entries/8 MiB unchanged. Exact
  full before+after encodings charge all keys/metadata; current-command one-short
  refuses before content/redo mutation. Oldest candidate eviction unchanged. Four
  default histories total at most4 MiB logical charges, hard max32 MiB. Allocator
  overhead/current owners/configuration/transient snapshots and old/new timeline
  copies are additional. Both terrain layers/semantic keys are restored, not colors.
  A permitted rich-metadata command can still exceed default and explicitly refuse.
- Each current document <=8192 layer cells, semantic label vector<=4096 per layer;
  each resolver vector<=4096 packets, four groups<=16384. Owned stage/copy scratch
  scales with these finite contents and bounded history. These are logical bounds,
  not exact RSS quotas or allocation-success promises. Allocation/programming
  exceptions propagate; copy/rederivation/I/O are synchronous and may stall update.
- Runtime edit/history/adoption, file rename and complete input ticks are separate
  boundaries. Actual update catches anticipated admission/storage/conversion/history
  errors, logs/drop remaining tick, permits later updates; no whole-tick rollback.
  Serialize update/preparation and platform cleanup after simulation joins. Borrowed
  views expire on changing edit/undo/redo/LOAD; copied values/packets retain owners.

These are coordinator-delegated engineering choices, high confidence for exercised
CPU behavior, low as personal API/game taste. Reversal changes private gates/caps /
callers, not Engine ABI/schema. No new preference investigation was repeated.

## Actual commands, exits and results

All native work used `/mnt/projects/cheryl-features/scripts/build --` from this
clone, one job, existing adaptive admission/no-swap/shared-lock policy. No refusal,
new build directory, changed configuration, denied prerequisite retry or X11 probe.
Every invocation used command-local `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`
and `UBSAN_OPTIONS=halt_on_error=1`; existing Debug/shared ASan/UBSan/WARN selections
remain. Leak checking is UNVERIFIED under recorded tracing limitation.

Exact commands/environment/timing/immediate exits: `build/world-u5-logs/commands.jsonl`.
Full stdout/stderr and `.exit` sidecars are retained under that ignored directory.

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug \
  --target tests-engine tests-terrain-consumer map_storage_consumer \
  map_storage_header_probes tests-world-consumer --parallel 1
/mnt/projects/cheryl-features/scripts/build -- python3 build/world-u5-logs/discover.py
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error --verbose --parallel 1 \
  --output-junit /mnt/projects/cheryl-features/projects/tilemap/build/world-u5-logs/results.xml \
  -R '^(tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.|tests-terrain-consumer\.|map_storage\.consumer\.|world\.)'
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u4-display-debug \
  --target demo --parallel 1
/mnt/projects/cheryl-features/scripts/build -- python3 build/world-u5-logs/check-ownership.py
```

| Check | Executed evidence |
| --- | --- |
| Shared CPU build, headers | Exit0,47.88s; fixture review correction and incremental unit rebuild BEFORE tests exit0,13.78s; no warnings |
| Registration | Exit0;119 unique selected entries, all112 predecessor names +6 unit +1 process; correspondence saved |
| Accumulated CPU | ONE run, exit0;119/119,0 failed/0 skipped,89.52s; no failure/retry or CPU rerun |
| New process scenario | variable-author/fresh-variable-verify both exit0;20 files/33168 layer cells each, every key/null/metadata/numeric meaning/label/cardinal visual; read-only verifier preserves all20 file hashes |
| All world processes |14 child invocations exit0; complete argv/stdout/stderr/immediate exits saved |
| Actual demo | Compile/link exit0,21.36s; no warnings; no display/run |
| Ownership | Exit0; SAME world-document/editor sources compiled twice in CPU unit/world and once in demo;14 real Engine exports/imports; nm/readelf/ldd/link confirm one normal shared Engine per executable, no alternate implementation archive |
| Format | Wrapped clang-format --dry-run --Werror exit0 for8 changed/new C++ files |
| Preserved source |375 public/production/replay/archive/unchanged-suite hashes and14 prior receipt hashes match; no standalone storage/procedural repeat needed |

Six new unit cases compare every cell/metadata for7x5,1x7,7x1,1x1,64x64;
full-domain semantic visuals use independent cardinal rectangle-neighbor logic.
They exercise active-mode last/internal/outer/nextafter/exterior picks, distinct
west/east sizes and spawns, full-value edits/undo/redo/save/load, exact/one-short
run/history budgets, zero/65 axes/wrong layers/key/spawn/cell refusals, larger first
owner followed by second-layer failure, independent positive-area culling/pivot /
pass ordering and retained frames after replacement/owner destruction. Synthetic
bind/draw/upload counters remain zero. Actual DemoTilemaps update diagnostics/drop
and next-tick recovery execute synthetically. Test-owned expected-data sharing is
independent of production bridge/selector; fresh expected patterns never derive
from the author's result or adapter summary.

Primary logs: `shared-build.log`, `shared-build-final.log`, `demo-build.log`,
`discovery.log`/`discovery.json`/`registration-correspondence.json`, `tests.log` /
`results.xml`/`results.json`/`LastTest.log`,
`variable_documents-variable-author.log`,
`variable_documents-fresh-variable-verify.log`, `world-processes.json`,
`ownership.log`/`ownership.json` (plus full symbol/link/loader logs),
`format-check.log`, `source-preservation.json`.

## Remaining limits and exact next action

Real display/input, atlas decode/upload, capture, graphical teardown, visual
correctness/fun and leaks stay open. Prior X11 :1 failure is unchanged and not
retried. Compile/synthetic CPU/link checks prove no graphical key delivery or
rendered image. GNU/Linux source-tree shared imports tested; installed SDK,
static/Release, other platforms/toolchains/ABI unverified. Existing controlled
single-writer directory/stable cwd/native checked-rename profile only; no fsync,
power-loss, hostile-race/multiwriter/distributed transaction or compact retention
promise. Current tables/rule/targets are owned immutable reconstruction config;
no migration/hot replacement/schema/recipe/history persistence added.

Next action: coordinator reviews this exact drained checkpoint, required receipt
and saved logs; publisher alone backs up assigned feat/tilemap. Worker2 remains
IDLE after this third/final milestone. No fourth unit, automatic integration,
sibling import or recurring scheduling is authorized.
