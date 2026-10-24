# Map-storage U9 checkpoint and next action

U9 starts clean at completed U8 `db1feff357113b13091e7bb66ff8553e5d8ab884`,
whose ready receipt and accepted 171-case/header/shared-consumer evidence were
inspected before dependent work. U9 adds one public bounded cell-difference
operation; codec, file algorithms, strict v1 format and existing CLI behavior
remain unchanged. The accumulated CPU suite passed **189/189**, retaining all
171 exact names and adding 16 diff cases plus two fresh-process cases.
The tests-off public consumer passed **3/3** new cases; the actual shared Engine
passed **8/8**, retaining its five accepted cases and adding three. All four CPU
and four shared header-first probes built; export/import/RTTI/resolved-DSO checks
passed. The ready receipt reports execution, not automatic coordinator source
acceptance, installed SDK acceptance or an unexecuted platform.

## Small public caller and output contract

```cpp
#include <cheryl/core/resources/map-storage/cell-patch.h>
#include <cheryl/core/resources/map-storage/file-storage.h>
namespace Patches = CE::Maps::CellPatches;
try {
    auto prepared = Patches::diff(base, literal_target);
    auto result = Patches::apply(base, prepared);
    assert(result == literal_target); // Complete independent literal snapshot.
    CE::Maps::save(path, result);      // Separate checked caller publication.
} catch (const Patches::Error& error) {
    auto owned = error; // Code, diff/apply operation, location, optional codec cause.
}
```

Both inputs must satisfy public snapshot semantics/encodability and patch limits:
65,536 total cells, 8 MiB canonical bytes and at most 1,024 changed cells. Identical
map ID, width/height, ordered layer count/IDs and ALL ordered spawn values are
required. Only optional tile values change: replacement, empty-to-key and key-to-
empty. Equal/aliased snapshots produce an empty patch still tied to the exact
entire original base. Applying to same-ID/shape data with an unrelated cell,
layer order or spawn change rejects. Witness bytes include unchanged cells and
all metadata; no hash/digest substitutes for exact comparison.

Diff admits/encodes the base into the existing opaque Witness, admits/public-
validates the target through the existing semantic checks and counting formatter,
then checks metadata in ID/dimensions/layer/spawn field order. Only then does it
count changes in authored-layer then row-major order. Change 1,025 rejects before
an edit vector is allocated/copied. After count admission, reserve the exact count
and scan again, emitting changed target optional keys in that deterministic order.
The complete valid target and unchanged metadata establish the candidate without
a redundant Snapshot copy inside diff. Apply retains its separate exact-match,
address/candidate validation and independent owned result contract.

Output owns one full witness of up to 8 MiB and at most 1,024 Edit objects, each
with bounded layer/key strings (128 ASCII bytes each, or explicit empty). Scratch
is scalar counters and existing bounded semantic/path bookkeeping; no target
serialization buffer, per-cell list or candidate/source Snapshot copy is made.
These are logical bounds, not an allocator/heap quota or admission of already-
oversized caller storage. Both cell scans stop within 65,536 admitted cells;
positive dimensions and divide-before-multiply checks precede coordinate math.
No file access, registry, asset lookup, input mutation or retained input reference
occurs. Shared inputs require caller synchronization. Success deeply owns values;
all failures preserve inputs and previously held caller results. Only anticipated
Maps::Error is translated; bad_alloc/programming exceptions propagate distinctly.

Patch Error appends InvalidTarget and UnsupportedChange after old Code members,
retaining existing ordinals. New errors own operation `diff`, precise $.base/
$.target field context and optional original codec cause. Invalid base retains
InvalidBase/encode context; invalid target uses InvalidTarget/validate context;
valid forbidden metadata uses UnsupportedChange; cell/change/byte budgets use
LimitExceeded. Early base or target validation can precede metadata/count refusal.
Existing capture/apply errors and v1 bytes are preserved. No semantic validator
is duplicated. Witness can now be produced by capture or diff, with no arbitrary
byte import; default/moved-from refusal, copy ownership and borrowed-view lifetime
remain unchanged.

MS-014 in both decision views records this engineering choice, alternatives and
reversal costs. Confidence is high in bounded semantics and medium in new category/
precedence/API adoption. Reversal affects public C++ source/ABI callers, not v1
stored bytes. The full canonical witness remains edit-correctness evidence, NOT
compact generated-base retention, a stable cross-version digest or a persisted
patch/recipe format. generation-storage.md remains an optional later integration
direction; no generator dependency, metadata migration, undo/history or save-slot
policy is selected.

