# Map-storage U6 checkpoint and next action

U6 starts clean at accepted U5 `48ad543a18ebacdb759bb5bf7bc1f5ed64202917`.
The branch now implements a **private exact-base cell-patch application proof**.
All **162/162** selected cases passed: every accepted 136 regression name plus
24 new patch cases and two actual-process checks. No test was skipped; configure,
build, discovery and execution exited 0. No production header, codec/file
algorithm, format, dependency, pin, existing CLI or shared consumer changed.
Coordinator acceptance remains separate from these local results.

## Small caller and private contract

```cpp
// cell-patch.h is private to projects/tests/map-storage.
CellPatches::Patch patch{CellPatches::capture(base), {
    {"soil", 1, 0, "garden:sand"},
    {"markers", 0, 1, "garden:flower"}
}};
auto edited = CellPatches::apply(base, patch);
CE::Maps::save(path, edited);
// Another process: Maps::load(path) == PatchFixture::expected(), a complete literal.
```

`capture` validates and owns the complete deterministic public `encode(base)`
bytes. `apply` revalidates/re-encodes the proposed base and compares those bytes
exactly, including unrelated cells, layer/spawn order and identity. Map ID and
shape alone are insufficient. No hash/digest or collision claim is involved.
Only existing cell values can be set/cleared by stable layer ID and finite 2D
coordinates. No topology, identity, shape, ordering or spawn change is applied.
The helper performs no file access or input mutation.

Chosen private bounds: 65,536 total base cells, 1,024 edits, public key/dimension/
layer/spawn limits, and 8 MiB for both witness and candidate canonical encoding.
Shape and products use divide-before-multiply admission. Batch size and witness
validity precede base encoding/matching; each layer, coordinate, duplicate and key
is checked before one deep candidate allocation. A constant one-cell public
codec probe supplies key grammar validation. Complete output validation may
reject an otherwise valid batch at the encoded-byte cap. The transient base
encoding is released before copying the candidate; no candidate is copied per
edit. Bookkeeping contains at most 1,024 addresses. These cap admitted logical
storage/work, not allocator overhead or memory already allocated by a caller to
an oversized aggregate input. A full witness may approach 8 MiB: this is safe
edit semantics, **not compact storage, persisted patches or recipe replay**.

Empty batches require the same exact base match and return an independent copy.
No-op sets/clears are valid, but repeated addresses reject even for no-ops;
coordinates on different layers are independent. Opaque witnesses can only be
captured from valid bases; default/moved-from witnesses reject, copies own bytes,
and move invalidates its source explicitly. There is no arbitrary witness-byte
import. Source, witness, edits and prior successful results survive all rejection
paths. `CellPatches::Error` owns category, operation, field location and optional
copied public codec context. Anticipated codec rejections translate; allocation
and programming exceptions propagate unchanged. Shared input mutation requires
caller synchronization. A rejected application never reaches caller save().

These are engineering choices, not inferred user API taste. MS-011 in both
[decisions.md](decisions.md) and [agent-notes.md](agent-notes.md) records provenance,
alternatives, costs and the same contract. Confidence is high in exact-match/
ownership semantics, medium in workload caps; changes affect private callers,
tests and records only. The witness relies on current canonical wire behavior;
it is not a stable cross-version retention identity.

## Actual verification and saved artifacts

Reused `build/map-storage-u1-debug`, GNU C++23 Debug/Ninja, with one build/test
job through `/mnt/projects/cheryl-features/scripts/build --`. All native children
stayed inside that wrapper's shared resource envelope. No resource refusal,
download, dependency preparation, device/UI run or Engine campaign occurred.
Complete logs and immediate exit files are under
`build/map-storage-u1-debug/u6-logs/` (ignored clone-local output). Prior U5 next.md
is also saved there as `accepted-u5-next.md`; its committed record is unchanged.

Actual commands, all from this clone:

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake \
  -S projects/tests/map-storage -B build/map-storage-u1-debug \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u1-debug \
  --target map_storage_tests map_storage_patch_consumer map_storage_cli --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u1-debug \
  --show-only=json-v1 -R '^map_storage\.'
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u1-debug \
  -R '^map_storage\.' --no-tests=error --output-on-failure --verbose --parallel 1 \
  --output-junit u6-logs/results.xml
