# Map-storage U7 checkpoint and next action

U7 starts clean at accepted U6 `fa267333a8bf30fe737d67544a46da8f3d4d2a14`.
Implementation and the accumulated CPU selection are complete: **171/171 cases
passed**, retaining all 162 accepted names and adding nine cases. The tests-off
source-only consumer passed 1/1 validation case and all three
static header probes built. The actual shared-Engine consumer passed 4/4 cases
and all three shared header probes built; export/import/DSO ownership checks
passed. The ready receipt reports these executed checks, not automatic coordinator
source acceptance.

## Caller and current contract

```cpp
try {
    CE::Maps::validate(snapshot);
    // Now choose whether to retain/publish this complete value.
} catch (const CE::Maps::Error& error) {
    auto owned = error;
}
```

Public `CE_API void validate(const Snapshot&)` in codec.h checks full semantics
AND current v1 encodability through 8 MiB, including the final LF. It counts
exact fragments from the same formatter used by encode, without materializing
output bytes. There is one semantic policy and no guessed size formula, JSON
DOM, validity cache, I/O, mutation, registry, asset lookup or retained reference.
This is not allocation-free/noexcept: bounded semantic/path/numeric bookkeeping
can allocate. Allocation/programming failures propagate distinctly.

Anticipated errors own the existing Error fields, with operation `validate`,
origin `<snapshot>`, encode-matching category/field/detail/precedence, and no
input byte offset, native I/O or cleanup context. Existing parse/encode context,
validation order, wire bytes and strict cheryl.map-fixture v1 limits remain.
Parse retains its original input/reconstruction budget behavior; a compact parsed
value can still need validation before assuming its canonical encoding fits.
Current keys are ASCII identifiers; non-ASCII/control/escaping characters reject
semantically, while escaped ASCII JSON input decodes to identical canonical keys.

Private patches retain exact canonical witness bytes, 65,536-cell/1,024-edit/
8 MiB bounds and deep ownership. Address/size admission precedes one candidate
copy, then public validate(candidate) supplies all semantic and output-size
checks. The repeated one-cell serialization probes and final output-buffer
serialization are gone. Semantic key errors can follow allocation and use
candidate cell order, not edit order. Useful `$.edits[i].tile` diagnostics retain
the original public cause at the actual candidate cell and operation validate;
encoded-limit failures retain `$.candidate`. A later address refusal may precede
an earlier invalid key. Early key-length guards are admission bounds, with no
private key grammar. Base/witness/edits/previous results survive failure. Empty
batches still require exact base encoding and return an independent copy.

MS-011 and MS-012 in both decision views record this change, confidence, tradeoffs
and reversible costs. These are engineering choices, not user taste. Confidence
is high in this Linux/GNU same-formatter mechanism and medium in
future public API adoption. Public symbol adoption has a source/ABI compatibility
cost; persisted bytes do not change. No recipe/digest/persisted-patch format or
sibling integration is selected.

## Actual completed CPU checks

Every configure/build/native command runs from this clone through
`/mnt/projects/cheryl-features/scripts/build --`, one job, using the unchanged
shared adaptive memory/no-swap/admission policy. Output/logs are clone-local;
existing incremental outputs and accepted prior logs remain intact.

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
  --output-junit u7-logs/results.xml
```

`build/map-storage-u1-debug/u7-logs/` retains configure/build/discovery/tests logs
and immediate `.exit` files, all 0. Build had no compiler warnings. Discovery
comparison with accepted U6 confirms all 162 names retained and exactly nine
added. XML confirms all 171 names executed with zero failures, skips or disabled
cases, 11.77 seconds total. `LastTest.log` and verbose `tests.log` retain complete
diagnostics; deliberate rejection fixtures are passing assertions, not failed
checks. `cpu-summary.json`, `compile-link-inputs.txt`, `discovery-comparison.json`
and `source-preservation.json` retain static source/selection/input evidence.
The semantic function is text-identical except its private rename; formatter
fragments/order are identical after indentation normalization against U6.

New direct checks cover a valid complete garden, semantic/product/key failures
and precedence, exact encoded sizes 8 MiB minus one/exactly/plus one, maximum keys,
escaped ASCII input and rejected UTF-8/quote/backslash/control/NUL keys, owning
error copies, input and previously held result preservation. The ninth case
checks patch address admission preceding candidate key checks. All accepted
codec/file/CLI and private patch/process regressions passed once; no redundant
broad rerun is planned.

## Actual public consumer and shared boundary checks

Reused the existing tests-off CPU and U5 shared-Engine outputs. Actual commands:

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake \
  -S projects/engine/tests/map-storage-consumer -B build/map-storage-u4-consumer-debug \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DMAP_STORAGE_BUILD_TESTS=OFF -DMAP_STORAGE_USE_SHARED_ENGINE=OFF \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u4-consumer-debug \
  --target map_storage_consumer map_storage_header_probes --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u4-consumer-debug \
  -R '^map_storage.consumer.validation$' --no-tests=error --output-on-failure \
  --verbose --parallel 1 --output-junit u7-logs/results.xml
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u5-shared-debug \
  --target map_storage_consumer map_storage_header_probes --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u5-shared-debug \
  --show-only=json-v1 -R '^map_storage.consumer\.'
ASAN_OPTIONS=detect_leaks=0 /mnt/projects/cheryl-features/scripts/build -- ctest \
  --test-dir build/map-storage-u5-shared-debug -R '^map_storage.consumer\.' \
  --no-tests=error --output-on-failure --verbose --parallel 1 \
  --output-junit u7-logs/results.xml
/mnt/projects/cheryl-features/scripts/build -- python3 \
  build/map-storage-u5-shared-debug/u7-logs/check-exports.py
```

