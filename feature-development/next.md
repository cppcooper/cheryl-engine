# WORLD-U1 checked CPU integration handoff

Status: implementation and focused CPU checks pass; coordinator source/promotion
review pending. This is a new assignment after TILEMAP-U10, not a fourth finite
queue unit. No follow-up scheduling or worker1 integration starts here.

Exact original assigned head was clean feat/tilemap at
`edf710a79ef4fc59c7949f7bc78d9bf7d617e29a`. The local fetch retrieved only candidate
`aca49bdbfc5ac8d1f736d78f9f4ad8f4f815b56d` from projects/world-integration.
All three sources below were checked ancestors, then ordinary --ff-only adopted
that exact candidate and left a clean tree. The candidate is this checkpoint's
parent; no arbitrary sibling tip, reset, rebase, force or cherry-pick was used.

| Source | Exact SHA |
| --- | --- |
| Tilemap | edf710a79ef4fc59c7949f7bc78d9bf7d617e29a |
| Map storage | def28ccfc63eb057995f6e1fb95ee48cfeb8fd75 |
| Procedural generation | 946c2db7d9a14311a398664444fc4d1fe0417a90 |

All nine archived records/provenance, public CPU sources/headers, generator replay
and tests remain unchanged. source-preservation.json checks 65 paths against the
candidate. Root WINT-001–006 decisions match in [compact](decisions.md) and
[detailed](agent-notes.md) views. Controls/meanings are engineering policy, not
user preference evidence. [Receipt](integration/WORLD-U1.json) reports executed
behavior, not whole-feature or automatic coordinator acceptance.

## Caller, ownership and observed behavior

The [private caller](../projects/engine/tests/world-consumer/main.cpp) includes
public headers first, constructs actual Engine values, and links one normal shared
Engine with storage and tilemap. Its [bridge](../projects/engine/tests/world-consumer/bridge.h)
is private application policy. No Engine implementation is copied or linked via
an alternate archive in this process. The generator-only private archive compiles
committed maze.cpp/random.cpp/cave.cpp directly, bypassing neither algorithm nor
its limits. No public procedural API or new World framework is selected.

```cpp
const auto untouched_base = CE::Maps::load(input);
auto live = WorldProof::LiteralOwner::load(input, key_table);
live->map().set({0}, {0, 0}, CE::Tilemaps::TileId{613});
auto target = live->snapshot();
auto patch = CE::Maps::CellPatches::diff(untouched_base, target);
CE::Maps::save(output, CE::Maps::CellPatches::apply(untouched_base, patch));
// A new verifier process loads output against independent complete literals.
```

- world.literal: raw independent 3x2 JSON, ground/detail in authored order,
  explicit nulls, map ID and two ordered spawns. Key/ID table owns earth101,
  stone307, water613, flag911, flower1201, wall2017 and floor4051; zero is null.
  Both lookup directions check every entry. Every cell and metadata field matches
  independent full snapshots/numeric arrays before/after six actual Map edits.
  Diff returns six exact layer-major/row-major addresses; apply leaves base/old
  copied map/sibling intact. Checked save follows owner destruction. A fresh
  process reloads/reconstructs all 12 numeric cells and complete metadata.
- world.generated: actual cave {5,4}, stream {42,0x776f726c64,7}, fill 4500, one
  birth/survival0 smoothing step, retain-components and budgets 64/8192/10000.
  Actual usage is 231 work/six draws. Full literal occupancy is 14 perimeter solid
  and six interior floor cells. Application adopts X-right/Y-down, x+5*y.
  Solid 1 -> label 11/proof:wall/meaning ID2017; floor 0 -> label 23/proof:floor/ID4051.
  Empty terrain label 0 is distinct from cave floor 0. Explicit N/E/S/W masks select
  targets0..15, mapped to derived visuals 1001..1016. Second layer/flag and two
  spawns survive. Three wall/floor/wall requests at (2,1) publish final wall through
  actual TerrainMap::edit_batch; exact center/N/E/S/W invalidation, full independent
  label/visual oracle, old results/sibling/other layer and one-address final diff
  pass. Fresh reload compares complete literal snapshot, 40 numeric meaning cells,
  20 labels and derived visuals without generator replay.