```

- `configure.log/.exit`: 0, reused Debug configuration; no pin/source acquisition.
- `build.log/.exit`: 0, new prototype/test/consumer compiled without warnings.
- `discovery.json`, `discovery.stderr/.exit`: 0; exactly 162 names. Static JSON
  comparison with accepted `u4-logs/discovery.json` confirms all 136 names retained,
  none removed and exactly 26 added (`discovery-comparison.json`).
- `tests.log/.exit`, `LastTest.log`, `results.xml`: 0, 162 tests, zero failures,
  skips or disabled cases, 9.89 seconds total. XML case names exactly equal the
  discovered names. This single run includes both new cases and all regressions;
  no redundant broad rerun was needed.
- `compile-link-inputs.txt`: actual Ninja compile/link blocks show patch/consumer
  compiling against only the declared public Engine include root, inheriting
  `CE_ENGINE_STATIC` from Cheryl::MapStorageCPU, with no private JSON/FileOps
  includes. The process consumer links only `libmap_storage_patches.a` and the
  existing Engine-owned-source `libmap_storage_codec.a`, no gtest/Engine DSO or
  native/graphics startup. Existing tests retain pinned gtest as before. The
  prototype targets live after the tests-on guard; the tests-off CPU path is
  unchanged and was not reconfigured/rerun.
- `unchanged-public-and-accepted-consumers.diff/.exit`: empty / 0 against accepted
  U5 for all Engine source (including shared consumer) and existing codec/file/
  CLI checks. Source inspection plus prior accepted U5 evidence establish this
  unchanged boundary; no new shared-Engine claim is inferred from the CPU run.
- `commands.txt`: exact top-level native argv. Process-level `.command`, stdout,
  stderr and `.exit` files are retained in the two unique child directories below.

New gtest coverage includes full literal two-cell reconstruction and unchanged
metadata/order/spawns/empties; canonical witness bytes; source/witness/edit/result
lifetime isolation; empty batches; clear/no-op behavior; duplicate and distinct-
layer coordinates; missing/overlong layer, coordinate maxima, invalid/overlong/
maximum keys; default/moved witness; invalid base/shape/count/product admission;
exact 65,536-cell and 1,024-edit success; 1,025-edit refusal; overencoded base and
candidate; owning diagnostic copies surviving all inputs. Exact-base mismatch
cases include same-ID/same-shape unrelated cell changes, spawn-only coordinate/
name changes and layer reordering, plus identity and reshaped values. Refusal
checks compare every input and previously held successful result after failure.
The near-limit case fills 63,200 cells with 128-byte keys, then attempts 1,024
additional sets; it specifically reaches final encoded-output rejection.

Actual independent process evidence (four invocations, each exit 0):

- `patch-checks/round_trip-fc80f94a0bf39d2a/`: author applied the two garden edits,
  compared the full independent literal, checked-saved, destroyed/mutated source
  and patch, and reloaded. Separate `fresh-verify` loaded and compared that literal
  (all twelve cells/empties, both layers/order and both spawns) and checked two
  independent loaded instances. Harness verified unchanged bytes during verify.
- `patch-checks/preservation-fcc1f7e7fbdcad98/`: preserve seeded distinct valid
  `garden:predecessor`, captured exact bytes, and attempted save(apply(...)) using
  a same-ID/same-shape base with unrelated soil cell 4 changed. The expected
  `BaseMismatch operation=apply location=$.base` was caught before save; exact
  predecessor bytes and full reload were unchanged. Separate `fresh-predecessor`
  verified the literal and the harness checked verify did not change bytes.
  Expected refusal stderr is deliberate coverage, not a failed test/checkpoint.

The unchanged harness is
`projects/engine/tests/map-storage-consumer/check.cmake`; it launches the new
private executable, not a function pretending to be main. Existing CLI process
regressions were retained in the 162-case run. No new fault knob or public patch
API was introduced.

## Boundaries and next concrete action

No blockers remain for local U6 verification. Stop after the coherent local commit
for coordinator source/evidence review of MS-011, the private helper and these
162 outcomes. Publisher transport stays with the coordinator. Do not begin a
new format, persisted recipe/patch, SDK export or sibling integration automatically.

Future generated-base/recipe retention needs its own assigned semantic domain,
algorithm/version/input provenance, exact identity/revision and old-version replay
or migration/baking policy. This prototype retains full canonical bytes and saves
only a complete ordinary snapshot; it proves none of that compact persistence.
No procedural/cave code, N-D flattening, tilemap or lifecycle dependency exists.

U5 evidence remains accepted: three actual shared-Engine consumer cases, three
shared and three CPU self-contained header probes, exported public functions/
Error RTTI and exact local DSO ownership. Its initial three LeakSanitizer ptrace
failures and invocation-local `ASAN_OPTIONS=detect_leaks=0` rerun remain recorded
in accepted U5 logs/commit. U6 uses the standalone CPU Debug target without Engine
ASan/UBSan flags; it sets no global or invocation leak environment workaround and
does not establish leak/sanitizer acceptance. Static Engine, non-Linux, other
ABI/toolchains and installed SDK remain unverified. Linux caller-controlled,
serialized directory/native-path/leaf-symlink/exclusive-stage/checked-I/O/one-rename
contracts stay intact. No durability/fsync, hostile-directory race, network-FS or
multiwriter guarantee is added; failures after a successful save are not rollback.
