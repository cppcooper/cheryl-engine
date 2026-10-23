# Map-storage U4 checkpoint and next action

U4 extracts the accepted U3 boundary (`9efd4fd9f248fdd3aa7971d260d45c8449896bdc`)
into Engine-owned public CPU headers/source. The exact **136 accepted regression
names all passed** after extraction; the three new independent consumer cases
have passing outcomes, and all three public header-first probes built. One new
consumer diagnostic assertion failed initially, was corrected, and passed its
focused rerun as detailed below. Coordinator review of this coherent local
checkpoint is the next action. No subsequent unit starts automatically; the
coordinator publisher owns checkpoint transport.

## Implemented boundary and small caller

Public headers are `cheryl/core/resources/map-storage/{snapshot,codec,file-storage}.h`,
namespace `CE::Maps`. Snapshot/Layer/Spawn own standard values and support complete
equality. parse/encode/load/save and Error's constructor declare CE_API; Error
also has CE_TYPE_API for public ELF type visibility. No JSON or native FileOps
appears in the public headers. The only authoritative codec/adapter files now
live in `projects/engine/src/core/resources/map-storage/`; private FileOps stays
beside them. Tests/CLI retain their existing fixture, behavior and names, consuming
those exact files. `MapFixture` remains only in private fixture/formatter helpers.