- world.refusals: zero/duplicate/empty/overlong tables, unknown key/reverse ID, changed Map
  shape, invalid storage shape, >65,536 patch cells and change 1025 all refuse.
  Metadata ID/shape/layer name/order/spawn coordinate/name/order changes reject
  as UnsupportedChange after valid setup; altered exact bases and duplicate patch
  addresses reject. Unknown labels (including overwritten), bad coordinates,
  initially unused missing mask becoming a later (3,1) failure and unmapped initial
  target preserve prior adopted terrain, semantic/visual cells and old results.
  Owned inputs can die without affecting owners. Refused invalid save preserves
  exact predecessor bytes, then another process verifies all literal fields/cells.

Key/semantic tables and metadata are copied. Key bytes are capped before a
validation-probe/optional-key copy; small 129-byte fixtures verify refusal without
large allocations. TerrainMap is owned through fresh
unique_ptr candidates; no move API is required. Serialize all access/preparation;
borrowed views expire with owners, input spans are retained only for calls.
Direct Map edits are immediate. Failed export preserves that edited runtime and
previous snapshot, not a rollback of authored edits. Engine batches publish
labels/visuals together; export/diff/apply and checked file save have their own
complete-result/publication boundaries. No whole-frame/distributed transaction.
Adoption/export use public validate and stricter patch bounds; byte/change/work
refusals reject rather than truncate. No allocator/OOM probes occur.

## Build, discovery and test evidence

All native operations use /mnt/projects/cheryl-features/scripts/build --, one job,
normal shared queue/adaptive limits/no swap/admission floor. No resource refusal,
limit bypass, installs, downloads, pin changes or protected/sibling outputs occur.
Preserve existing build/tilemap-u1-debug Debug/shared/WARN/tests and native/OFF
selections, CMake policy minimum3.5, all sanitizer settings and incremental outputs.

Two new clone-local outputs are necessary: unchanged storage unit fault tests
need hidden FileOps from its standalone CPU producer; the procedural CMake refuses
add_subdirectory and retains its own focused suite. They do not replace combined
shared-Engine proof. Neither protected nor sibling caches/outputs are used.

```sh
# Existing shared configuration regenerates from source during this build.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug \
  --target tests-engine tests-terrain-consumer map_storage_consumer map_storage_header_probes tests-world-consumer --parallel 1
/mnt/projects/cheryl-features/scripts/build -- cmake -S projects/tests/map-storage \
  -B build/world-storage-cpu-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DMAP_STORAGE_BUILD_TESTS=ON
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/world-storage-cpu-debug \
  --target map_storage_tests map_storage_patch_consumer map_storage_cli --parallel 1
/mnt/projects/cheryl-features/scripts/build -- cmake -S projects/tests/procedural-generation \
  -B build/world-procgen-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DPROCGEN_UBSAN=ON
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/world-procgen-debug --parallel 1
/mnt/projects/cheryl-features/scripts/build -- python3 build/world-u1-logs/check-ownership.py
```

Before execution each selection uses wrapped ctest --show-only=json-v1 and saves
its complete discovery. Actual test commands (JUNIT paths expanded to this clone):

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error --verbose --parallel 1 \
  --output-junit /mnt/projects/cheryl-features/projects/tilemap/build/world-u1-logs/shared-results.xml \
  -R '^(tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.|tests-terrain-consumer\.|map_storage\.consumer\.|world\.)'
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/world-storage-cpu-debug \
  --output-on-failure --no-tests=error --verbose --parallel 1 \
  --output-junit /mnt/projects/cheryl-features/projects/tilemap/build/world-u1-logs/storage-results.xml -R '^map_storage\.'
UBSAN_OPTIONS=halt_on_error=1 /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/world-procgen-debug \
  --output-on-failure --no-tests=error --verbose --parallel 1 \
  --output-junit /mnt/projects/cheryl-features/projects/tilemap/build/world-u1-logs/procgen-results.xml -R '^procgen\.'