## Actual commands and complete diagnostics

Every configure/build/native check uses the shared build wrapper, one job, existing
clone-local incremental Debug outputs and unique disposable fixture children.
Changed CMake graphs regenerated during the existing builds; no separate configure
was needed. Complete stdout/stderr and immediate .exit files remain in each build's
u9-logs directory. Shared-lock waiting is normal queueing; the original tool session
is preserved. Adaptive memory admission/caps and no-swap remain unchanged.

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u1-debug \
  --target map_storage_tests map_storage_patch_consumer map_storage_cli --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u1-debug \
  --show-only=json-v1 -R '^map_storage\.'
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u1-debug \
  -R '^map_storage\.' --no-tests=error --output-on-failure --verbose --parallel 1 \
  --output-junit u9-logs/results.xml
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u4-consumer-debug \
  --target map_storage_consumer map_storage_header_probes --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u4-consumer-debug \
  -R '^map_storage.consumer.diff($|_)' --no-tests=error --output-on-failure --verbose \
  --parallel 1 --output-junit u9-logs/results.xml
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u5-shared-debug \
  --target map_storage_consumer map_storage_header_probes --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u5-shared-debug \
  --show-only=json-v1 -R '^map_storage.consumer\.'
ASAN_OPTIONS=detect_leaks=0 /mnt/projects/cheryl-features/scripts/build -- ctest \
  --test-dir build/map-storage-u5-shared-debug -R '^map_storage.consumer\.' \
  --no-tests=error --output-on-failure --verbose --parallel 1 \
  --output-junit u9-logs/results.xml
/mnt/projects/cheryl-features/scripts/build -- python3 \
  build/map-storage-u5-shared-debug/u9-logs/check-exports.py