The small non-fixture caller in [agent-notes.md](agent-notes.md#ms-001-cpu-boundary)
authors an owned Snapshot, encodes/parses it, saves/loads an explicit path and
inspects a copied owned Error. MS-007/MS-009 in both [decision views](decisions.md)
record placement, export/usage requirements, alternatives, confidence and reversal.
The independent executable authors a different `garden:north` 2x3 soil/markers
map with gate at soil(0,2), lookout at markers(1,0), twelve literal cells/empties
and complete equality; it includes no fixture/test headers or private seams.

Wire identity remains `cheryl.map-fixture`, integer version 1. No validation,
budget, deterministic encoding, diagnostic precedence, ownership or I/O algorithm
changed. `u4-logs/algorithm-identity.txt` confirms codec.cpp, file-storage.cpp and
file-operations.h are identical to accepted U3 after namespace/include
substitution. Header changes add Engine visibility and necessary CPU contracts.
Native paths remain absolute once with original components, honoring ancestor
symlink/.. selection and rejecting final symlinks/nonregular targets. Staging,
checked short I/O/EINTR, descriptor relinquishment before exactly-once close,
owned cleanup/primary diagnostics and one rename commit remain intact.

## Actual configure/build/native commands

All commands ran from this clone through `/mnt/projects/cheryl-features/scripts/build`,
one job, with complete output and immediate status retained in the indicated
build's `u4-logs/`. Existing Debug outputs/logs were preserved. The separate
configuration exposes a tests-off dependency/usage boundary that tests-on cannot
establish. It configures the standalone entry with MAP_STORAGE_BUILD_TESTS=OFF,
then consumes only Cheryl::MapStorageCPU's declared public interface. The CLI
is still available as an excluded target; no fixture source is compiled/linked
into the independent consumer.

```bash
/mnt/projects/cheryl-features/scripts/build -- cmake -S projects/tests/map-storage -B build/map-storage-u1-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u1-debug --target map_storage_cli map_storage_tests --parallel 1
/mnt/projects/cheryl-features/scripts/build -- cmake -S projects/engine/tests/map-storage-consumer -B build/map-storage-u4-consumer-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMAP_STORAGE_BUILD_TESTS=OFF -DCMAKE_DISABLE_FIND_PACKAGE_Python3=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u4-consumer-debug --target map_storage_consumer map_storage_header_probes --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u1-debug -R '^map_storage\.' --show-only=json-v1 --no-tests=error --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u4-consumer-debug -R '^map_storage\.consumer\.' --show-only=json-v1 --no-tests=error --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u1-debug -R '^map_storage\.' --output-on-failure --no-tests=error --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u4-consumer-debug -R '^map_storage\.consumer\.' --output-on-failure --no-tests=error --parallel 1
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u4-consumer-debug --target map_storage_consumer --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u4-consumer-debug -R '^map_storage\.consumer\.memory$' --output-on-failure --no-tests=error --parallel 1
```

Configure/build outputs were redirected to `configure.log`/`build.log` (both
stdout/stderr), discovery JSON to `discovery.json` with separate
`discovery.stderr`, and CTest output to `all-tests.log`. The final consumer-only
build/check use `build-fix.log` and `memory-after-fix.log`. Each command saved its
immediate `$?` in the corresponding `.exit` and returned that status. All exited
0 except the first three-case consumer CTest, which exited 8 on the assertion
below; it remains saved as both `all-tests.*` and `initial-all-tests.*`.
Initial and final consumer LastTest logs are retained separately. No output
truncation prevents inspection of an actual error.

The tests-on Ninja/GNU C++ 16.2.1 Debug build reused pinned gtest
`6dae7eb4a5c3a169f3e298392bff4680224aa94a` and private JSON 3.11.3
`b36f4c477c40356a0ae1204b567cca3c2a57d201`. No dependency/pin/asset or system-tool
change was needed. Tests-off configures/builds with no gtest or Python discovery,
and no legacy dependency policy override is needed there. It produced one CMake
unused-variable warning for CMAKE_DISABLE_FIND_PACKAGE_Python3 because no find
call reached it; neither native build emitted compiler warnings. Both build
configurations declare C++23 and CE_ENGINE_STATIC through the producer interface,
not consumer compile-definition patches.

## Discovery and executed results

`build/map-storage-u1-debug/u4-logs/discovery-comparison.json` records exact set
equality with preserved U3 discovery: 136 retained, zero missing/added; independent
consumer discovery is nonempty with memory, round_trip and preservation. Discovery
commands finished with exit 0. An early static JSON comparison read the pending
output too soon and got JSONDecodeError; the original live sessions completed and
only that static comparison was rerun successfully. No native command was
cancelled/relaunched or repeated to work around queueing/admission.

- **Accepted regressions:** 136/136 passed, no failures/skips, 9.14 seconds real
  (9.05 summed test seconds). This retains 86 codec, 36 file, two formatter and
  twelve CLI process cases. Full `all-tests.log` and copied `LastTest.log` are
  under the reused build's u4-logs. The CLI harness made 71 real invocations;
  `cli-process-summary.json` inventories their unique retained fixture roots and
  expected/actual statuses: eighteen 0, two 1, sixteen 2, twenty-five 3 and ten 64.
  Every deliberate rejection/mismatch/output-failure exit matched expectations.
  Native path meaning, genuine rename/staging failure, exact predecessor
  preservation, explicit retry, fresh verification and honest post-save output
  failures remain covered. The unchanged passing selection was not rerun.
- **Independent consumer:** initial run passed round_trip and preservation but
  failed memory because the new assertion demanded exactly `$` for input `{`.
  The unchanged SAX parser correctly reports `$[""]` (pending decoded object
  member), offset 2. Only consumer memory assertion/output changed: require an
  owned root-based location consistent with what(), plus exact Malformed code,
  parse operation, retained origin and known positive offset. The focused memory
  rerun passed 1/1 in 0.01 seconds. The two unaffected passing file cases were
  not repeated; aggregate latest evidence covers all three cases.
- **Owned memory/error boundary:** source Snapshot, string and streams are
  mutated/destroyed; both parsed values and an independent copy remain completely
  equal to the literal. Editing one result leaves another unchanged. A failed
  parse preserves the existing destination; copied Error retains category,
  operation, original origin, location/message and offset after inputs disappear.
- **Actual fresh processes:** round_trip executes author then fresh-verify.
  Save completes before the authored source is mutated/destroyed. A new process
  loads twice, compares every field/cell/empty/spawn against its literal expected
  data and checks independence. The CMake harness compares exact bytes around
  verification. `garden.map.json` is 509 bytes, SHA-256
  `ce8f5a705167332edf2b1791be94c314a59ba43647874b6d5ec3b37efbfe0308`.
- **Public checked preservation:** preservation seeds distinct valid A, captures
  exact bytes, then passes invalid B with one cell removed to save. It catches
  owned InvalidValue / encode / <snapshot> / $.layers[0].cells before staging,
  checks no cleanup/leftover/native error, exact A bytes and full reload. A separate
  verify-predecessor process compares literal A completely, retaining bytes.
  This new case is a validation failure, not a claim of another native write
  fault. The accepted 136 retain native I/O fault coverage. A is 514 bytes,
  SHA-256 `035e663fd38c8115cafe667aff8ba3249e67e9bc12175f051672c4e605e2ea94`.

Consumer fixtures live only in unique children under
`build/map-storage-u4-consumer-debug/consumer-checks/`. Their stdout/stderr,
commands and immediate exits remain in each child; `u4-logs/process-summary.json`
inventories six actual invocations: the initial memory check failure, its passing
rerun and four passing author/verify/preserve/fresh-predecessor invocations.
This initial exit 1 was a failed check, not a deliberate rejection fixture.
`published-files.json` retains paths, sizes, hashes and decoded identities.

## Source/link/header proof and limits

Engine's existing recursive .cpp discovery selects the moved production files.
The Engine-owned MapStorageCpu.cmake defines a source-tree STATIC proof target
and alias Cheryl::MapStorageCPU, exposing only Engine's public include root,
C++23 and CE_ENGINE_STATIC; pinned JSON stays PRIVATE. Existing Engine retains
its own normal target/visibility configuration. The proof target is an alternate
producer; linking both copies into one binary is not accepted. No installed
export/package manifest or shared infrastructure changed.

Both u4-logs directories retain `compile-inputs.json` and `link-inputs.txt`.
The independent executable/probes receive only Engine public includes, language
and static requirements, without test headers, internal FileOps or JSON include
paths. Its link edge is main.cpp plus the archive of the two real Engine .cpp
files and the ordinary C++ runtime. The CLI has the same production archive;
gtest archives appear only on the focused test runner. Tests-off compile commands
also describe the available excluded CLI target; it is not a consumer input.
There is no renderer/platform/Runtime/Workers target, Engine stub or native
startup dependency. Actual headless execution supports that inspected boundary.

Each of the three public headers compiled first and twice in its own translation
unit. Probes instantiate complete value/equality declarations and assert inherited
CE_ENGINE_STATIC; codec/file probes verify CE_API is empty for static consumption.
They use no prefix/umbrella header or consumer-defined visibility override.
These checks establish this source-tree static configuration, not shared ABI.

No unresolved prerequisite or resource blocker remains. All native work used the
shared one-job wrapper and retained sessions. The 4 GiB aggregate/3 GiB throttle,
2 GiB worker, no-swap and 4 GiB host admission floor remain unchanged. No exit 75,
subagent, external agent, download, system install, OOM test, push or sibling
integration occurred. Protected workspaces/caches were not used.

Confidence is high in the bounded extraction and executed CPU compatibility,
medium in provisional namespace/header/target naming. These are engineering
choices, not inferred personal preferences; reversal is limited to source-tree
names and branch-local consumers. Wire changes still require compatibility review.
The supported file profile remains Linux, controlled stable local tree,
regular-or-absent final entry, serialized writer and staging mode 0600. No fsync,
power-loss durability, hostile-race, network-filesystem, multiwriter or save-game
framework guarantee is added. Other platforms/toolchains, sanitizers/fuzz, full
Engine static/shared builds, cross-library exception/RTTI/CRT ABI, installed SDK,
relocation and device/GUI acceptance are unverified.

**Next concrete action:** coordinator reviews this U4 extraction and saved
source-tree evidence, then selects any later bounded Engine integration/ABI work.
This worker stops at the local checkpoint; no SDK packaging, additional consumer,
public guide or phase progression is started automatically.