```

| Observed operation | Immediate exit / result |
| --- | --- |
| Initial shared build | 1: caller incorrectly included cheryl/internals/exceptions.h; fixed to existing internals/exceptions.h |
| Second shared build | 1: Werror caught copied range-loop vector; fixed to const reference |
| Third/final shared builds | 0/0: links after correction, then final ownership/refusal additions; no warnings |
| Storage/procedural configure/build | 0 each, no warnings |
| Format/final dry-run | 0 each; final source unchanged after affected recheck |
| Header/link/producer inspection | 0; six combined + four storage + one terrain probes build |
| Shared accumulated test invocation | 90/90 passed, zero failed/skipped,49.55s |
| Unchanged storage CPU invocation | 189/189 passed, zero failed/skipped,13.93s |
| Unchanged procedural invocation | 71/71 passed, zero failed/skipped,0.36s |
| Review key-byte preflight addition | Private caller rebuild 0, affected world 3/3,1.23s; final ownership/format checks 0 |

Each accumulated selection ran ONCE. Review subsequently added cheap 128-byte
key admission before validation-probe copies, with two small 129-byte refusal
fixtures. Only the private caller was rebuilt and its three world scenarios
rechecked (3/3); final import/source-hash and format checks passed. No test ever
failed. There are 350 unique passing entries and 353 passing executions, not two
full accumulated runs. All 79 previous tilemap names match the saved U10 registration, all eight storage shared consumer
names remain, and all 71 procedural names match unchanged registration source.
The storage suite retains 189 entries from unchanged source/registration. Discovery
sets exactly equal executed XML sets; total 350 entries pass. The initial six and affected-recheck six combined
children all exit 0; author/verify/rejected-predecessor transcripts and exact argv
are retained. Independent fresh verifiers leave bytes unchanged. Existing old
frame/native-counter tests remain passing; this new caller observes CPU values,
not rendered pixels.

The review rebuild targets only tests-world-consumer. Its wrapped/sanitized ctest
invocation filters -R '^world\.' and writes preflight-results.xml.
preflight-build/tests/ownership/format-check logs and commands.sh preserve exact
argv/exits and the final code hashes. No further source change follows.

Complete logs/argv/immediate .exit files, discovery/LastTest/XML/result comparisons,
child output, symbols/loader/link/compile inputs and source preservation are under
ignored build/world-u1-logs/. commands.sh records literal full operations; native
process fixture paths are indexed by world-processes.json and
preflight-world-processes.json. Expected refusal
messages are passing negative checks, not failed cases. Nothing is inferred from
truncated tool output; full stdout/stderr remains in those logs.

Actual ownership checks find 14 Engine exports matching real combined imports,
visible diagnostic RTTI, exactly one Engine producer for each of five storage/
tilemap sources, normal consumer usage without CE_ENGINE_STATIC/producer patches,
and loader resolution to this clone's libcheryl-engine.so.4. Only the actual three
private procedural sources enter the generator archive. JSON/private Engine source
requirements do not leak into the consumer. Normal STL/toolchain ABI limitations
and GNU diagnostic COPY-relocation behavior remain; no broad ABI claim is added.

Shared Engine/combined caller retain Debug ASan/UBSan/halt_on_error with no
diagnostics; detect_leaks=0 is invocation-local solely for the recorded tracing
limitation. Standalone storage Debug is unsanitized as before. Standalone procedural
is Debug/UBSan; its combined cave inputs additionally compile under root ASan/UBSan.
Global flags/settings are unchanged. Leak checking remains unverified.

## Limits and precise next action

No unresolved WORLD-U1 CPU/build prerequisite remains. The receipt may be ready
only for these executed gates. Real display/input, atlas decoding/upload, capture,
graphical teardown, visual correctness/fun and leaks remain open; unchanged X11 :1
failure is not retried. No installed SDK/static/Release/non-Linux/other-toolchain
acceptance, compact retention, saved recipe/patch format, fsync/power-loss,
hostile-race, multiwriter or distributed-transaction guarantee is supplied.

Next action: coordinator reviews this exact checked descendant, receipt and saved
logs, verifies publisher backup/drained processes, then decides promotion into
worker1. Worker2 remains idle after this local checkpoint, does not push/merge
into worker1 or alter unassigned refs, and starts no follow-up scheduler/unit.
