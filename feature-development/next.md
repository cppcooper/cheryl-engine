# Map-storage U8 checkpoint and next action

U8 starts clean at completed U7 `38aaccf5a7c4b9679a92e8b918c93070c6309f39`.
The bounded patch implementation now lives in Engine-owned public CPU source.
All **171/171** accumulated CPU cases passed, with exactly the same names;
**1/1** tests-off patch consumer case and **5/5** actual shared-Engine cases
passed. All **four CPU and four shared header-first probes** built. Export/import,
RTTI and resolved DSO ownership checks passed. The ready receipt records executed
checks, not automatic coordinator source acceptance or installed SDK acceptance.

## Public caller and unchanged semantics

```cpp
#include <cheryl/core/resources/map-storage/cell-patch.h>
#include <cheryl/core/resources/map-storage/file-storage.h>
namespace Patches = CE::Maps::CellPatches;
Patches::Patch patch{Patches::capture(base), {
    {"soil", 1, 0, "garden:sand"}, {"markers", 0, 1, "garden:flower"}
}};
try {
    auto edited = Patches::apply(base, patch);
    CE::Maps::save(path, edited);
} catch (const Patches::Error& error) {
    auto owned = error; // Includes an optional owning CE::Maps::Error cause.
}
```

Engine now owns cell-patch.h under include/cheryl/core/resources/map-storage and
cell-patch.cpp under src/core/resources/map-storage. Tests/private fixture/process
consumer include that public header. The old test-tree header/source and private
map_storage_patches target are removed; one implementation remains. The existing
Engine glob discovers it normally; Cheryl::MapStorageCPU compiles that same file
for source-tree CPU proof. Its inherited static visibility/include requirements
remain target-owned. Old ignored archive/object artifacts are preserved but are
not referenced by generated targets/link edges and are not producer authority.
No copied implementation, test shim, direct .cpp consumer include, dependency or
pin change, generator dependency or export-manifest change exists.

Capture/apply, noninline witness move/access operations and Error construction
use CE_API; Error RTTI uses CE_TYPE_API like Maps::Error. Public types retain
owned standard values. Opaque Witness can only be captured from a validated base;
default and moved-from instances reject, copies own complete independent bytes,
and there is no arbitrary witness-byte import. canonical_bytes() is a borrowed
read-only view; destruction, move or assignment of its witness ends that view's
contract. Copying a view does not own bytes; copying a Witness does.

Apply re-encodes and compares the ENTIRE base's canonical bytes, including all
unrelated cells, layer/spawn identities and order, map identity and dimensions.
No hash/digest is involved. Bounds remain 65,536 total cells, 1,024 unique edit
addresses and 8 MiB witness/candidate encoding, with checked products and finite
zero-origin 2D coordinates into existing stable layer IDs. Only cell set/clear is
permitted. Empty batches still require exact identity and return independent
values. Admission/address/key-size checks precede one candidate copy; existing
public validate(candidate) supplies semantic/full-output checks. Semantic key
errors may occur after allocation in candidate order, mapping back to owning
edit diagnostics and the original owning codec cause. Helpers perform no I/O or
mutation; only complete successful output can reach the caller's checked save.
Failures preserve base, witness, edits and held results. Allocation/programming
exceptions propagate distinctly; existing Error text, including historical
private-budget wording, is preserved. V1 wire/codec/validator/file algorithms and
file publication/ownership rules are unchanged.

MS-011/013 in both decision views record current ownership, alternatives and
reversal costs. These are engineering choices, not inferred personal taste.
Confidence is high in this Linux/GNU same-source CPU/shared proof and medium in
namespace/API adoption. Reversal now affects public source/ABI callers. Full
witnesses can approach 8 MiB; this is not compact persistence, a stable cross-
version identity, or a persisted patch/recipe protocol. No world-coordinate,
undo/history, save-slot or generation policy is selected.

## Actual commands and complete evidence

Every configure/build/native check ran through the shared wrapper from this
clone, one job. Existing Debug/Ninja outputs were reused. Each build automatically
regenerated its changed CMake graph; no separate configure command was needed.
No native check failed, skipped or hit exit 75; there were no compiler warnings.
The adaptive memory/admission/no-swap policy and normal native queue were retained.
No subagent, external agent, download, installation, protected write, remote/ref
change, publication or sibling integration occurred.

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u1-debug \
  --target map_storage_tests map_storage_patch_consumer map_storage_cli --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u1-debug \
  --show-only=json-v1 -R '^map_storage\.'
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u1-debug \
  -R '^map_storage\.' --no-tests=error --output-on-failure --verbose --parallel 1 \
  --output-junit u8-logs/results.xml
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u4-consumer-debug \
  --target map_storage_consumer map_storage_header_probes --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u4-consumer-debug \
  -R '^map_storage.consumer.patches$' --no-tests=error --output-on-failure --verbose \
  --parallel 1 --output-junit u8-logs/results.xml
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u5-shared-debug \
  --target map_storage_consumer map_storage_header_probes --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u5-shared-debug \
  --show-only=json-v1 -R '^map_storage.consumer\.'
ASAN_OPTIONS=detect_leaks=0 /mnt/projects/cheryl-features/scripts/build -- ctest \
  --test-dir build/map-storage-u5-shared-debug -R '^map_storage.consumer\.' \
  --no-tests=error --output-on-failure --verbose --parallel 1 \
  --output-junit u8-logs/results.xml
/mnt/projects/cheryl-features/scripts/build -- python3 \
  build/map-storage-u5-shared-debug/u8-logs/check-exports.py