- `build/map-storage-u4-consumer-debug/u7-logs/`: configure/build/tests and immediate
  exits all 0. The tests-off source-only target builds the real codec/file sources
  with C++23, public includes and inherited CE_ENGINE_STATIC, pinned JSON private;
  no gtest/Python/normal Engine dependency. Three header-first probes compiled,
  including codec.h first/repeated with a call to validate. The standalone public
  consumer's validation case passed (1/1, 0.02 s). `compile-inputs.json` and
  `link-inputs.txt` retain actual static usage requirements and CPU-only link edges.
- `build/map-storage-u5-shared-debug/u7-logs/`: build/discovery/tests/exports and
  immediate exits all 0. Build automatically regenerated the modified consumer
  CMake; no separate shared configure was issued. The existing scoped bootstrap
  retained the normal shared Engine producer and disabled unrelated campaigns,
  docs/SDK/startup/workers/native/graphics/UI/audio/apps. The two production map
  objects, actual DSO and same consumer/header probes rebuilt without warnings.
  All three shared probes compiled with import visibility, without consumer
  CE_ENGINE_STATIC/CE_BUILDING_ENGINE patches. Discovered and executed selection
  is exactly memory, round_trip, preservation and validation: 4/4, no skips,
  0.42 s total. Existing wire/value/file behavior and the new operation execute
  through the actual shared producer, not the alternate CPU archive.
- Shared tests alone used invocation-local `ASAN_OPTIONS=detect_leaks=0` for the
  accepted runner ptrace/LeakSanitizer limitation. Normal Debug Engine ASan/UBSan
  instrumentation/runtimes remain. No global environment/flags changed, and no
  unchanged known leak failure was repeated. This establishes no leak acceptance.
- Both validation invocations returned 0 and reported owning InvalidValue /
  operation `validate` / origin `<snapshot>` / `$.layers[1].cells[2]` after both original
  invalid snapshot and caught error disappeared. Literal complete garden values
  remained unchanged. Logs for actual native children are in tests-off
  `consumer-checks/validation-81a8f86a9c0df831/` and shared
  `consumer-checks/validation-91813bb1037eeeeb/`, with argv, stdout/stderr and exits.
  The shared four cases launch six native children in all; the old three behaviors
  retain their full literal fresh-process and predecessor checks.
- The wrapped export script sequentially runs nm/readelf/ldd, retaining exact argv,
  complete outputs and immediate per-tool exits. It confirms one normal Engine
  source compile entry per codec/adapter, CE_BUILDING_ENGINE on the producer,
  no private JSON/test includes or visibility patches on the consumer, and no
  alternate CPU archive/gtest in its link. The actual Engine dynamically exports
  `CE::Maps::validate(CE::Maps::Snapshot const&)` as FUNC GLOBAL DEFAULT; nm reports
  T and the consumer U. Dynamic/ldd evidence resolves libcheryl-engine.so.4 to
  this exact clone-build engine directory. `validate-exports.txt`, compile/link
  inputs and exports.log preserve the checks; all five symbol/loader tools exit 0.

All full diagnostics, XML results and LastTest snapshots remain in those
clone-local logs. Initial native queue waits were retained to completion, not
cancelled/relaunched. No exit 75, failed prerequisite, failed native check,
dependency retrieval, pin change, system install, subagent, protected write,
publication or sibling integration occurred. The only post-CPU-build test-source
change was an obsolete comment correction; executable code remained identical.

## Receipt, limits and next concrete action

`feature-development/queue/three-commits-20261024/MAP-STORAGE-U7.json` reports ready
and checks_passed=true only after the above checks. Its nonempty evidence paths
point to actual saved native logs; this is execution reporting, not coordinator
source acceptance, SDK acceptance or proof of another platform. No U7 blocker
remains. Next concrete action: verify this coherent checkpoint's committed
receipt and saved evidence. The finite queue controller alone selects the next
pinned U8 Engine patch unit after this run drains and its receipt/checkpoint gate
passes. Stop here; do not start it, another agent, or sibling integration during
U7. Publisher owns transport.

Prior U5 actual shared-Engine results/RTTI/header evidence and initial LeakSanitizer
ptrace failures remain preserved. U6's full 162-case record is archived as
u7-logs/accepted-u6-next.md and in its accepted commit. U7 CPU Debug checks are
unsanitized. Full static Engine, non-Linux, installed SDK/other toolchains and
leak checking remain unverified. Allocation/programming exceptions propagate by
source inspection; no OOM/allocation-exhaustion test was attempted. Counting
validation still traverses all bounded values and allocates small bookkeeping;
it promises neither allocation-free nor constant-time validation. Linux file
atomicity remains the accepted caller-controlled serialized-directory profile,
without durability/fsync, hostile races, network-FS or multiwriter guarantees.
The full patch witness may cost 8 MiB and is not compact persistence. No new
format, public patch/recipe framework, shared backend or protected change follows.