```

- `build/map-storage-u1-debug/u9-logs/`: build/discovery/tests immediate exits 0;
  all 189 discovered names executed, no failures/errors/skips/disabled tests,
  13.73 s elapsed. discovery-comparison.json retains all 171 old names plus the
  exact 18 additions; execution-summary.json checks XML against discovery.
  Complete build/tests logs, discovery/stderr, XML and LastTest snapshot remain.
  The suite ran once after the new implementation, without a repeated campaign.
- Diff cases compare independent literal targets and exact layer-major/row-major
  edit order, replacement and both null transitions; cover equal/aliased inputs,
  every metadata/spawn field/order refusal, invalid inputs, precedence, exact
  65,536-cell/1,024-edit boundaries, change 1,025, public-valid over-patch-cell
  inputs and both 8 MiB encoded caps. They check owned inputs/witness/edits/results,
  copied errors after source/original destruction, held-result preservation and
  whole-base mismatches. Existing codec byte/error/native file regressions pass.
- Real new private process fixtures are
  `patch-checks/diff_round_trip-11fb0738ecea0610/` and
  `patch-checks/diff_preservation-c1e5ed609b7f338d/` beneath that CPU build.
  Four actual invocations (author/fresh verify, refused preparation/fresh predecessor)
  all exit 0, with argv/stdout/stderr/exits retained. The full literal result is
  checked after source/patch destruction; rejected preparation leaves exact bytes
  and decoded predecessor intact. Verification checks unchanged bytes. Expected
  UnsupportedChange diagnostics are deliberate passing coverage, not failed checks.
- source-preservation.json establishes codec/file sources and public snapshot/
  codec/file headers unchanged against U8, with existing capture/apply bodies
  unchanged. Initial span extraction included one extra separating newline and
  failed its source assertion; source-preservation-initial.json is retained.
  Corrected extraction normalizes only trailing newlines of the selected span;
  no production fix or native rerun was needed for that inspection-script issue.

- `build/map-storage-u4-consumer-debug/u9-logs/`: build/tests exits 0, three new
  diff/diff_round_trip/diff_preservation cases passed, 0.05 s, five actual child
  invocations. All four public header probes built, including first/repeated
  cell-patch.h, capture/diff/apply and opaque/copy/nothrow-move assertions. Saved
  compile/link inputs establish inherited CE_ENGINE_STATIC and public includes,
  private JSON absent from consumers, tests off without gtest/Python, and one
  production CPU archive rather than a test-tree implementation. Process paths
  are `consumer-checks/diff-418dcf897b38c086/`,
  `diff_round_trip-73da44305b767553/` and `diff_preservation-d6a615c87b4824f7/`.
  Prior five CPU consumer cases retain accepted evidence; no broad rerun was needed.
- `build/map-storage-u5-shared-debug/u9-logs/`: build/discovery/tests/exports exits
  0. Normal scoped shared Engine rebuilt the changed production patch source,
  DSO/consumer and all four header probes. The eight discovered cases (old five
  plus diff/diff_round_trip/diff_preservation) all passed, 0.88 s, twelve actual
  child invocations. execution-summary.json compares discovery/XML, old names and
  per-process exits. New process directories are
  `consumer-checks/diff-b5651954305674fd/`, `diff_round_trip-9a218170baed6b1b/`
  and `diff_preservation-bd421d16de1f40e3/`; all remain disposable clone children.
- Both public-consumer modes execute literal complete target reconstruction and
  copied UnsupportedChange/diff/$.target.spawns[0].x plus InvalidTarget/diff/
  $.target after inputs/original errors die. The latter owns the original
  Maps::Error with validate/<snapshot>/$.layers[1].cells[2]. A previous successful
  Patch and both inputs survive rejected preparation. Separate author/verify
  processes test diff/apply/checked save, destroyed inputs and complete literal
  load. Rejected preparation preserves distinct predecessor exact bytes/reload,
  then a separate read-only process verifies it. Post-save errors never imply
  rollback. Existing unrelated-cell/layer/spawn mismatch behavior remains tested.
- Wrapped nm/readelf/ldd checks save individual full outputs/argv/status (all 0).
  diff is a GLOBAL DEFAULT dynamic function exported by this clone's real Engine
  and an undefined import in the consumer. Existing capture/apply/witness exports,
  Error constructor and DEFAULT-visible RTTI/typeinfo-name/vtable remain. The
  private witness byte constructor is absent from dynamic exports. Compile/link
  inspection establishes exactly one real producer per codec/adapter/patch source,
  CE_BUILDING_ENGINE only on the Engine, no consumer CE_ENGINE_STATIC/import
  patch or private JSON/test include, no alternate CPU archive/gtest shared link,
  and loader resolution to this clone's libcheryl-engine.so.4. Full evidence is
  in exports.log, patch-exports.txt, compile-inputs.json, link-inputs.txt and the
  per-tool logs. No producer/ABI defect or integration workaround was required.

No native command failed, skipped or refused admission, and no compiler warning
occurred. Shared execution alone retains invocation-local
ASAN_OPTIONS=detect_leaks=0 for the recorded U5 ptrace/LeakSanitizer limitation;
normal Engine Debug ASan/UBSan remain enabled. No global environment or sanitizer
flag change occurred. CPU Debug remains unsanitized; no new leak claim follows.

## Receipt, limitations and next concrete action

feature-development/queue/three-commits-20261024/MAP-STORAGE-U9.json reports ready
and checks_passed=true only after the implementation/native acceptance above.
Its evidence paths name nonempty saved native logs. No U9 prerequisite/resource/
check blocker remains. Review and checkpoint this coherent assigned unit; U9 is
the last pinned map-storage milestone in this finite batch. Next concrete action
after the checkpoint: coordinator source/evidence review and publisher backup.
Stop rather than start another unit, background agent, SDK work or sibling
integration. The queue controller owns scheduling; receipt readiness is not
automatic coordinator source acceptance.

Prior U8 records/logs/receipt remain; its next.md is archived in
build/map-storage-u1-debug/u9-logs/accepted-u8-next.md and Git. Full static Engine,
non-Linux/other-toolchain ABI, installed SDK/export-manifest and leak checking
remain unverified. No OOM/allocation-exhaustion test is authorized or run;
allocation/programming propagation is maintained by source inspection. Public C++
values/exceptions require ABI-compatible consumers. Borrowed canonical views expire
with witness destruction/move/assignment. Full witnesses are not compact persistence.
Linux files remain caller-controlled single-writer, native component-preserving
paths, final-link rejection, exclusive same-parent staging, checked I/O/EINTR/
once-only close, owned cleanup and one rename commit. No fsync/durability,
hostile-directory-race, network-filesystem or multiwriter guarantee is added;
post-save failures are not rollback. No pin/dependency/system installation,
protected change, sibling integration, remote/ref change or publication is selected.