```

- `build/map-storage-u1-debug/u8-logs/`: build/discovery/tests immediate exits all
  0. Discovery JSON comparison confirms exactly 171 names retained, none added or
  removed. XML confirms the same 171 names executed, no failures/skips/disabled
  tests, 11.69 s total. Full build/tests logs, LastTest snapshot, XML, discovery/
  stderr and `.exit` files remain. `source-preservation.json` verifies the moved
  .cpp is text-identical after namespace/include substitution and codec/file
  sources are unchanged against U7. `link-inputs.txt` verifies no private patch
  target/archive remains in the graph or consumer/test link edges. The native
  suite ran once after extraction; no broad repeated campaign was needed.
- Actual fresh-process patch checks remained in that 171-case run:
  `patch-checks/round_trip-7e3c25876e910269/` author/fresh-verify and
  `patch-checks/preservation-94f76700a9684ad9/` preserve/fresh-predecessor each
  retain argv/stdout/stderr/immediate exit 0 (four invocations). Complete literal
  two-cell garden reconstruction, independent loads and exact predecessor byte/
  value preservation on rejected apply passed. The harness checks verify never
  changes bytes. Rejections are deliberate passing coverage, not failed checks.
- `build/map-storage-u4-consumer-debug/u8-logs/`: build/tests exits 0. The tests-off
  CPU consumer passed its new patches case (1/1, 0.01 s), through public includes
  and the single production archive, without gtest/Python/Engine runtime. All four
  public header-first probes built with inherited CE_ENGINE_STATIC. The new
  cell-patch probe includes the header first/repeated, calls capture/apply, and
  asserts Witness is copy-constructible/nothrow-movable but not constructible from
  string bytes. Compile/link inputs are saved. Actual process logs are in
  `consumer-checks/patches-2e5ee6bddea49777/`.
- `build/map-storage-u5-shared-debug/u8-logs/`: build/discovery/tests/exports exits
  0. The existing scoped normal Engine bootstrap rebuilt its newly discovered
  production patch source and real DSO/consumer. Four public header probes built
  with shared import visibility and no consumer macro patches/private includes.
  Selection is exactly memory, round_trip, preservation, validation and patches:
  5/5 passed, no skips, 0.47 s; seven native child invocations in all. Patch process
  logs are `consumer-checks/patches-5588e45ae91ab1f9/`.
- Both new patch consumer invocations execute the same public capture/apply,
  literal complete two-cell result, independent witness/results, moved-witness
  refusal and preservation rules. Runtime rejects same-ID/same-shape unrelated
  cell changes, layer reordering AND spawn reordering. It catches/copies the
  public Patch Error after inputs and original caught errors die, then copies
  and destroys those retained originals too. Owned BaseMismatch/apply/$.base and
  InvalidEdit/apply/$.edits[1].tile survive; the nested owning Maps::Error retains
  InvalidValue/validate/<snapshot>/$.layers[1].cells[2] and its message. Held result
  and source/patch comparisons establish failure preservation. Defaults are
  exercised by retained accumulated unit cases; the new consumer exercises moved
  witnesses specifically, without using an expired borrowed view.
- The wrapped export script sequentially runs nm/readelf/ldd, saving full output,
  argv and individual statuses (all 0). It checks one compile entry per Engine
  codec/adapter/patch source, CE_BUILDING_ENGINE on the producer, no old private
  source, no consumer CE_ENGINE_STATIC/CE_BUILDING_ENGINE/private JSON/test include
  patch, and no alternate CPU archive/gtest in the shared link. Capture/apply,
  public witness move/assignment/view and Error construction dynamically export;
  Error typeinfo/typeinfo-name/vtable have DEFAULT visibility. The consumer imports
  all five called noninline patch/witness operations. The private string witness
  constructor is absent from dynamic exports. Loader/dependency evidence resolves
  the actual libcheryl-engine.so.4 to this clone's engine build. `patch-exports.txt`,
  compile/link inputs, per-tool logs and exports.log retain the observations.

Shared native checks alone set invocation-local `ASAN_OPTIONS=detect_leaks=0` for
U5's known ptrace/LeakSanitizer runner limitation, retaining normal Engine Debug
ASan/UBSan instrumentation/runtimes. No global environment or sanitizer flags
changed; no unchanged known failure was repeated. Standalone CPU Debug remains
unsanitized. This adds no leak-check acceptance. Prior U7 records/logs/receipt
remain; its next.md is archived as u8-logs/accepted-u7-next.md and in Git.

## Receipt, limits and next concrete action

`feature-development/queue/three-commits-20261024/MAP-STORAGE-U8.json` reports ready
and checks_passed=true only after these implementation/native checks. Evidence
paths point to nonempty saved native logs, not to an unexecuted plan. This reports
execution, not automatic coordinator review or another platform/SDK. No U8
prerequisite/resource/check blocker remains.

Next concrete action: verify the coherent U8 checkpoint's committed receipt,
source/export evidence and saved native results. Stop here; only the finite
queue controller selects the pinned U9 bounded-difference unit after this run
drains and its checkpoint/receipt gate passes. Publisher owns transport. Do not
start U9, a background worker, general persistence or sibling integration now.

Full static Engine, non-Linux/other-toolchain ABI, installed SDK/export manifests
and leak checking remain unverified. No allocation-exhaustion/OOM test was run;
allocation/programming propagation is unchanged by source inspection and the
moved algorithm proof. Public C++ layout/exception ABI requires compatible
consumers. Full-witness costs and current canonical-version coupling remain.
File support remains the accepted Linux caller-controlled serialized-directory
profile: component-respecting paths, final-link rejection, exclusive same-parent
staging, checked I/O/once-only close, owned cleanup and one rename commit. No
fsync/durability, hostile-directory race, network-FS or multiwriter guarantee is
added; post-save failures are not rollback.
